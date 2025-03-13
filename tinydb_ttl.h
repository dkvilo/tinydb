#ifndef __TINY_DB_TTL_H
#define __TINY_DB_TTL_H

#include "tinydb_context.h"
#include "tinydb_database.h"
#include <stdint.h>
#include <time.h>

int32_t
Set_TTL(Database* db, const char* key, int64_t seconds);

int64_t
Get_TTL(Database* db, const char* key);

int
Check_Expiry(DatabaseEntry* entry);

int32_t
Cleanup_Expired_Keys(Database* db);

int32_t
Start_TTL_Cleanup(RuntimeContext* ctx, int interval_seconds);

int32_t
Stop_TTL_Cleanup(RuntimeContext* ctx);

int32_t
Set_TTL_Cleanup_Interval(RuntimeContext* ctx, int interval_seconds);

#endif // __TINY_DB_TTL_H