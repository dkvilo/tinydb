#ifndef __TINY_DB_USER
#define __TINY_DB_USER

#include <stdint.h>

#include "tinydb_acl.h"
#include "tinydb_datatype.h"

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

#endif // __TINY_DB_USER