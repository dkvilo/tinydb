#ifndef __TINY_DB_LOG
#define __TINY_DB_LOG

#include <stdint.h>
#include <time.h>

typedef enum DB_LOG_LEVEL
{
  DB_LOG_INFO = 1 << 1,
  DB_LOG_WARNING = 2 << 1,
  DB_LOG_ERROR = 3 << 1
} DB_LOG_LEVEL;

#define INITIAL_LOG_CAPACITY 10

typedef struct
{
  DB_LOG_LEVEL level;
  char* text;
  time_t timestamp;
} Log;

typedef struct DB_Log_Manager
{
  Log* logs;
  int32_t num_logs;
  int32_t capacity;
} DB_Log_Manager;

void
DB_Log(DB_LOG_LEVEL log_lvl, char* fmt, ...);

void
DB_Free_Logs();

#endif // __TINY_DB_LOG