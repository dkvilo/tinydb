#ifndef __TINY_DB_QUERY_PARSER
#define __TINY_DB_QUERY_PARSER

#include "tinydb_lex.h"

#define MAX_ARGS 10

typedef enum TOKEN
{
  TOKEN_STRING = 0,
  TOKEN_NUMBER
} TOKEN;

typedef struct
{
  char* command;
  int32_t argc;
  char* argv[MAX_ARGS];
  TOKEN types[MAX_ARGS];
  Lexer lex;
} ParsedCommand;

ParsedCommand*
Parse_Command(char* buffer, size_t buffer_size, size_t* total_read);

void
Free_Parsed_Command(ParsedCommand* cmd);

#endif // __TINY_DB_QUERY_PARSER
