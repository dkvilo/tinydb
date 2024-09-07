#ifndef __TINY_DB_QUERY_PARSER
#define __TINY_DB_QUERY_PARSER

typedef struct
{
  char* command;
  char* key;
  char* value;
} ParsedCommand;

ParsedCommand*
Parse_Command(const char* input);

void
Free_Parsed_Command(ParsedCommand* cmd);

#endif //__TINY_DB_QUERY_PARSER
