#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
  if (!cmd->command)
    return;
  if (strcmp(cmd->command, "SET") == 0) {
    if (cmd->key == NULL || cmd->value == NULL) {
      write(sock, get_message("USAGE_SET"), strlen(get_message("USAGE_SET")));
    } else {
      DB_Value val_def = { .string = { strdup(cmd->value) } };
      DB_Atomic_Store(db, cmd->key, val_def, DB_ENTRY_STRING);
      write(sock, get_message("OK"), strlen(get_message("OK")));
    }
  } else if (strcmp(cmd->command, "GET") == 0) {
    if (cmd->key == NULL) {
      write(sock, get_message("USAGE_GET"), strlen(get_message("USAGE_GET")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, cmd->key);
      if (res.type == DB_ENTRY_STRING) {
        write(sock, res.value.string.value, strlen(res.value.string.value));
        write(sock, "\n", 1);
      } else if (res.type == DB_ENTRY_NUMBER) {
        char buffer[32];
        sprintf(buffer, "%d\n", res.value.number);
        write(sock, buffer, strlen(buffer));
      } else {
        write(sock,
              get_message("KEY_NOT_FOUND"),
              strlen(get_message("KEY_NOT_FOUND")));
      }
    }
  } else if (strcmp(cmd->command, "APPEND") == 0) {
    if (cmd->key == NULL || cmd->value == NULL) {
      write(
        sock, get_message("USAGE_APPEND"), strlen(get_message("USAGE_APPEND")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, cmd->key);
      if (res.type == DB_ENTRY_STRING) {
        if (res.value.string.value) {
          char* new_value =
            malloc(strlen(res.value.string.value) + strlen(cmd->value) + 1);
          strcpy(new_value, res.value.string.value);
          strcat(new_value, cmd->value);
          DB_Value new_val = { .string = { new_value } };
          DB_Atomic_Store(db, cmd->key, new_val, DB_ENTRY_STRING);
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
    if (cmd->key == NULL) {
      write(
        sock, get_message("USAGE_STRLEN"), strlen(get_message("USAGE_STRLEN")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, cmd->key);
      if (res.type == DB_ENTRY_STRING) {
        if (res.value.string.value) {
          char length[20];
          sprintf(length, "%ld\n", strlen(res.value.string.value));
          write(sock, length, strlen(length));
        } else {
          write(sock,
                get_message("KEY_NOT_FOUND"),
                strlen(get_message("KEY_NOT_FOUND")));
        }
      }
    }
  } else if (strcmp(cmd->command, "INCR") == 0) {
    if (cmd->key == NULL) {
      write(sock, get_message("USAGE_INC"), strlen(get_message("USAGE_INC")));
    } else {
      DatabaseEntry res = DB_Atomic_Get(db, cmd->key);
      if (res.type == DB_ENTRY_NUMBER) {
        if (res.value.number.value || res.value.number.value == 0) {
          res.value.number.value++;
          DB_Atomic_Store(db, cmd->key, res.value, DB_ENTRY_NUMBER);

          char response[20];
          sprintf(response, "%ld\n", res.value.number.value);
          write(sock, response, strlen(response));
        } else {
          DB_Value new_val;
          new_val.number.value = 1;

          DB_Atomic_Store(db, cmd->key, new_val, DB_ENTRY_NUMBER);
          write(sock, "1\n", 2);
        }
      }
    }
  } else if (strcmp(cmd->command, "EXPORT") == 0) {
    if (cmd->key == NULL) {
      write(
        sock, get_message("USAGE_EXPORT"), strlen(get_message("USAGE_EXPORT")));
    } else {
      if (Export_Snapshot(context, cmd->key) == 0) {
        DB_Log(DB_LOG_INFO, "Exporting snapshot %s was successful", cmd->key);
        write(sock, get_message("OK"), strlen(get_message("OK")));
      } else {
        DB_Log(DB_LOG_ERROR, "Exporting snapshot %s failed", cmd->key);
        write(sock, get_message("FAILED"), strlen(get_message("FAILED")));
      }
    }
  } else if (strcmp(cmd->command, "INSP") == 0) {
    Print_Runtime_Context(context);
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
