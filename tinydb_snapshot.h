#ifndef __TINY_DB_SNAPSHOT_H
#define __TINY_DB_SNAPSHOT_H

#include "tinydb_context.h"
#include <stdint.h>

#define TINYDB_SIGNATURE "TINYDB"
#define TINYDB_VERSION "0.0.1"

int32_t
Export_Snapshot(RuntimeContext* ctx, const char* filename);

int32_t
Import_Snapshot(RuntimeContext* ctx, const char* filename);

void
Print_Runtime_Context(RuntimeContext* ctx);

int32_t
Start_Background_Snapshot(RuntimeContext* ctx,
                          int interval_seconds,
                          const char* filename);

int32_t
Stop_Background_Snapshot(RuntimeContext* ctx);

int32_t
Set_Snapshot_Interval(RuntimeContext* ctx, int interval_seconds);

#endif // __TINY_DB_SNAPSHOT_H