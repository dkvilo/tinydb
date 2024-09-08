#ifndef __TINY_DB
#define __TINY_DB

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_hashmap.h"
#include "config.h"

typedef uint64_t EntryID;

typedef enum DB_ACCESS_LEVEL
{
  DB_READ = 1 << 1,
  DB_WRITE = 2 << 1,
  DB_DELETE = 3 << 1
} DB_ACCESS_LEVEL;

typedef struct DB_Access
{
  EntryID database;
  DB_ACCESS_LEVEL acl;
} DB_Access;

typedef struct DB_User
{
  EntryID ID;

  char* name;
  char password[32];

  DB_Access* access;
} DB_User;

typedef struct UserManager
{
  DB_User* users;
  int32_t num_users;
} UserManager;

typedef enum
{
  DB_ENTRY_STRING,
  DB_ENTRY_NUMBER,
  DB_ENTRY_OBJECT,
} DB_ENTRY_TYPE;

typedef struct DB_Number
{
  int64_t value;
} DB_Number;

typedef struct DB_String
{
  char* value;
} DB_String;

typedef struct DB_Object
{
  void* value;
} DB_Object;

typedef union
{
  DB_Number number;
  DB_String string;
  DB_Object object;
} DB_Value;

typedef struct
{
  char* key;
  DB_Value value;
  DB_ENTRY_TYPE type;
} DatabaseEntry;

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

typedef struct RuntimeContext
{
  DatabaseManager db_manager;
  UserManager user_manager;
  struct
  {
    Database* db;
    DB_User* user;
  } Active;
} RuntimeContext;

RuntimeContext*
Initialize_Context(int32_t num_databases, const char* snapshot_file);

void
Cleanup_Partial_Context(RuntimeContext* context, int32_t num_initialized_dbs);

void
Free_Context(RuntimeContext* context);

int32_t
Pick_Shard(const char* key);

void
DB_Atomic_Store(Database* db,
                const char* key,
                DB_Value value,
                DB_ENTRY_TYPE type);

DB_Value
DB_Atomic_Get(Database* db, const char* key, DB_ENTRY_TYPE type);

void
Initialize_Database(Database* db);

#endif // __TINY_DB