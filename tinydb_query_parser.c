#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_query_parser.h"

ParsedCommand*
Parse_Command(const char* input)
{
  ParsedCommand* cmd = malloc(sizeof(ParsedCommand));
  if (!cmd)
    return NULL;

  char* input_copy = strdup(input);
  if (!input_copy) {
    free(cmd);
    return NULL;
  }

  cmd->command = strtok(input_copy, " ");
  cmd->key = strtok(NULL, " ");
  cmd->value = strtok(NULL, "\0");

  if (cmd->command)
    cmd->command = strdup(cmd->command);
  if (cmd->key)
    cmd->key = strdup(cmd->key);
  if (cmd->value)
    cmd->value = strdup(cmd->value);

  free(input_copy);
  return cmd;
}

void
Free_Parsed_Command(ParsedCommand* cmd)
{
  if (cmd) {
    if (cmd->command) {
      free(cmd->command);
      cmd->command = NULL;
    }
    if (cmd->key) {
      free(cmd->key);
      cmd->key = NULL;
    }
    if (cmd->value) {
      free(cmd->value);
      cmd->value = NULL;
    }
    free(cmd);
  }
}
