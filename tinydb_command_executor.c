#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tinydb_atomic_proc.h"
#include "tinydb_command_executor.h"
#include "tinydb_database.h"
#include "tinydb_list.h"
#include "tinydb_log.h"
#include "tinydb_snapshot.h"

#define RESPONSE_OK "Ok\n"
#define RESPONSE_FAILED "FAILED\n"
#define RESPONSE_KEY_NOT_FOUND "null\n"
#define RESPONSE_USAGE_SET "Usage: SET <key> <value>\n"
#define RESPONSE_USAGE_GET "Usage: GET <key>\n"
#define RESPONSE_USAGE_INCR "Usage: INCR <key>\n"
#define RESPONSE_USAGE_APPEND "Usage: APPEND <key> <value>\n"
#define RESPONSE_USAGE_STRLEN "Usage: STRLEN <key>\n"
#define RESPONSE_USAGE_EXPORT "Usage: EXPORT snapshot.bin\n"
#define RESPONSE_USAGE_RPUSH "Usage: RPUSH <key> <value>\n"
#define RESPONSE_USAGE_LPUSH "Usage: LPUSH <key> <value>\n"
#define RESPONSE_USAGE_RPOP "Usage: RPOP <key>\n"
#define RESPONSE_USAGE_LPOP "Usage: LPOP <key>\n"
#define RESPONSE_USAGE_LLEN "Usage: LLEN <key>\n"
#define RESPONSE_USAGE_LRANGE "Usage: LRANGE <key> <min> <max>\n"
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
                             { "USAGE_LPOP", RESPONSE_USAGE_LPOP },
                             { "USAGE_RPOP", RESPONSE_USAGE_RPOP },
                             { "USAGE_LLEN", RESPONSE_USAGE_LLEN },
                             { "USAGE_LRANGE", RESPONSE_USAGE_LRANGE },
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

  if (strcmp(cmd->command, "SET") == 0) {
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

  else if (strcmp(cmd->command, "GET") == 0) {

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
  } else if (strcmp(cmd->command, "APPEND") == 0) {
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
  } else if (strcmp(cmd->command, "STRLEN") == 0) {
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

    // TODO (FIX) INCR not a number
  } else if (strcmp(cmd->command, "INCR") == 0) {
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

  } else if (strcmp(cmd->command, "EXPORT") == 0) {
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
  } else if (strcmp(cmd->command, "INSP") == 0) {
    Print_Runtime_Context(context);
  }

  else if (strcmp(cmd->command, "RPUSH") == 0) {
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

  else if (strcmp(cmd->command, "LPUSH") == 0) {
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

  else if (strcmp(cmd->command, "LPOP") == 0) {
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

  else if (strcmp(cmd->command, "RPOP") == 0) {
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

  else if (strcmp(cmd->command, "LLEN") == 0) {
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
  }
  else if (strcmp(cmd->command, "SUB") == 0) {
    Subscribe(context->pubsub_system, cmd->argv[0], sock);
  } else if (strcmp(cmd->command, "UNSUB") == 0) {
    Unsubscribe(context->pubsub_system, cmd->argv[0], sock);
  } else if (strcmp(cmd->command, "PUB") == 0) {
    Publish(context->pubsub_system, cmd->argv[0], cmd->argv[1]);
  }
  else if (strcmp(cmd->command, "LRANGE") == 0) {
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
  } else if (strcmp(cmd->command, "LOAD") == 0) {
    if (Import_Snapshot(context, "snapshot.bin") == 0) {
      DB_Log(DB_LOG_INFO, "SNAPSHOT was loaded successfully");
    }
  } else {
    TCP_Write(sock, MSG("UNKNOWN_COMMAND"), 0);
  }
}
