#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_query_parser.h"

TOKEN
To_Command_Token(LEX_TOKEN ltok)
{
  switch (ltok) {
    case LEX_TOKEN_STRING:
    case LEX_TOKEN_IDENTIFIER:
      return TOKEN_STRING;
      break;
    case LEX_TOKEN_NUMBER:
      return TOKEN_NUMBER;
      break;
    default:
      return TOKEN_STRING;
      break;
  }
}

ParsedCommand*
Parse_Command(char* buffer, size_t buffer_size, size_t* total_read)
{
  ParsedCommand* cmd = calloc(1, sizeof(ParsedCommand));
  if (!cmd) {
    return NULL;
  }

  cmd->lex = (Lexer){ 0 };
  Lexer_Reset(&cmd->lex);
  Lexer_Lex(&cmd->lex, (const uint8_t*)buffer, strlen(buffer));

  // reset the buffer
  *(total_read) = 0;
  memset(buffer, 0, buffer_size);

  if (cmd->lex.token_count > 0) {
    for (size_t i = 0; i < cmd->lex.token_count; ++i) {
      Token t = cmd->lex.tokens[i];
      if (t.type == LEX_TOKEN_EOF)
        continue;

      if (i == 0 && t.type == LEX_TOKEN_COMMAND) {
        cmd->command = strdup(t.value);
        continue;
      }

      if (!cmd->command)
        break;
      cmd->argv[cmd->argc] = strdup(t.value);
      cmd->types[cmd->argc] = To_Command_Token(t.type);
      cmd->argc++;
    }
  }

  return cmd;
}

void
Free_Parsed_Command(ParsedCommand* cmd)
{
  // free the lexer stuff
  for (size_t i = 0; i < cmd->lex.token_count; i++) {
    Lexer_Free_Token_Value(&cmd->lex.tokens[i]);
  }

  // free the cmd stuff
  if (cmd) {
    free(cmd->command);
    for (int i = 0; i < cmd->argc; i++) {
      free(cmd->argv[i]);
    }
    free(cmd);
  }
}