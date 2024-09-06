#ifndef __TINY_DB_SNAPSHOT
#define __TINY_DB_SNAPSHOT

#include "tinydb.h"

#define TINYDB_SIGNATURE "TINYDB"
#define TINYDB_VERSION "0.0.1"

int32_t
Export_Snapshot(RuntimeContext* ctx, const char* filename);

int32_t
Import_Snapshot(RuntimeContext* ctx, const char* filename);

void
Print_Runtime_Context(RuntimeContext* ctx);

#endif // __TINY_DB_SNAPSHOT