#include "tinydb.h"
#include "tinydb_hash.h"

#define STB_DS_IMPLEMENTATION
#include "external/stb_ds.h"

RuntimeContext*
Initialize_Context(int32_t num_databases)
{
  RuntimeContext* context = (RuntimeContext*)malloc(sizeof(RuntimeContext));
  if (context == NULL) {
    fprintf(stderr, "Failed to allocate memory for RuntimeContext\n");
    return NULL;
  }

  // Initialize DatabaseManager
  context->db_manager.num_databases = num_databases;
  context->db_manager.databases =
    (Database*)malloc(sizeof(Database) * num_databases);
  if (context->db_manager.databases == NULL) {
    fprintf(stderr, "Failed to allocate memory for databases\n");
    free(context);
    return NULL;
  }

  // Initialize each Database
  for (int32_t i = 0; i < num_databases; ++i) {
    Database* db = &context->db_manager.databases[i];
    db->ID = i;
    db->name = NULL;

    // Initialize shards
    Initialize_Database(db);
  }

  // Initialize UserManager (basic initialization, expand as needed)
  context->user_manager.users = NULL;
  context->user_manager.num_users = 0;

  // Initialize Active context
  context->Active.db = NULL;
  context->Active.user = NULL;

  return context;
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
      for (uint64_t k = 0; k < shard->num_entries; ++k) {
        free(shard->entries[k].key);
        if (shard->entries[k].type == DB_ENTRY_STRING) {
          free(shard->entries[k].value.string.value);
        }

        // note (David): we need to do clean up for DB_ENTRY_OBJECT as well.
        // todo
      }
      free(shard->entries);
      pthread_mutex_destroy(&shard->mutex);
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
Initialize_Database(Database* db)
{
  for (int i = 0; i < NUM_SHARDS; i++) {
    db->shards[i].entries = NULL;
    db->shards[i].num_entries = 0;
    pthread_mutex_init(&db->shards[i].mutex, NULL);
  }
}

void
DB_Atomic_Store(Database* db,
                const char* key,
                DB_Value value,
                DB_ENTRY_TYPE type)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_mutex_lock(&shard->mutex);
  DatabaseEntry* existing_entry = hmgetp_null(shard->entries, key);
  if (existing_entry != NULL) {
    atomic_store(&existing_entry->value, value);
    existing_entry->type = type;
  } else {
    DatabaseEntry new_entry = { strdup(key), value, type };
    shput(shard->entries, new_entry.key, value);
    shard->num_entries++;
  }
  pthread_mutex_unlock(&shard->mutex);
}

DB_Value
DB_Atomic_Get(Database* db, const char* key, DB_ENTRY_TYPE* type)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_mutex_lock(&shard->mutex);
  /*
  DatabaseEntry *entry = hmgetp_null(shard->entries, key);
  DB_Value result;

  if (entry != NULL) {
    result = atomic_load(&entry->value);
  } else { // not found, default type for now will be Number
    memset(&result, 0, sizeof(DB_Value));
  }
  */

  DB_Value result = shget(shard->entries, key);
  pthread_mutex_unlock(&shard->mutex);
  return result;
}
