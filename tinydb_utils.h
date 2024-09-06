#ifndef __TINY_DB_UTILS
#define __TINY_DB_UTILS

#include <stdlib.h>
#include <pthread.h>

#include "tinydb.h"

void
DB_Utils_Save_To_File(Database* db, const char* filename);

#endif // __TINY_DB_UTILS