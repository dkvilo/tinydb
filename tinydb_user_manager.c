#include "tinydb_user_manager.h"
#include "tinydb_context.h"
#include "tinydb_hash.h"
#include "tinydb_log.h"

#include <stdlib.h>
#include <string.h>

/**
 * @return 0 on success, -1 on failure
 */
int32_t Create_User(RuntimeContext* ctx, const char* username, const char* password) {
  if (!ctx || !username || !password) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to Create_User");
    return -1;
  }
  
  for (int32_t i = 0; i < ctx->user_manager.num_users; i++) {
    if (ctx->user_manager.users[i].name != NULL && 
        strcmp(ctx->user_manager.users[i].name, username) == 0) {
      DB_Log(DB_LOG_ERROR, "User %s already exists", username);
      return -1;
    }
  }
  
  size_t new_size = (ctx->user_manager.num_users + 1) * sizeof(DB_User);
  DB_User* new_users = (DB_User*)realloc(ctx->user_manager.users, new_size);
  if (!new_users) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for new user");
    return -1;
  }
  
  ctx->user_manager.users = new_users;

  DB_User* new_user = &ctx->user_manager.users[ctx->user_manager.num_users];
  memset(new_user, 0, sizeof(DB_User));
  
  new_user->ID = ctx->user_manager.num_users;
  new_user->name = strdup(username);
  if (!new_user->name) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for username");
    return -1;
  }
  
  SHA256(new_user->password, password, strlen(password));

  new_user->access = (DB_Access*)malloc(sizeof(DB_Access));
  if (!new_user->access) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for user access");
    free(new_user->name);
    return -1;
  }
  
  new_user->access[0].database = 0;
  new_user->access[0].acl = DB_READ;
  
  ctx->user_manager.num_users++;
  return 0;
}

/**
 * @return 0 on success, -1 on failure
 */
int32_t Authenticate_User(RuntimeContext* ctx, const char* username, const char* password) {
  if (!ctx || !username || !password) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to Authenticate_User");
    return -1;
  }
  
  for (int32_t i = 0; i < ctx->user_manager.num_users; i++) {
    if (ctx->user_manager.users[i].name != NULL && 
        strcmp(ctx->user_manager.users[i].name, username) == 0) {
      
      uint8_t hashed_password[32];
      SHA256(hashed_password, password, strlen(password));
      
      if (memcmp(hashed_password, ctx->user_manager.users[i].password, 32) == 0) {
        ctx->Active.user = &ctx->user_manager.users[i];
        return 0;
      } else {
        return -1;
      }
    }
  }

  return -1;
}

/**
 * @return 0 on success, -1 on failure
 */
int32_t Delete_User(RuntimeContext* ctx, const char* username) {
  if (!ctx || !username) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to Delete_User");
    return -1;
  }
  
  // we don't want to delete the default user
  if (strcmp(username, "default") == 0) {
    DB_Log(DB_LOG_ERROR, "Cannot delete the default user");
    return -1;
  }

  int32_t user_index = -1;
  for (int32_t i = 0; i < ctx->user_manager.num_users; i++) {
    if (ctx->user_manager.users[i].name != NULL && 
        strcmp(ctx->user_manager.users[i].name, username) == 0) {
      user_index = i;
      break;
    }
  }
  
  if (user_index == -1) {
    DB_Log(DB_LOG_ERROR, "User %s not found", username);
    return -1;
  }
  
  free(ctx->user_manager.users[user_index].name);
  free(ctx->user_manager.users[user_index].access);
  
  for (int32_t j = user_index; j < ctx->user_manager.num_users - 1; j++) {
    ctx->user_manager.users[j] = ctx->user_manager.users[j + 1];
  }
  
  if (ctx->user_manager.num_users > 1) {
    size_t new_size = (ctx->user_manager.num_users - 1) * sizeof(DB_User);
    DB_User* new_users = (DB_User*)realloc(ctx->user_manager.users, new_size);
    if (new_users) {
      ctx->user_manager.users = new_users;
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to reallocate memory for users, but user was deleted");
    }
  }
  
  ctx->user_manager.num_users--;
  if (ctx->Active.user == NULL || 
      (ctx->Active.user->name != NULL && strcmp(ctx->Active.user->name, username) == 0)) {
    ctx->Active.user = &ctx->user_manager.users[0]; // default user
  }
  
  return 0;
}

/**
 * Print the user manager state for debugging
 */
void Print_User_Manager_State(RuntimeContext* ctx) {
  if (!ctx) {
    DB_Log(DB_LOG_ERROR, "Invalid context in Print_User_Manager_State");
    return;
  }
  
  DB_Log(DB_LOG_INFO, "UserManager:");
  DB_Log(DB_LOG_INFO, "  Number of users: %d", ctx->user_manager.num_users);
  
  for (int32_t i = 0; i < ctx->user_manager.num_users; i++) {
    DB_Log(DB_LOG_INFO, "  User %d:", i);
    DB_Log(DB_LOG_INFO, "    ID: %d", ctx->user_manager.users[i].ID);
    
    if (ctx->user_manager.users[i].name != NULL) {
      DB_Log(DB_LOG_INFO, "    Name: %s", ctx->user_manager.users[i].name);
    } else {
      DB_Log(DB_LOG_INFO, "    Name: NULL");
    }
    
    if (ctx->user_manager.users[i].access != NULL) {
      DB_Log(DB_LOG_INFO, "    Access - Database: %d, ACL: %d", 
             ctx->user_manager.users[i].access[0].database,
             ctx->user_manager.users[i].access[0].acl);
    } else {
      DB_Log(DB_LOG_INFO, "    Access: NULL");
    }
  }
} 