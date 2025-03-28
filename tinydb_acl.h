#ifndef __TINY_DB_ACL
#define __TINY_DB_ACL

#include "tinydb_datatype.h"

typedef enum DB_ACCESS_LEVEL
{
  DB_READ = 1 << 0,
  DB_WRITE = 1 << 1,
  DB_DELETE = 1 << 2
} DB_ACCESS_LEVEL;

typedef struct DB_Access
{
  EntryID database;
  DB_ACCESS_LEVEL acl;
} DB_Access;

#endif // __TINY_DB_ACL
