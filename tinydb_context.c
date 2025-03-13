#include "tinydb_context.h"
#include "tinydb_hash.h"
#include "tinydb_log.h"
#include "tinydb_pubsub.h"
#include "tinydb_snapshot.h"
#include "tinydb_ttl.h"
#include <time.h>

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

  context->snapshot_config.enabled = ENABLE_BACKGROUND_SNAPSHOTS;
  context->snapshot_config.interval_seconds = DEFAULT_SNAPSHOT_INTERVAL;
  context->snapshot_config.snapshot_filename = strdup(DEFAULT_SNAPSHOT_NAME);
  atomic_init(&context->snapshot_config.running, 0);
  context->snapshot_config.last_snapshot_time = time(NULL);
  
  // TTL cleanup configuration
  context->ttl_cleanup_config.enabled = ENABLE_TTL_CLEANUP;
  context->ttl_cleanup_config.interval_seconds = DEFAULT_TTL_CLEANUP_INTERVAL;
  atomic_init(&context->ttl_cleanup_config.running, 0);
  context->ttl_cleanup_config.last_cleanup_time = time(NULL);

  context->pubsub_system = Create_PubSub_System();

  if (snapshot_file != NULL) {
    if (Import_Snapshot(context, snapshot_file) == 0) {
      if (context->snapshot_config.enabled) {
        Start_Background_Snapshot(context,
                                  context->snapshot_config.interval_seconds,
                                  context->snapshot_config.snapshot_filename);
      }

      if (context->ttl_cleanup_config.enabled) {
        Start_TTL_Cleanup(context, context->ttl_cleanup_config.interval_seconds);
      }
      
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
    free(context->snapshot_config.snapshot_filename);
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

  if (context->snapshot_config.enabled) {
    Start_Background_Snapshot(context,
                              context->snapshot_config.interval_seconds,
                              context->snapshot_config.snapshot_filename);
  }
  
  // Start TTL cleanup if enabled
  if (context->ttl_cleanup_config.enabled) {
    Start_TTL_Cleanup(context, context->ttl_cleanup_config.interval_seconds);
  }

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
Free_Context(RuntimeContext* context)
{
  if (context == NULL)
    return;

  if (atomic_load(&context->snapshot_config.running)) {
    Stop_Background_Snapshot(context);
  }
  
  if (atomic_load(&context->ttl_cleanup_config.running)) {
    Stop_TTL_Cleanup(context);
  }

  if (context->snapshot_config.snapshot_filename != NULL) {
    free(context->snapshot_config.snapshot_filename);
  }

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
