#include "tinydb.h"
#include "tinydb_hash.h"
#include "tinydb_log.h"
#include "tinydb_snapshot.h"

RuntimeContext*
Initialize_Context(int32_t num_databases, const char* snapshot_file)
{
  RuntimeContext* context = (RuntimeContext*)malloc(sizeof(RuntimeContext));
  if (context == NULL) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for RuntimeContext.");
    return NULL;
  }

  context->Active.db = NULL;
  context->Active.user = NULL;

  if (snapshot_file != NULL) {
    if (Import_Snapshot(context, snapshot_file) == 0) {
      return context;
    } else {
      DB_Log(DB_LOG_WARNING,
             "Failed to import snapshot. Initializing empty context.");
    }
  }

  // normal initialization if there was no snapshot or failed to import
  context->db_manager.num_databases = num_databases;
  context->db_manager.databases =
    (Database*)malloc(sizeof(Database) * num_databases);
  if (context->db_manager.databases == NULL) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for databases");
    free(context);
    return NULL;
  }

  // initializing each db
  for (int32_t i = 0; i < num_databases; ++i) {
    Database* db = &context->db_manager.databases[i];
    db->ID = i;
    db->name = NULL;
    Initialize_Database(db);
  }

  context->user_manager.users = NULL;
  context->user_manager.num_users = 0;

  return context;
}

void
Cleanup_Partial_Context(RuntimeContext* context, int32_t num_initialized_dbs)
{
  for (int32_t i = 0; i < num_initialized_dbs; ++i) {
    Database* db = &context->db_manager.databases[i];
    free(db->name);
    for (int j = 0; j < NUM_SHARDS; ++j) {
      HM_Destroy(db->shards[j].entries);
      pthread_rwlock_destroy(&db->shards[j].rwlock);
    }
  }
  free(context->db_manager.databases);
  free(context);
}

void
Initialize_Database(Database* db)
{
  for (int i = 0; i < NUM_SHARDS; i++) {
    db->shards[i].entries = HM_Create();
    if (db->shards[i].entries == NULL) {
      DB_Log(DB_LOG_ERROR, "Failed to create hash map for shard %d", i);
      for (int j = 0; j < i; j++) {
        HM_Destroy(db->shards[j].entries);
        pthread_rwlock_destroy(&db->shards[j].rwlock);
      }
      return;
    }
    db->shards[i].num_entries = 0;

    if (pthread_rwlock_init(&db->shards[i].rwlock, NULL) != 0) {
      DB_Log(DB_LOG_ERROR, "Error initializing rwlock for shard %d", i);
      for (int j = 0; j <= i; j++) {
        HM_Destroy(db->shards[j].entries);
        if (j < i)
          pthread_rwlock_destroy(&db->shards[j].rwlock);
      }
      return;
    }
  }
}

void
Free_Context(RuntimeContext* context)
{
  if (context == NULL)
    return;

  for (int32_t i = 0; i < context->db_manager.num_databases; ++i) {
    Database* db = &context->db_manager.databases[i];
    free(db->name);

    for (int j = 0; j < NUM_SHARDS; ++j) {
      DatabaseShard* shard = &db->shards[j];
      HM_Destroy(shard->entries);
      pthread_rwlock_destroy(&shard->rwlock);
    }
  }
  free(context->db_manager.databases);

  for (int32_t i = 0; i < context->user_manager.num_users; ++i) {
    free(context->user_manager.users[i].name);
    free(context->user_manager.users[i].access);
  }
  free(context->user_manager.users);

  free(context);
}

int32_t
Pick_Shard(const char* key)
{
  uint64_t hash = DJB2_Hash_String(key);
  return hash & (NUM_SHARDS - 1);
}

void
DB_Atomic_Store(Database* db,
                const char* key,
                DB_Value value,
                DB_ENTRY_TYPE type)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  DatabaseEntry* new_entry = (DatabaseEntry*)malloc(sizeof(DatabaseEntry));
  new_entry->key = strdup(key);
  new_entry->value = value;
  new_entry->type = type;
  int8_t state = HM_Put(shard->entries, new_entry->key, new_entry);

  if (state == HM_ACTION_FAILED) {
    free(new_entry->key);
    free(new_entry);
  } else if (state == HM_ACTION_ADDED) {
    atomic_fetch_add(&shard->num_entries, 1);
  }
}

DB_Value
DB_Atomic_Get(Database* db, const char* key, DB_ENTRY_TYPE type)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];
  DatabaseEntry* entry = HM_Get(shard->entries, key);

  DB_Value result;
  if (entry != NULL) {
    result = entry->value;
  } else {
    memset(&result, 0, sizeof(DB_Value));
  }

  return result;
}
