#ifndef __TINY_DB_USER_MANAGER_H
#define __TINY_DB_USER_MANAGER_H

#include "tinydb_user.h"
#include <stdint.h>

typedef struct RuntimeContext RuntimeContext;

/**
 * @return 0 on success, -1 on failure
 */
int32_t
Create_User(RuntimeContext* ctx, const char* username, const char* password);

/**
 * @return 0 on success, -1 on failure
 */
int32_t
Authenticate_User(RuntimeContext* ctx,
                  const char* username,
                  const char* password);

/**
 * @return 0 on success, -1 on failure
 */
int32_t
Delete_User(RuntimeContext* ctx, const char* username);

void
Print_User_Manager_State(RuntimeContext* ctx);

#endif // __TINY_DB_USER_MANAGER_H