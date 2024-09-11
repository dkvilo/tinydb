#ifndef __TINY_DB
#define __TINY_DB

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tinydb_datatype.h"
#include "tinydb_user.h"
#include "tinydb_database.h"

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

#endif // __TINY_DB