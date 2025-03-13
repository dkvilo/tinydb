#include "tinydb_atomic_proc.h"
#include "tinydb_database_entry_destructor.h"
#include "tinydb_log.h"
#include "tinydb_ttl.h"

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
  new_entry->has_ttl = 0; // Default: no TTL
  new_entry->expiry = 0;  // Default: no expiry time

  int8_t state = HM_Put(shard->entries, new_entry->key, new_entry);

  if (state == HM_ACTION_FAILED) {
    free(new_entry->key);
    free(new_entry);
  } else if (state == HM_ACTION_ADDED) {
    atomic_fetch_add(&shard->num_entries, 1);
  }
}

DatabaseEntry
DB_Atomic_Get(Database* db, const char* key)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_rwlock_rdlock(&shard->rwlock);
  DatabaseEntry* entry = HM_Get(shard->entries, key);

  if (entry != NULL) {
    if (Check_Expiry(entry)) {
      pthread_rwlock_unlock(&shard->rwlock);
      pthread_rwlock_wrlock(&shard->rwlock);

      entry = HM_Get(shard->entries, key);
      if (entry != NULL && Check_Expiry(entry)) {
        DB_Log(DB_LOG_INFO, "Auto-removing expired key: %s", key);
        HM_Remove(shard->entries, key);
        atomic_fetch_sub(&shard->num_entries, 1);
      }

      pthread_rwlock_unlock(&shard->rwlock);
      return (DatabaseEntry){ .type = DB_ENTRY_STRING,
                              .value = { .string = { .value = "null" } },
                              .has_ttl = 0,
                              .expiry = 0 };
    }

    // key exists and has not expired
    DatabaseEntry result = *entry;
    pthread_rwlock_unlock(&shard->rwlock);
    return result;
  }

  pthread_rwlock_unlock(&shard->rwlock);
  return (DatabaseEntry){ .type = DB_ENTRY_STRING,
                          .value = { .string = { .value = "null" } },
                          .has_ttl = 0,
                          .expiry = 0 };
}

int64_t
DB_Atomic_Incr(Database* db, const char* key)
{
  int32_t shard_id = Pick_Shard(key);
  DatabaseShard* shard = &db->shards[shard_id];

  pthread_rwlock_wrlock(&shard->rwlock);
  DatabaseEntry* entry = HM_Get(shard->entries, key);

  if (entry == NULL) {
    DB_Value value = { .number = { .value = 1 } };
    DB_Atomic_Store(db, key, value, DB_ENTRY_NUMBER);

    pthread_rwlock_unlock(&shard->rwlock);
    return 1;
  }

  if (Check_Expiry(entry)) {
    DB_Log(DB_LOG_INFO, "Auto-removing expired key during INCR: %s", key);

    HM_Remove(shard->entries, key);
    atomic_fetch_sub(&shard->num_entries, 1);

    DB_Value value = { .number = { .value = 1 } };
    DB_Atomic_Store(db, key, value, DB_ENTRY_NUMBER);

    pthread_rwlock_unlock(&shard->rwlock);
    return 1;
  }

  if (entry->type != DB_ENTRY_NUMBER) {
    pthread_rwlock_unlock(&shard->rwlock);
    DB_Log(DB_LOG_WARNING, "INCR Attempt to increment a non-integer value");
    return -1;
  }

  int64_t new_value = atomic_fetch_add(&(entry->value.number.value), 1) + 1;

  pthread_rwlock_unlock(&shard->rwlock);

  return new_value;
}
