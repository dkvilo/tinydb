#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_query_parser.h"

static TOKEN
detect_token_type(const char* token)
{
  int is_number = 1;
  int decimal_point_found = 0;
  size_t length = strlen(token);

  if (token[0] == '"' && token[length - 1] == '"') {
    return TOKEN_STRING;
  }

  for (size_t i = 0; i < length; ++i) {
    if (token[i] == '.') {
      if (decimal_point_found) {
        is_number = 0;
        break;
      }
      decimal_point_found = 1;
    } else if (!isdigit(token[i])) {
      is_number = 0;
      break;
    }
  }
  return is_number ? TOKEN_NUMBER : TOKEN_STRING;
}

static char*
trim_quotes(char* str)
{
  if (str == NULL)
    return NULL;
  size_t len = strlen(str);
  if (len > 1 && str[0] == '"' && str[len - 1] == '"') {
    str[len - 1] = '\0';
    return str + 1;
  }
  return str;
}

ParsedCommand*
Parse_Command(const char* input)
{
  ParsedCommand* cmd = calloc(1, sizeof(ParsedCommand));
  if (!cmd)
    return NULL;

  char* input_copy = strdup(input);
  if (!input_copy) {
    free(cmd);
    return NULL;
  }

  char* saveptr;
  char* token = strtok_r(input_copy, " ", &saveptr);
  if (token) {
    cmd->command = strdup(token);
    if (!cmd->command)
      goto cleanup;
  }

  char* key = strtok_r(NULL, " ", &saveptr);
  if (key) {
    cmd->argv[cmd->argc] = strdup(key);
    if (!cmd->argv[cmd->argc])
      goto cleanup;
    cmd->types[cmd->argc] = TOKEN_STRING;
    cmd->argc++;

    char* value = strtok_r(NULL, "", &saveptr);
    if (value) {
      while (*value == ' ')
        value++;
      if (value[0] == '"') {
        char* end_quote = strrchr(value, '"');
        if (end_quote) {
          *end_quote = '\0';
          cmd->argv[cmd->argc] = strdup(value + 1);
        } else {
          cmd->argv[cmd->argc] = strdup(value);
        }
      } else {
        cmd->argv[cmd->argc] = strdup(value);
      }
      if (!cmd->argv[cmd->argc])
        goto cleanup;
      cmd->types[cmd->argc] = detect_token_type(cmd->argv[cmd->argc]);
      cmd->argc++;
    }
  }

  free(input_copy);
  return cmd;

cleanup:
  Free_Parsed_Command(cmd);
  free(input_copy);
  return NULL;
}

void
Free_Parsed_Command(ParsedCommand* cmd)
{
  if (cmd) {
    free(cmd->command);
    for (int i = 0; i < cmd->argc; i++) {
      free(cmd->argv[i]);
    }
    free(cmd);
  }
}