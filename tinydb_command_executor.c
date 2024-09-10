#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "tinydb.h"
#include "tinydb_command_executor.h"
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
#define RESPONSE_UNKNOWN_COMMAND "Unknown command\n"

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
                             { "UNKNOWN_COMMAND", RESPONSE_UNKNOWN_COMMAND } };

const char*
get_message(const char* key)
{
  int lut_size = sizeof(message_lut) / sizeof(MessageLUT);
  for (int i = 0; i < lut_size; i++) {
    if (strcmp(message_lut[i].key, key) == 0) {
      return message_lut[i].message;
    }
  }
  return RESPONSE_UNKNOWN_COMMAND;
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
      write(sock, get_message("USAGE_SET"), strlen(get_message("USAGE_SET")));
    } else {
      DB_Value val_def = { .string = { strdup(value) } };
      DB_Atomic_Store(db, key, val_def, DB_ENTRY_STRING);
      write(sock, get_message("OK"), strlen(get_message("OK")));
    }
  }

  else if (strcmp(cmd->command, "GET") == 0) {

    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(sock, get_message("USAGE_GET"), strlen(get_message("USAGE_GET")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);
      if (res.type == DB_ENTRY_STRING) {
        write(sock, res.value.string.value, strlen(res.value.string.value));
        write(sock, "\n", 1);
      } else if (res.type == DB_ENTRY_NUMBER) {
        char buffer[32];
        sprintf(buffer, "%" PRId64 "\n", res.value.number.value);
        write(sock, buffer, strlen(buffer));
      } else if (res.type == DB_ENTRY_LIST) {
        HPLinkedList* list = res.value.list;
        char* buffer = HPList_ToString(list);

        write(sock, buffer, strlen(buffer));
        write(sock, "\n", 1);

        free(buffer);
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  } else if (strcmp(cmd->command, "APPEND") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL || value == NULL) {
      write(
        sock, get_message("USAGE_APPEND"), strlen(get_message("USAGE_APPEND")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);
      if (res.type == DB_ENTRY_STRING) {
        if (res.value.string.value) {
          char* new_value =
            malloc(strlen(res.value.string.value) + strlen(value) + 1);
          strcpy(new_value, res.value.string.value);
          strcat(new_value, value);
          DB_Value new_val = { .string = { new_value } };
          DB_Atomic_Store(db, key, new_val, DB_ENTRY_STRING);
          write(sock, get_message("OK"), strlen(get_message("OK")));
        } else {
          write(sock,
                get_message("KEY_NOT_FOUND"),
                strlen(get_message("KEY_NOT_FOUND")));
        }
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  } else if (strcmp(cmd->command, "STRLEN") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(
        sock, get_message("USAGE_STRLEN"), strlen(get_message("USAGE_STRLEN")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);
      if (res.type == DB_ENTRY_STRING) {
        if (res.value.string.value) {
          char length[20];
          sprintf(length, "%lu\n", strlen(res.value.string.value));
          write(sock, length, strlen(length));
        } else {
          write(sock,
                get_message("KEY_NOT_FOUND"),
                strlen(get_message("KEY_NOT_FOUND")));
        }
      }
    }

    // TODO (FIX) INCR not a number 
  } else if (strcmp(cmd->command, "INCR") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(sock, get_message("USAGE_INC"), strlen(get_message("USAGE_INC")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);
      // if (res.type == DB_ENTRY_NUMBER) {
        if (res.value.number.value || res.value.number.value == 0) {
          res.value.number.value++;
          DB_Atomic_Store(db, key, res.value, DB_ENTRY_NUMBER);

          char response[20];
          sprintf(response, "%"PRId64"\n", res.value.number.value);
          write(sock, response, strlen(response));
        } else {
          DB_Value new_val;
          new_val.number.value = 1;

          DB_Atomic_Store(db, key, new_val, DB_ENTRY_NUMBER);
          write(sock, "1\n", 2);
        }
      // }
      //  else {
      //   DB_Log(DB_LOG_WARNING, "Not an number!");
      // }
    }
  } else if (strcmp(cmd->command, "EXPORT") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(
        sock, get_message("USAGE_EXPORT"), strlen(get_message("USAGE_EXPORT")));
    } else {
      if (Export_Snapshot(context, key) == 0) {
        DB_Log(DB_LOG_INFO, "Exporting snapshot %s was successful", key);
        write(sock, get_message("OK"), strlen(get_message("OK")));
      } else {
        DB_Log(DB_LOG_ERROR, "Exporting snapshot %s failed", key);
        write(sock, get_message("FAILED"), strlen(get_message("FAILED")));
      }
    }
  } else if (strcmp(cmd->command, "INSP") == 0) {
    Print_Runtime_Context(context);
  }

  else if (strcmp(cmd->command, "RPUSH") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];
    const TOKEN type = cmd->types[1];

    if (key == NULL || value == NULL) {
      write(sock, "Usage: RPUSH <key> <value>\n", 27);
    } else {
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
        write(sock, "OK\n", 3);
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

        write(sock, "OK\n", 3);
      }
    }
  }

  else if (strcmp(cmd->command, "LPUSH") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];
    const TOKEN type = cmd->types[1];

    if (key == NULL || value == NULL) {
      write(sock, "Usage: LPUSH <key> <value>\n", 27);
    } else {
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
        write(sock, "OK\n", 3);
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
        write(sock, "OK\n", 3);
      }
    }
  }

  else if (strcmp(cmd->command, "LPOP") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(sock, "Usage: LPOP <key>\n", 19);
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);

      if (res.type == DB_ENTRY_LIST) {
        HPLinkedList* list = res.value.list;
        ListNode* node = HPList_LPop(list);

        if (node) {
          if (node->type == TYPE_STRING) {
            write(
              sock, node->value.string_value, strlen(node->value.string_value));
          } else if (node->type == TYPE_INT) {
            char buffer[32];
            sprintf(buffer, "%"PRId64, node->value.int_value);
            write(sock, buffer, strlen(buffer));
          } else if (node->type == TYPE_FLOAT) {
            char buffer[32];
            sprintf(buffer, "%f", node->value.float_value);
            write(sock, buffer, strlen(buffer));
          }

          write(sock, "\n", 1);
        } else {
          write(sock, "null\n", 5);
        }
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  }

  else if (strcmp(cmd->command, "RPOP") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(sock, "Usage: RPOP <key>\n", 19);
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);

      if (res.type == DB_ENTRY_LIST) {
        HPLinkedList* list = res.value.list;
        ListNode* node = HPList_RPop(list);

        if (node) {
          if (node->type == TYPE_STRING) {
            write(
              sock, node->value.string_value, strlen(node->value.string_value));
          } else if (node->type == TYPE_INT) {
            char buffer[32];
            sprintf(buffer, "%"PRId64, node->value.int_value);
            write(sock, buffer, strlen(buffer));
          } else if (node->type == TYPE_FLOAT) {
            char buffer[32];
            sprintf(buffer, "%f", node->value.float_value);
            write(sock, buffer, strlen(buffer));
          }
          write(sock, "\n", 1);

        } else {
          write(sock, "null\n", 5);
        }
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  }

  else if (strcmp(cmd->command, "LLEN") == 0) {
    const char* key = cmd->argv[0];
    const char* value = cmd->argv[1];

    if (key == NULL) {
      write(sock, "Usage: LLEN <key>\n", 18);
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, key);

      if (res.type == DB_ENTRY_LIST) {
        HPLinkedList* list = res.value.list;
        char buffer[32];
        sprintf(buffer, "%zu\n", list->count);
        write(sock, buffer, strlen(buffer));
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  }

  else if (strcmp(cmd->command, "LRANGE") == 0) {
    if (cmd->argc < 3) {
      write(sock, "Usage: LRANGE <key> <start> <stop>\n", 35);
    } else {
      const char* key = cmd->argv[0];
      int start = atoi(cmd->argv[1]);
      int stop = atoi(cmd->argv[2]);

      DatabaseEntry res = DB_Atomic_Get(db, key);

      if (res.type == DB_ENTRY_LIST) {
        HPLinkedList* list = res.value.list;
        pthread_rwlock_rdlock(&list->rwlock);

        ListNode* current = list->head;
        int index = 0;

        // skip start
        while (current && index < start) {
          current = current->next;
          index++;
        }

        // iterating from start to stop
        while (current && index <= stop) {
          if (current->type == TYPE_STRING) {
            write(sock,
                  current->value.string_value,
                  strlen(current->value.string_value));
          } else if (current->type == TYPE_INT) {
            char buffer[32];
            sprintf(buffer, "%"PRId64, current->value.int_value);
            write(sock, buffer, strlen(buffer));
          } else if (current->type == TYPE_FLOAT) {
            char buffer[32];
            sprintf(buffer, "%f", current->value.float_value);
            write(sock, buffer, strlen(buffer));
          }
          if (index != stop) {
            write(sock, ", ", 2);
          }
          current = current->next;
          index++;
        }

        pthread_rwlock_unlock(&list->rwlock);
        write(sock, "\n", 1);
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  }

  else if (strcmp(cmd->command, "LOAD") == 0) {
    if (Import_Snapshot(context, "snapshot.bin") == 0) {
      DB_Log(DB_LOG_INFO, "SNAPSHOT was loaded successfully");
    }
  } else {
    write(sock,
          get_message("UNKNOWN_COMMAND"),
          strlen(get_message("UNKNOWN_COMMAND")));
  }
}
