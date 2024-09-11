#ifndef __TINY_DB_ATOMIC_PROC
#define __TINY_DB_ATOMIC_PROC

#include "tinydb_database.h"

void
DB_Atomic_Store(Database* db,
                const char* key,
                DB_Value value,
                DB_ENTRY_TYPE type);

DatabaseEntry
DB_Atomic_Get(Database* db, const char* key);

int64_t
DB_Atomic_Incr(Database* db, const char* key);

#endif // __TINY_DB_ATOMIC_PROC