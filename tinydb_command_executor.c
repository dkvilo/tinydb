#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tinydb_atomic_proc.h"
#include "tinydb_command_executor.h"
#include "tinydb_context.h"
#include "tinydb_database.h"
#include "tinydb_datatype.h"
#include "tinydb_event_tcp_server.h"
#include "tinydb_list.h"
#include "tinydb_log.h"
#include "tinydb_pubsub.h"
#include "tinydb_snapshot.h"
#include "tinydb_ttl.h"
#include "tinydb_user_manager.h"
#include "tinydb_webhook.h"

#define RESPONSE_OK "Ok\n"
#define RESPONSE_FAILED "FAILED\n"
#define RESPONSE_KEY_NOT_FOUND "null\n"
#define RESPONSE_USAGE_SET "Usage: set <key> <value>\n"
#define RESPONSE_USAGE_GET "Usage: get <key>\n"
#define RESPONSE_USAGE_INCR "Usage: incr <key>\n"
#define RESPONSE_USAGE_APPEND "Usage: append <key> <value>\n"
#define RESPONSE_USAGE_STRLEN "Usage: strlen <key>\n"
#define RESPONSE_USAGE_EXPORT "Usage: export snapshot.bin\n"
#define RESPONSE_USAGE_RPUSH "Usage: rpush <key> <value>\n"
#define RESPONSE_USAGE_LPUSH "Usage: lpush <key> <value>\n"
#define RESPONSE_USAGE_RPOP "Usage: rpop <key>\n"
#define RESPONSE_USAGE_LPOP "Usage: lpop <key>\n"
#define RESPONSE_USAGE_LLEN "Usage: llen <key>\n"
#define RESPONSE_USAGE_LRANGE "Usage: lrange <key> <min> <max>\n"
#define RESPONSE_USAGE_CREATE_USER "Usage: create_user <username> <password>\n"
#define RESPONSE_USAGE_AUTH "Usage: auth <username> <password>\n"
#define RESPONSE_USAGE_DELETE_USER "Usage: delete_user <username>\n"
#define RESPONSE_USAGE_SNAPSHOT_START "Usage: snapshot_start <interval_seconds> <filename>\n"
#define RESPONSE_USAGE_SNAPSHOT_STOP "Usage: snapshot_stop\n"
#define RESPONSE_USAGE_SNAPSHOT_INTERVAL "Usage: snapshot_interval <interval_seconds>\n"
#define RESPONSE_USAGE_SNAPSHOT_STATUS "Usage: snapshot_status\n"
#define RESPONSE_USAGE_EXPIRE "Usage: expire <key> <seconds>\n"
#define RESPONSE_USAGE_TTL "Usage: ttl <key>\n"
#define RESPONSE_USAGE_TTL_CLEANUP_START "Usage: ttl_cleanup_start <interval_seconds>\n"
#define RESPONSE_USAGE_TTL_CLEANUP_STOP "Usage: ttl_cleanup_stop\n"
#define RESPONSE_USAGE_TTL_CLEANUP_INTERVAL "Usage: ttl_cleanup_interval <interval_seconds>\n"
#define RESPONSE_USAGE_TTL_CLEANUP_STATUS "Usage: ttl_cleanup_status\n"
#define RESPONSE_UNKNOWN_COMMAND "Unknown command\n"
#define MSG(key) Get_Message(key)

extern RuntimeContext* context;

typedef struct
{
  const char* key;
  const char* message;
} MessageLUT;

MessageLUT message_lut[] = { { "OK", RESPONSE_OK },
                             { "FAILED", RESPONSE_FAILED },
                             { "KEY_NOT_FOUND", RESPONSE_KEY_NOT_FOUND },
                             { "USAGE_SET", RESPONSE_USAGE_SET },
                             { "USAGE_GET", RESPONSE_USAGE_GET },
                             { "USAGE_INC", RESPONSE_USAGE_INCR },
                             { "USAGE_APPEND", RESPONSE_USAGE_APPEND },
                             { "USAGE_STRLEN", RESPONSE_USAGE_STRLEN },
                             { "USAGE_EXPORT", RESPONSE_USAGE_EXPORT },
                             { "USAGE_RPUSH", RESPONSE_USAGE_RPUSH },
                             { "USAGE_LPUSH", RESPONSE_USAGE_LPUSH },
                             { "USAGE_RPOP", RESPONSE_USAGE_RPOP },
                             { "USAGE_LPOP", RESPONSE_USAGE_LPOP },
                             { "USAGE_LLEN", RESPONSE_USAGE_LLEN },
                             { "USAGE_LRANGE", RESPONSE_USAGE_LRANGE },
                             { "USAGE_CREATE_USER", RESPONSE_USAGE_CREATE_USER },
                             { "USAGE_AUTH", RESPONSE_USAGE_AUTH },
                             { "USAGE_DELETE_USER", RESPONSE_USAGE_DELETE_USER },
                             { "USAGE_SNAPSHOT_START", RESPONSE_USAGE_SNAPSHOT_START },
                             { "USAGE_SNAPSHOT_STOP", RESPONSE_USAGE_SNAPSHOT_STOP },
                             { "USAGE_SNAPSHOT_INTERVAL", RESPONSE_USAGE_SNAPSHOT_INTERVAL },
                             { "USAGE_SNAPSHOT_STATUS", RESPONSE_USAGE_SNAPSHOT_STATUS },
                             { "USAGE_EXPIRE", RESPONSE_USAGE_EXPIRE },
                             { "USAGE_TTL", RESPONSE_USAGE_TTL },
                             { "USAGE_TTL_CLEANUP_START", RESPONSE_USAGE_TTL_CLEANUP_START },
                             { "USAGE_TTL_CLEANUP_STOP", RESPONSE_USAGE_TTL_CLEANUP_STOP },
                             { "USAGE_TTL_CLEANUP_INTERVAL", RESPONSE_USAGE_TTL_CLEANUP_INTERVAL },
                             { "USAGE_TTL_CLEANUP_STATUS", RESPONSE_USAGE_TTL_CLEANUP_STATUS },
                             { "UNKNOWN_COMMAND", RESPONSE_UNKNOWN_COMMAND } };

static inline const char*
Get_Message(const char* key)
{
  int lut_size = sizeof(message_lut) / sizeof(MessageLUT);
  for (int i = 0; i < lut_size; i++) {
    if (strcmp(message_lut[i].key, key) == 0) {
      return message_lut[i].message;
    }
  }
  return RESPONSE_UNKNOWN_COMMAND;
}

static inline void
TCP_Write(int32_t sock, const char* message, uint8_t new_line)
{
  write(sock, message, strlen(message));
  if (new_line) {
    write(sock, "\n", 1);
  }
}

void
Execute_Command(int sock, ParsedCommand* cmd, Database* db)
{
  if (!cmd->command) {
    return;
  }

  if (strcmp(cmd->command, "set") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL || value == NULL) {
      TCP_Write(sock, MSG("USAGE_SET"), 0);
      return;
    }

    DB_Value val_def = { .string = { strdup(value) } };
    DB_Atomic_Store(db, key, val_def, DB_ENTRY_STRING);

    TCP_Write(sock, MSG("OK"), 0);
  }

  else if (strcmp(cmd->command, "get") == 0) {

    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_GET"), 0);
      return;
    }

    DatabaseEntry res = DB_Atomic_Get(db, key);
    if (res.type == DB_ENTRY_STRING) {
      TCP_Write(sock, res.value.string.value, 1);

    } else if (res.type == DB_ENTRY_NUMBER) {
      char buffer[32];
      sprintf(buffer, "%" PRId64 "\n", res.value.number.value);
      TCP_Write(sock, buffer, 0);

    } else if (res.type == DB_ENTRY_LIST) {
      HPLinkedList* list = res.value.list;
      char* buffer = HPList_ToString(list);
      TCP_Write(sock, buffer, 1);

      free(buffer); // buffer was allocated on heap by ToString
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    }
  } else if (strcmp(cmd->command, "append") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL || value == NULL) {
      TCP_Write(sock, MSG("USAGE_APPEND"), 0);
      return;
    }

    DatabaseEntry res = DB_Atomic_Get(db, key);
    if (res.type == DB_ENTRY_STRING) {
      if (res.value.string.value) {
        char* new_value =
          malloc(strlen(res.value.string.value) + strlen(value) + 1);
        strcpy(new_value, res.value.string.value);
        strcat(new_value, value);
        DB_Value new_val = { .string = { new_value } };
        DB_Atomic_Store(db, key, new_val, DB_ENTRY_STRING);

        TCP_Write(sock, MSG("OK"), 0);

      } else {
        TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
      }
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    }
  } else if (strcmp(cmd->command, "strlen") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_STRLEN"), 0);
      return;
    }

    DatabaseEntry res = DB_Atomic_Get(db, key);
    if (res.type == DB_ENTRY_STRING) {
      if (res.value.string.value) {
        char length[20];
        sprintf(length, "%lu\n", strlen(res.value.string.value));
        TCP_Write(sock, length, 0);
      } else {
        TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
      }
    }

    // todo (David) 'incr' when key exists and value is not a number (it returns
    // -1 and data is not modified)
  } else if (strcmp(cmd->command, "incr") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_INC"), 0);
      return;
    }

    int64_t result = DB_Atomic_Incr(db, key);

    char as_number[20];
    sprintf(as_number, "%" PRId64 "\n", result);
    TCP_Write(sock, as_number, 0);

  } else if (strcmp(cmd->command, "export") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_EXPORT"), 0);
      return;
    }
    if (Export_Snapshot(context, key) == 0) {
      DB_Log(DB_LOG_INFO, "Exporting snapshot %s was successful", key);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Exporting snapshot %s failed", key);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "insp") == 0) {
    Print_Runtime_Context(context);
  }

  else if (strcmp(cmd->command, "rpush") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];
    const TOKEN type = cmd->types[1];

    if (key == NULL || value == NULL) {
      TCP_Write(sock, MSG("USAGE_RPUSH"), 0);
      return;
    }
    DatabaseEntry res = DB_Atomic_Get(db, key);

    if (res.type == DB_ENTRY_LIST) { // append element to list
      HPLinkedList* list = res.value.list;
      switch (type) {
        case TOKEN_STRING: {
          HPList_RPush_String(list, value);
        } break;

        case TOKEN_NUMBER: {
          HPList_RPush_Int(list, atoll(value));
        } break;
      };

      TCP_Write(sock, MSG("OK"), 0);
    }

    else { // not entry, create new list
      HPLinkedList* new_list = HPList_Create();
      switch (type) {
        case TOKEN_STRING: {
          HPList_RPush_String(new_list, value);
        } break;
        case TOKEN_NUMBER: {
          HPList_RPush_Int(new_list, atoll(value));
        } break;
      };

      DB_Value list_val;
      list_val.list = new_list;
      DB_Atomic_Store(db, key, list_val, DB_ENTRY_LIST);

      TCP_Write(sock, MSG("OK"), 0);
    }
  }

  else if (strcmp(cmd->command, "lpush") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];
    const TOKEN type = cmd->types[1];

    if (key == NULL || value == NULL) {
      TCP_Write(sock, MSG("USAGE_LPUSH"), 0);
      return;
    }
    DatabaseEntry res = DB_Atomic_Get(db, key);

    if (res.type == DB_ENTRY_LIST) { // append
      HPLinkedList* list = res.value.list;
      switch (type) {
        case TOKEN_STRING: {
          HPList_LPush_String(list, value);
        } break;

        case TOKEN_NUMBER: {
          HPList_LPush_Int(list, atoll(value));
        } break;
      };
      TCP_Write(sock, MSG("OK"), 0);
    }

    else { // no, entry create new list
      HPLinkedList* new_list = HPList_Create();
      switch (type) {
        case TOKEN_STRING: {
          HPList_LPush_String(new_list, value);
        } break;
        case TOKEN_NUMBER: {
          HPList_LPush_Int(new_list, atoll(value));
        } break;
      };

      DB_Value list_val;
      list_val.list = new_list;
      DB_Atomic_Store(db, key, list_val, DB_ENTRY_LIST);
      TCP_Write(sock, MSG("OK"), 0);
    }
  }

  else if (strcmp(cmd->command, "lpop") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_LPOP"), 0);
      return;
    }

    DatabaseEntry res = DB_Atomic_Get(db, key);
    if (res.type == DB_ENTRY_LIST) {
      HPLinkedList* list = res.value.list;
      ListNode* node = HPList_LPop(list);

      if (node) {
        if (node->type == TYPE_STRING) {
          TCP_Write(sock, node->value.string_value, 0);
        } else if (node->type == TYPE_INT) {
          char buffer[32];
          sprintf(buffer, "%" PRId64, node->value.int_value);
          TCP_Write(sock, buffer, 0);

        } else if (node->type == TYPE_FLOAT) {
          char buffer[32];
          sprintf(buffer, "%f", node->value.float_value);
          TCP_Write(sock, buffer, 0);
        }

        TCP_Write(sock, "\n", 0);

      } else {
        TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
      }
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    }
  }

  else if (strcmp(cmd->command, "rpop") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_RPOP"), 0);
      return;
    }
    DatabaseEntry res = DB_Atomic_Get(db, key);

    if (res.type == DB_ENTRY_LIST) {
      HPLinkedList* list = res.value.list;
      ListNode* node = HPList_RPop(list);

      if (node) {
        if (node->type == TYPE_STRING) {
          TCP_Write(sock, node->value.string_value, 0);
        } else if (node->type == TYPE_INT) {
          char buffer[32];
          sprintf(buffer, "%" PRId64, node->value.int_value);
          TCP_Write(sock, buffer, 0);
        } else if (node->type == TYPE_FLOAT) {
          char buffer[32];
          sprintf(buffer, "%f", node->value.float_value);
          TCP_Write(sock, buffer, 0);
        }

        TCP_Write(sock, "\n", 0);

      } else {
        TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
      }
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    }
  }

  else if (strcmp(cmd->command, "llen") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_LLEN"), 0);
      return;
    }
    DatabaseEntry res = DB_Atomic_Get(db, key);

    if (res.type == DB_ENTRY_LIST) {
      HPLinkedList* list = res.value.list;
      char buffer[32];
      sprintf(buffer, "%zu\n", list->count);
      TCP_Write(sock, buffer, 0);
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    }
  } else if (strcmp(cmd->command, "sub") == 0) {
    Subscribe(context->pubsub_system, cmd->argv[0], sock);
  } else if (strcmp(cmd->command, "unsub") == 0) {
    Unsubscribe(context->pubsub_system, cmd->argv[0], sock);
  } else if (strcmp(cmd->command, "pub") == 0) {
    Publish(context->pubsub_system, cmd->argv[0], cmd->argv[1]);
  } else if (strcmp(cmd->command, "lrange") == 0) {
    if (cmd->argc < 3) {
      TCP_Write(sock, MSG("USAGE_LRANGE"), 0);
      return;
    }
    const char* key = cmd->argv[0];
    int start = atoi(cmd->argv[1]);
    int stop = atoi(cmd->argv[2]);

    DatabaseEntry res = DB_Atomic_Get(db, key);
    if (res.type == DB_ENTRY_LIST) {
      HPLinkedList* list = res.value.list;
      char* buffer = HPList_ToString(list);
      TCP_Write(sock, buffer, 1);

      free(buffer); // buffer was allocated on heap by ToString
    } else {
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 1);
    }
  } else if (strcmp(cmd->command, "load") == 0) {
    if (Import_Snapshot(context, "snapshot.bin") == 0) {
      DB_Log(DB_LOG_INFO, "SNAPSHOT was loaded successfully");
    }
  } else if (strcmp(cmd->command, "create_user") == 0) {
    const char* username = cmd->argv[0];
    const char* password = cmd->argv[1];

    if (username == NULL || password == NULL) {
      TCP_Write(sock, MSG("USAGE_CREATE_USER"), 0);
      return;
    }

    if (Create_User(context, username, password) == 0) {
      DB_Log(DB_LOG_INFO, "User %s created successfully", username);
      Print_User_Manager_State(context);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to create user %s", username);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "auth") == 0) {
    const char* username = cmd->argv[0];
    const char* password = cmd->argv[1];

    if (username == NULL || password == NULL) {
      TCP_Write(sock, MSG("USAGE_AUTH"), 0);
      return;
    }

    if (Authenticate_User(context, username, password) == 0) {
      DB_Log(DB_LOG_INFO, "User %s authenticated successfully", username);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to authenticate user %s", username);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "delete_user") == 0) {
    const char* username = cmd->argv[0];

    if (username == NULL) {
      TCP_Write(sock, MSG("USAGE_DELETE_USER"), 0);
      return;
    }

    if (Delete_User(context, username) == 0) {
      DB_Log(DB_LOG_INFO, "User %s deleted successfully", username);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to delete user %s", username);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "snapshot_start") == 0) {
    if (cmd->argc < 2) {
      TCP_Write(sock, MSG("USAGE_SNAPSHOT_START"), 0);
      return;
    }
    const char* interval_str = cmd->argv[0];
    const char* filename = cmd->argv[1];

    int interval = atoi(interval_str);
    if (interval <= 0) {
      TCP_Write(sock, MSG("USAGE_SNAPSHOT_START"), 0);
      return;
    }

    if (Start_Background_Snapshot(context, interval, filename) == 0) {
      DB_Log(DB_LOG_INFO, "Snapshot started successfully with interval %d seconds and filename %s", interval, filename);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to start snapshot with interval %d seconds and filename %s", interval, filename);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "snapshot_stop") == 0) {
    if (Stop_Background_Snapshot(context) == 0) {
      DB_Log(DB_LOG_INFO, "Snapshot stopped successfully");
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to stop snapshot");
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "snapshot_interval") == 0) {
    if (cmd->argc < 1) {
      TCP_Write(sock, MSG("USAGE_SNAPSHOT_INTERVAL"), 0);
      return;
    }
    const char* interval_str = cmd->argv[0];

    int interval = atoi(interval_str);
    if (interval <= 0) {
      TCP_Write(sock, MSG("USAGE_SNAPSHOT_INTERVAL"), 0);
      return;
    }

    if (Set_Snapshot_Interval(context, interval) == 0) {
      DB_Log(DB_LOG_INFO, "Snapshot interval set to %d seconds", interval);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to set snapshot interval to %d seconds", interval);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "snapshot_status") == 0) {
    char status_buffer[256];
    if (atomic_load(&context->snapshot_config.running)) {
      time_t current_time = time(NULL);
      time_t elapsed = current_time - context->snapshot_config.last_snapshot_time;
      snprintf(status_buffer, sizeof(status_buffer), 
              "Snapshot status: active\nInterval: %d seconds\nLast snapshot: %ld seconds ago\nFilename: %s\n",
              context->snapshot_config.interval_seconds,
              elapsed,
              context->snapshot_config.snapshot_filename);
    } else {
      snprintf(status_buffer, sizeof(status_buffer), 
              "Snapshot status: inactive\n");
    }
    TCP_Write(sock, status_buffer, 0);
  } else if (strcmp(cmd->command, "expire") == 0 || strcmp(cmd->command, "ttl_set") == 0) {
    const char* key = cmd->argv[0];
    const char* seconds_str = cmd->argv[1];

    if (key == NULL || seconds_str == NULL) {
      TCP_Write(sock, MSG("USAGE_EXPIRE"), 0);
      return;
    }

    int64_t seconds = atoll(seconds_str);
    if (seconds < 0) {
      TCP_Write(sock, MSG("FAILED"), 0);
      return;
    }

    if (Set_TTL(db, key, seconds) == 0) {
      DB_Log(DB_LOG_INFO, "Set TTL for key %s to %ld seconds", key, seconds);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to set TTL for key %s", key);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "ttl") == 0) {
    const char* key = cmd->argv[0];

    if (key == NULL) {
      TCP_Write(sock, MSG("USAGE_TTL"), 0);
      return;
    }

    int64_t ttl = Get_TTL(db, key);
    char ttl_buffer[32];
    
    if (ttl == -1) {
      // Key doesn't exist
      TCP_Write(sock, MSG("KEY_NOT_FOUND"), 0);
    } else if (ttl == -2) {
      // No TTL set
      snprintf(ttl_buffer, sizeof(ttl_buffer), "-1\n");
      TCP_Write(sock, ttl_buffer, 0);
    } else {
      // TTL exists
      snprintf(ttl_buffer, sizeof(ttl_buffer), "%lld\n", ttl);
      TCP_Write(sock, ttl_buffer, 0);
    }
  } else if (strcmp(cmd->command, "ttl_cleanup_start") == 0) {
    if (cmd->argc < 1) {
      TCP_Write(sock, MSG("USAGE_TTL_CLEANUP_START"), 0);
      return;
    }
    const char* interval_str = cmd->argv[0];

    int interval = atoi(interval_str);
    if (interval <= 0) {
      TCP_Write(sock, MSG("USAGE_TTL_CLEANUP_START"), 0);
      return;
    }

    if (Start_TTL_Cleanup(context, interval) == 0) {
      DB_Log(DB_LOG_INFO, "TTL cleanup started successfully with interval %d seconds", interval);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to start TTL cleanup with interval %d seconds", interval);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "ttl_cleanup_stop") == 0) {
    if (Stop_TTL_Cleanup(context) == 0) {
      DB_Log(DB_LOG_INFO, "TTL cleanup stopped successfully");
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to stop TTL cleanup");
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "ttl_cleanup_interval") == 0) {
    if (cmd->argc < 1) {
      TCP_Write(sock, MSG("USAGE_TTL_CLEANUP_INTERVAL"), 0);
      return;
    }
    const char* interval_str = cmd->argv[0];

    int interval = atoi(interval_str);
    if (interval <= 0) {
      TCP_Write(sock, MSG("USAGE_TTL_CLEANUP_INTERVAL"), 0);
      return;
    }

    if (Set_TTL_Cleanup_Interval(context, interval) == 0) {
      DB_Log(DB_LOG_INFO, "TTL cleanup interval set to %d seconds", interval);
      TCP_Write(sock, MSG("OK"), 0);
    } else {
      DB_Log(DB_LOG_ERROR, "Failed to set TTL cleanup interval to %d seconds", interval);
      TCP_Write(sock, MSG("FAILED"), 0);
    }
  } else if (strcmp(cmd->command, "ttl_cleanup_status") == 0) {
    char status_buffer[256];
    if (atomic_load(&context->ttl_cleanup_config.running)) {
      time_t current_time = time(NULL);
      time_t elapsed = current_time - context->ttl_cleanup_config.last_cleanup_time;
      snprintf(status_buffer, sizeof(status_buffer), 
              "TTL cleanup status: active\nInterval: %d seconds\nLast cleanup: %ld seconds ago\n",
              context->ttl_cleanup_config.interval_seconds,
              elapsed);
    } else {
      snprintf(status_buffer, sizeof(status_buffer), 
              "TTL cleanup status: inactive\n");
    }
    TCP_Write(sock, status_buffer, 0);
  } else {
    TCP_Write(sock, MSG("UNKNOWN_COMMAND"), 0);
  }
}
