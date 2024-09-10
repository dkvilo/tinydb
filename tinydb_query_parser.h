#ifndef __TINY_DB_QUERY_PARSER
#define __TINY_DB_QUERY_PARSER

#define MAX_ARGS 10

typedef enum TOKEN
{
  TOKEN_STRING,
  TOKEN_NUMBER
} TOKEN;

typedef struct
{
  char* command;
  int argc;
  char* argv[MAX_ARGS];
  TOKEN types[MAX_ARGS];
} ParsedCommand;

ParsedCommand*
Parse_Command(const char* input);

void
Free_Parsed_Command(ParsedCommand* cmd);

#endif // __TINY_DB_QUERY_PARSER
