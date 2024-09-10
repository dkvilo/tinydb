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
  ParsedCommand* cmd = malloc(sizeof(ParsedCommand));
  if (!cmd)
    return NULL;

  cmd->command = NULL;
  cmd->argc = 0;
  memset(cmd->argv, 0, sizeof(cmd->argv));
  memset(cmd->types, 0, sizeof(cmd->types));

  char* input_copy = strdup(input);
  if (!input_copy) {
    free(cmd);
    return NULL;
  }

  char* token = strtok(input_copy, " ");
  if (token) {
    cmd->command = strdup(token);
  }

  while ((token = strtok(NULL, " ")) != NULL && cmd->argc < MAX_ARGS) {
    TOKEN token_type = detect_token_type(token);
    if (token_type == TOKEN_STRING) {
      token = trim_quotes(token);
    }

    cmd->argv[cmd->argc] = strdup(token);
    cmd->types[cmd->argc] = token_type;
    cmd->argc++;
  }

  free(input_copy);
  return cmd;
}

void
Free_Parsed_Command(ParsedCommand* cmd)
{
  if (cmd) {
    if (cmd->command) {
      free(cmd->command);
    }
    for (int i = 0; i < cmd->argc; i++) {
      if (cmd->argv[i]) {
        free(cmd->argv[i]);
      }
    }
    free(cmd);
  }
}
