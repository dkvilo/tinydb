#ifndef __TINY_DB_UTILS
#define __TINY_DB_UTILS

#include <pthread.h>
#include <stdlib.h>

#include "tinydb_database.h"

void
DB_Utils_Save_To_File(Database* db, const char* filename);

void
Append_To_Buffer(char** buffer,
                 const char* str,
                 size_t* buffer_size,
                 size_t* buffer_len);

#endif // __TINY_DB_UTILS