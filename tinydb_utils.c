#include "tinydb_utils.h"
#include "tinydb_log.h"

#include <stdio.h>
#include "external/stb_ds.h"

void
DB_Utils_Save_To_File(Database* db, const char* filename)
{
  FILE* file = fopen(filename, "w");

  if (file == NULL) {
    DB_Log(DB_LOG_ERROR, "FS Failed to open file %s", filename);
    return;
  }

  for (int shard_id = 0; shard_id < NUM_SHARDS; shard_id++) {
    DatabaseShard* shard = &db->shards[shard_id];
    pthread_mutex_lock(&shard->mutex);

    fprintf(file,"#TINYDB TEXT FORMAT\n");
    fprintf(file,"#\n");
    fprintf(file,"#SHARD_ID:KEY:VALUE:DATATYPE\n");

    for (int i = 0; i < hmlen(shard->entries); i++) {
      DatabaseEntry* entry = &shard->entries[i];

      if (entry->type == DB_ENTRY_STRING) {
        fprintf(file,"%d:%s:%s:STRING\n",
                shard_id,
                entry->key,
                entry->value.string.value);
      } else if (entry->type == DB_ENTRY_NUMBER) {
        fprintf(file,"%d:%s:%ld:NUMBER\n",
                shard_id,
                entry->key,
                entry->value.number.value);
      } else if (entry->type == DB_ENTRY_OBJECT) {
        fprintf(file,"%d:%s:%s:NUMBER\n", shard_id, entry->key, "EMPTY");
      }
    }

    pthread_mutex_unlock(&shard->mutex);
  }

  fclose(file);
  DB_Log(DB_LOG_INFO, "FS Database was exported in TEXT format to %s", filename);
}
