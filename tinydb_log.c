#include "tinydb_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

DB_Log_Manager log_manager = { NULL, 0, 0 };

const char*
level_to_str(DB_LOG_LEVEL level)
{
  switch (level) {
    case DB_LOG_INFO:
      return "(INFO)";
      break;

    case DB_LOG_WARNING:
      return "(WARNING)";
      break;

    case DB_LOG_ERROR:
      return "(ERROR)";
      break;

    default:
      break;
  }
}

void
resize_log_array()
{
  if (log_manager.num_logs == log_manager.capacity) {
    log_manager.capacity = log_manager.capacity == 0 ? INITIAL_LOG_CAPACITY
                                                     : log_manager.capacity * 2;
    log_manager.logs =
      realloc(log_manager.logs, log_manager.capacity * sizeof(Log));
    if (!log_manager.logs) {
      fprintf(stderr, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
  }
}

void
DB_Log(DB_LOG_LEVEL log_lvl, char* fmt, ...)
{
  resize_log_array();

  va_list args;
  va_start(args, fmt);

  Log* log_entry = &log_manager.logs[log_manager.num_logs];
  log_entry->level = log_lvl;
  log_entry->timestamp = time(NULL);

  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  log_entry->text = strdup(buffer);

  char time_buffer[20];
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&log_entry->timestamp));

  printf("%s %s %s\n", time_buffer, level_to_str(log_entry->level), log_entry->text);
  fflush(stdout);

  log_manager.num_logs++;
  va_end(args);
}

void
DB_Free_Logs()
{
  for (int i = 0; i < log_manager.num_logs; i++) {
    free(log_manager.logs[i].text);
  }
  free(log_manager.logs);
}
