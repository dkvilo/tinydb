#ifndef __TINY_DB_USER
#define __TINY_DB_USER

#include <stdint.h>

#include "tinydb_acl.h"
#include "tinydb_datatype.h"

typedef struct RuntimeContext RuntimeContext;

typedef struct DB_User
{
  EntryID ID;

  char* name;
  uint8_t password[32];

  DB_Access* access;
} DB_User;

typedef struct UserManager
{
  DB_User* users;
  int32_t num_users;
} UserManager;

int32_t
Create_User(RuntimeContext* ctx, const char* username, const char* password);

int32_t
Authenticate_User(RuntimeContext* ctx,
                  const char* username,
                  const char* password);

int32_t
Delete_User(RuntimeContext* ctx, const char* username);

#endif // __TINY_DB_USER