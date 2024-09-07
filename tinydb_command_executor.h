#ifndef __TINY_DB_COMMAND_EXECUTOR
#define __TINY_DB_COMMAND_EXECUTOR

#include "tinydb.h"
#include "tinydb_query_parser.h"

void
Execute_Command(int sock, ParsedCommand* cmd, Database* db);

#endif // __TINY_DB_COMMAND_EXECUTOR
