#ifndef __TINY_DB_DATABASE
#define __TINY_DB_DATABASE

#include "config.h"
#include "tinydb_acl.h"
#include "tinydb_user.h"
#include "tinydb_datatype.h"
#include "tinydb_hashmap.h"

typedef struct
{
  HashMap* entries;
  atomic_size_t num_entries;
  pthread_rwlock_t rwlock;
} DatabaseShard;

typedef struct
{
  EntryID ID;
  char* name;
  DatabaseShard shards[NUM_SHARDS];
} Database;

typedef struct DatabaseManager
{
  Database* databases;
  int32_t num_databases;
} DatabaseManager;

int32_t
Pick_Shard(const char* key);

void
Initialize_Database(Database* db);

#endif // __TINY_DB_DATABASE