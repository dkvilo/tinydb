#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tinydb_database_entry_destructor.h"
#include "tinydb_log.h"
#include "tinydb_ttl.h"

/**
 * Set_TTL - Sets a time-to-live for a key
 */
int32_t
Set_TTL(Database* db, const char* key, int64_t seconds)
{
  if (db == NULL || key == NULL) {
    DB_Log(DB_LOG_ERROR, "NULL database or key in Set_TTL");
    return -1;
  }

  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_rwlock_wrlock(&shard->rwlock);
  DatabaseEntry* entry = HM_Get(shard->entries, key);

  if (entry == NULL) {
    pthread_rwlock_unlock(&shard->rwlock);
    DB_Log(DB_LOG_WARNING, "Key '%s' not found for TTL", key);
    return -1;
  }

  if (seconds <= 0) {
    entry->has_ttl = 0;
    entry->expiry = 0;
  } else {
    entry->has_ttl = 1;
    entry->expiry = time(NULL) + seconds;
  }

  pthread_rwlock_unlock(&shard->rwlock);
  return 0;
}

/**
 * Get_TTL - Gets the remaining time-to-live for a key
 * @return Remaining TTL in seconds, -1 if key doesn't exist, -2 if key has no TTL
 */
int64_t
Get_TTL(Database* db, const char* key)
{
  if (db == NULL || key == NULL) {
    DB_Log(DB_LOG_ERROR, "NULL database or key in Get_TTL");
    return -1;
  }

  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_rwlock_rdlock(&shard->rwlock);
  DatabaseEntry* entry = HM_Get(shard->entries, key);

  if (entry == NULL) {
    pthread_rwlock_unlock(&shard->rwlock);
    return -1; // key doesn't exist
  }

  if (!entry->has_ttl) {
    pthread_rwlock_unlock(&shard->rwlock);
    return -2; // no ttl set
  }

  time_t now = time(NULL);
  int64_t remaining = entry->expiry - now;

  pthread_rwlock_unlock(&shard->rwlock);

  // if already expired but not yet cleaned up
  if (remaining < 0) {
    return 0;
  }

  return remaining;
}

/**
 * Check_Expiry - Checks if a key has expired
 */
int
Check_Expiry(DatabaseEntry* entry)
{
  if (entry == NULL) {
    return 0;
  }

  if (!entry->has_ttl) {
    return 0;
  }

  time_t now = time(NULL);
  return (now >= entry->expiry) ? 1 : 0;
}

/**
 * Cleanup_Expired_Keys - Removes all expired keys from a database
 */
int32_t
Cleanup_Expired_Keys(Database* db)
{
  if (db == NULL) {
    DB_Log(DB_LOG_ERROR, "NULL database in Cleanup_Expired_Keys");
    return -1;
  }

  int32_t removed_count = 0;
  time_t now = time(NULL);

  for (int j = 0; j < NUM_SHARDS; j++) {
    DatabaseShard* shard = &db->shards[j];
    pthread_rwlock_wrlock(&shard->rwlock);

    // create a list of keys to remove
    char** keys_to_remove = NULL;
    int num_keys_to_remove = 0;

    // first, identify expired keys
    for (size_t i = 0; i < shard->entries->capacity; i++) {
      HashEntry* hash_entry = &shard->entries->entries[i];
      if (hash_entry->is_occupied && !hash_entry->is_deleted) {
        DatabaseEntry* entry = (DatabaseEntry*)hash_entry->value;

        if (entry->has_ttl && now >= entry->expiry) {
          // add to list of keys to remove
          keys_to_remove =
            realloc(keys_to_remove, (num_keys_to_remove + 1) * sizeof(char*));
          if (keys_to_remove == NULL) {
            DB_Log(DB_LOG_ERROR,
                   "Memory allocation failed in Cleanup_Expired_Keys");
            pthread_rwlock_unlock(&shard->rwlock);
            return -1;
          }
          keys_to_remove[num_keys_to_remove] = strdup(entry->key);
          num_keys_to_remove++;
        }
      }
    }

    // remove expired keys
    for (int i = 0; i < num_keys_to_remove; i++) {
      DatabaseEntry* entry = HM_Get(shard->entries, keys_to_remove[i]);
      if (entry != NULL) {
        DB_Log(DB_LOG_INFO, "Removing expired key: %s", keys_to_remove[i]);
        HM_Remove(shard->entries, keys_to_remove[i]);
        atomic_fetch_sub(&shard->num_entries, 1);
        removed_count++;
      }
      free(keys_to_remove[i]);
    }

    free(keys_to_remove);
    pthread_rwlock_unlock(&shard->rwlock);
  }

  return removed_count;
}

/**
 * Background TTL cleanup thread function
 */
void*
ttl_cleanup_thread(void* arg)
{
  RuntimeContext* ctx = (RuntimeContext*)arg;

  while (atomic_load(&ctx->ttl_cleanup_config.running)) {
    sleep(ctx->ttl_cleanup_config.interval_seconds);
    if (!atomic_load(&ctx->ttl_cleanup_config.running)) {
      break;
    }

    DB_Log(DB_LOG_INFO, "Running automatic TTL cleanup");
    int total_removed = 0;

    // clean up expired keys in all databases
    for (int i = 0; i < ctx->db_manager.num_databases; i++) {
      Database* db = &ctx->db_manager.databases[i];
      int removed = Cleanup_Expired_Keys(db);
      if (removed > 0) {
        total_removed += removed;
        DB_Log(DB_LOG_INFO,
               "Removed %d expired keys from database %s",
               removed,
               db->name ? db->name : "unnamed");
      }
    }

    ctx->ttl_cleanup_config.last_cleanup_time = time(NULL);

    if (total_removed > 0) {
      DB_Log(DB_LOG_INFO,
             "TTL cleanup completed: removed %d expired keys",
             total_removed);
    } else {
      DB_Log(DB_LOG_INFO, "TTL cleanup completed: no expired keys found");
    }
  }

  DB_Log(DB_LOG_INFO, "Background TTL cleanup thread exiting");
  return NULL;
}

/**
 * Start_TTL_Cleanup - Starts a background thread that periodically cleans up
 * expired keys
 */
int32_t
Start_TTL_Cleanup(RuntimeContext* ctx, int interval_seconds)
{
  if (ctx == NULL || interval_seconds <= 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to Start_TTL_Cleanup");
    return -1;
  }

  if (atomic_load(&ctx->ttl_cleanup_config.running)) {
    DB_Log(DB_LOG_WARNING, "Background TTL cleanup thread is already running");
    return -1;
  }

  ctx->ttl_cleanup_config.enabled = 1;
  ctx->ttl_cleanup_config.interval_seconds = interval_seconds;
  ctx->ttl_cleanup_config.last_cleanup_time = time(NULL);

  atomic_store(&ctx->ttl_cleanup_config.running, 1);

  if (pthread_create(&ctx->ttl_cleanup_config.cleanup_thread,
                     NULL,
                     ttl_cleanup_thread,
                     ctx) != 0) {
    DB_Log(DB_LOG_ERROR, "Failed to create background TTL cleanup thread");
    atomic_store(&ctx->ttl_cleanup_config.running, 0);
    return -1;
  }

  DB_Log(DB_LOG_INFO,
         "Background TTL cleanup thread started with interval %d seconds",
         interval_seconds);

  return 0;
}

/**
 * Stop_TTL_Cleanup - Stops the background TTL cleanup thread
 */
int32_t
Stop_TTL_Cleanup(RuntimeContext* ctx)
{
  if (ctx == NULL) {
    DB_Log(DB_LOG_ERROR, "NULL context in Stop_TTL_Cleanup");
    return -1;
  }

  if (!atomic_load(&ctx->ttl_cleanup_config.running)) {
    DB_Log(DB_LOG_WARNING, "No background TTL cleanup thread is running");
    return -1;
  }

  atomic_store(&ctx->ttl_cleanup_config.running, 0);

  if (pthread_join(ctx->ttl_cleanup_config.cleanup_thread, NULL) != 0) {
    DB_Log(DB_LOG_ERROR, "Failed to join background TTL cleanup thread");
    return -1;
  }

  ctx->ttl_cleanup_config.enabled = 0;
  DB_Log(DB_LOG_INFO, "Background TTL cleanup thread stopped");

  return 0;
}

/**
 * Set_TTL_Cleanup_Interval - Changes the interval between TTL cleanup runs
 */
int32_t
Set_TTL_Cleanup_Interval(RuntimeContext* ctx, int interval_seconds)
{
  if (ctx == NULL || interval_seconds <= 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to Set_TTL_Cleanup_Interval");
    return -1;
  }

  ctx->ttl_cleanup_config.interval_seconds = interval_seconds;
  DB_Log(DB_LOG_INFO,
         "TTL cleanup interval updated to %d seconds",
         interval_seconds);

  return 0;
}