#include "tinydb_utils.h"
#include "tinydb_hashmap.h"
#include "tinydb_hashmap_iterator.h"
#include "tinydb_log.h"

#include <stdio.h>
#include <inttypes.h>

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

void
Escape_String(char* dest, const char* src)
{
  while (*src) {
    if (*src == '"' || *src == '\\') {
      *dest++ = '\\';
    }
    *dest++ = *src++;
  }
  *dest = '\0';
}

char*
Serialize_DB_Object_ToJSON(DB_Object* obj)
{
  char* json = (char*)malloc(4096);
  strcpy(json, "{");

  HashMapIterator it = HM_Iterator(obj->fields);
  int32_t first = 1;

  while (HM_IteratorHasNext(&it)) {
    DatabaseEntry* entry = HM_IteratorNext(&it);

    if (!first) {
      strcat(json, ", ");
    }
    first = 0;

    char escaped_key[256];
    Escape_String(escaped_key, entry->key);
    strcat(json, "\"");
    strcat(json, escaped_key);
    strcat(json, "\": ");

    if (entry->type == DB_ENTRY_STRING) {
      char escaped_value[1024];
      Escape_String(escaped_value, entry->value.string.value);
      strcat(json, "\"");
      strcat(json, escaped_value);
      strcat(json, "\"");
    } else if (entry->type == DB_ENTRY_NUMBER) {
      char number[64];
      snprintf(number,
               sizeof(number),
               "%" PRId64,
               atomic_load(&entry->value.number.value));
      strcat(json, number);
    } else if (entry->type == DB_ENTRY_LIST) {
      strcat(json, "[");
      HPLinkedList* list = entry->value.list;
      ListNode* current_node = list->head;
      int first_in_list = 1;
      while (current_node != NULL) {
        if (!first_in_list) {
          strcat(json, ", ");
        }
        first_in_list = 0;

        if (current_node->type == TYPE_STRING) {
          char escaped_list_value[1024];
          Escape_String(escaped_list_value, current_node->value.string_value);
          strcat(json, "\"");
          strcat(json, escaped_list_value);
          strcat(json, "\"");
        } else if (current_node->type == TYPE_INT) {
          char list_number[64];
          snprintf(list_number,
                   sizeof(list_number),
                   "%" PRId64,
                   current_node->value.int_value);
          strcat(json, list_number);
        }
        current_node = current_node->next;
      }
      strcat(json, "]");
    } else if (entry->type == DB_ENTRY_OBJECT) {
      char* nested_json = Serialize_DB_Object_ToJSON(entry->value.object);
      strcat(json, nested_json);
      free(nested_json);
    }
  }

  strcat(json, "}");
  return json;
}

void
Add_Indentation(char* dest, int32_t level)
{
  for (int32_t i = 0; i < level; i++) {
    strcat(dest, "  ");
  }
}

void
Serialize_DB_Object_ToListStyle(DB_Object* obj, char* buffer, int32_t indent_level)
{
  HashMapIterator it = HM_Iterator(obj->fields);
  while (HM_IteratorHasNext(&it)) {
    DatabaseEntry* entry = HM_IteratorNext(&it);

    Add_Indentation(buffer, indent_level);

    strcat(buffer, entry->key);
    strcat(buffer, ": ");

    if (entry->type == DB_ENTRY_STRING) {
      strcat(buffer, entry->value.string.value);
      strcat(buffer, "\n");
    } else if (entry->type == DB_ENTRY_NUMBER) {
      char number[64];
      snprintf(number,
               sizeof(number),
               "%" PRId64,
               atomic_load(&entry->value.number.value));
      strcat(buffer, number);
      strcat(buffer, "\n");
    } else if (entry->type == DB_ENTRY_LIST) {
      strcat(buffer, "\n");
      HPLinkedList* list = entry->value.list;
      ListNode* current = list->head;
      while (current != NULL) {
        Add_Indentation(buffer, indent_level + 1);
        if (current->type == TYPE_STRING) {
          strcat(buffer, current->value.string_value);
        } else if (current->type == TYPE_INT) {
          char list_number[64];
          snprintf(list_number,
                   sizeof(list_number),
                   "%" PRId64,
                   current->value.int_value);
          strcat(buffer, list_number);
        }
        strcat(buffer, "\n");
        current = current->next;
      }
    } else if (entry->type == DB_ENTRY_OBJECT) {
      strcat(buffer, "\n");
      Serialize_DB_Object_ToListStyle(
        entry->value.object, buffer, indent_level + 1);
    }
  }
}

char*
Serialize_DB_Object_ToListStyleWrapper(DB_Object* obj)
{
  char* buffer = (char*)malloc(4096);
  buffer[0] = '\0';

  Serialize_DB_Object_ToListStyle(obj, buffer, 0);
  return buffer;
}
