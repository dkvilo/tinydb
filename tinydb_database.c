#include "tinydb_log.h"
#include "tinydb_hash.h"
#include "tinydb_database.h"

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


