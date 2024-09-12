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

void
Escape_String(char* dest, const char* src);

void
Add_Indentation(char* dest, int32_t level);

// following functions are temporary and they are used for testing
char*
Serialize_DB_Object_ToJSON(DB_Object* obj);


void
Serialize_DB_Object_ToListStyle(DB_Object* obj,
                                char* buffer,
                                int32_t indent_level);

char*
Serialize_DB_Object_ToListStyleWrapper(DB_Object* obj);

#endif // __TINY_DB_UTILS