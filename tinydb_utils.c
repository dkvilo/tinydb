#include "tinydb_utils.h"
#include "tinydb_hashmap.h"
#include "tinydb_log.h"

#include <inttypes.h>
#include <stdio.h>

// note (David): this is just format for inspecting the data
void
DB_Utils_Save_To_File(Database* db, const char* filename)
{
  FILE* file = fopen(filename, "w");
  if (file == NULL) {
    DB_Log(DB_LOG_ERROR, "Failed to open file %s", filename);
    return;
  }

  for (int shard_id = 0; shard_id < NUM_SHARDS; shard_id++) {
    DatabaseShard* shard = &db->shards[shard_id];
    pthread_rwlock_wrlock(&shard->rwlock); // acquire write lock for the shard

    fprintf(file, "#TINYDB TEXT FORMAT\n");
    fprintf(file, "#\n");
    fprintf(file, "#SHARD_ID:KEY:VALUE:DATATYPE\n");

    for (size_t i = 0; i < shard->entries->capacity; i++) {
      HashEntry* entry = &shard->entries->entries[i];
      if (entry->is_occupied && !entry->is_deleted) {
        DatabaseEntry* db_entry = (DatabaseEntry*)entry->value;
        if (db_entry->type == DB_ENTRY_STRING) {
          fprintf(file,
                  "%d:%s:%s:STRING\n",
                  shard_id,
                  db_entry->key,
                  db_entry->value.string.value);
        } else if (db_entry->type == DB_ENTRY_NUMBER) {
          fprintf(file,
                  "%d:%s:%" PRId64 ":NUMBER\n",
                  shard_id,
                  db_entry->key,
                  db_entry->value.number.value);
        } else if (db_entry->type == DB_ENTRY_OBJECT) {
          fprintf(
            file,
            "%d:%s:%s:OBJECT\n",
            shard_id,
            db_entry->key,
            "OBJECT_DATA"); // this is not implemented yet, but probably we will
                            // use json to represent complex objects.
        }
      }
    }

    pthread_rwlock_unlock(&shard->rwlock);
  }

  fclose(file);
  DB_Log(DB_LOG_INFO, "Database was exported in TEXT format to %s", filename);
}

void
Append_To_Buffer(char** buffer,
                 const char* str,
                 size_t* buffer_size,
                 size_t* buffer_len)
{
  size_t len = strlen(str);
  if (*buffer_len + len >= *buffer_size) {
    *buffer_size *= 2;
    *buffer = realloc(*buffer, *buffer_size);
  }

  memcpy(*buffer + *buffer_len, str, len);
  *buffer_len += len;
  (*buffer)[*buffer_len] = '\0';
}
