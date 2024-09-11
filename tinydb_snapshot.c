#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tinydb_log.h"
#include "tinydb_snapshot.h"

void
write_string(FILE* file, const char* str)
{
  if (str == NULL) {
    DB_Log(DB_LOG_ERROR, "Attempting to write NULL string");
    uint32_t len = 0;
    fwrite(&len, sizeof(uint32_t), 1, file);
    return;
  }

  uint32_t len = strlen(str);
  fwrite(&len, sizeof(uint32_t), 1, file);
  fwrite(str, 1, len, file);
}

char*
read_string_mmap(char** ptr, char* end_of_mapped_region)
{
  // make sure that we have enough space to read the length
  if (*ptr + sizeof(uint32_t) > end_of_mapped_region) {
    DB_Log(DB_LOG_ERROR, "Invalid memory access: string length exceeds mmap");
    return NULL;
  }

  uint32_t len = *(uint32_t*)(*ptr); // read the length
  *ptr += sizeof(uint32_t); // move the pointer forward by the size of uint32_t

  // check if the length is reasonable size
  if (len > MAX_STRING_LENGTH) {
    DB_Log(DB_LOG_ERROR, "Invalid string length: %u", len);
    return NULL;
  }

  if (len == 0) {
    return NULL;
  }

  // make sure that we have enough space to read the string data
  if (*ptr + len > end_of_mapped_region) {
    DB_Log(DB_LOG_ERROR, "Invalid memory access: string content exceeds mmap");
    return NULL;
  }

  char* str = malloc(len + 1);
  memcpy(str, *ptr, len); // copy the content
  str[len] = '\0';
  *ptr += len; // move the pointer forward by the length of the string
  return str;
}

int32_t
Export_Snapshot(RuntimeContext* ctx, const char* filename)
{
  if (ctx == NULL || filename == NULL) {
    DB_Log(DB_LOG_ERROR, "NULL context or filename in Export_Snapshot");
    return -1;
  }

  FILE* file = fopen(filename, "wb");
  if (!file) {
    DB_Log(DB_LOG_ERROR, "Unable to open file %s for writing %s", filename);
    return -1;
  }

  // header
  write_string(file, TINYDB_SIGNATURE);
  write_string(file, TINYDB_VERSION);

  // DatabaseManager
  fwrite(&ctx->db_manager.num_databases, sizeof(int32_t), 1, file);
  for (int i = 0; i < ctx->db_manager.num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];
    fwrite(&db->ID, sizeof(EntryID), 1, file);
    write_string(file, db->name);

    for (int j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];
      fwrite(&shard->num_entries, sizeof(uint64_t), 1, file);
      for (size_t k = 0; k < shard->entries->capacity; k++) {
        HashEntry* hash_entry = &shard->entries->entries[k];
        if (hash_entry->is_occupied && !hash_entry->is_deleted) {
          DatabaseEntry* entry = (DatabaseEntry*)hash_entry->value;
          write_string(file, entry->key);
          fwrite(&entry->type, sizeof(DB_ENTRY_TYPE), 1, file);

          switch (entry->type) {
            case DB_ENTRY_NUMBER:
              fwrite(&entry->value.number.value, sizeof(int64_t), 1, file);
              break;
            case DB_ENTRY_STRING:
              write_string(file, entry->value.string.value);
              break;
            case DB_ENTRY_LIST: {
              HPLinkedList* list = entry->value.list;
              fwrite(&list->count, sizeof(size_t), 1, file); // list size

              ListNode* current = list->head;
              while (current) {
                // node type
                fwrite(&current->type, sizeof(ValueType), 1, file);

                // node value based on its type
                switch (current->type) {
                  case TYPE_STRING:
                    write_string(file, current->value.string_value);
                    break;
                  case TYPE_INT:
                    fwrite(&current->value.int_value, sizeof(int64_t), 1, file);
                    break;
                  case TYPE_FLOAT:
                    fwrite(
                      &current->value.float_value, sizeof(double), 1, file);
                    break;
                }

                current = current->next;
              }
            } break;
            case DB_ENTRY_OBJECT:
              DB_Log(DB_LOG_WARNING,
                     "DB_ENTRY_OBJECT not implemented for key %s",
                     entry->key);
              break;
          }
        }
      }
    }
  }

  // UserManager
  fwrite(&ctx->user_manager.num_users, sizeof(int32_t), 1, file);
  for (int i = 0; i < ctx->user_manager.num_users; i++) {
    DB_User* user = &ctx->user_manager.users[i];
    fwrite(&user->ID, sizeof(EntryID), 1, file);
    write_string(file, user->name);
    fwrite(user->password, sizeof(char), 32, file);

    // Access information
    if (user->access) {
      uint8_t has_access = 1;
      fwrite(&has_access, sizeof(uint8_t), 1, file);
      fwrite(&user->access->database, sizeof(EntryID), 1, file);
      fwrite(&user->access->acl, sizeof(DB_ACCESS_LEVEL), 1, file);
    } else {
      uint8_t has_access = 0;
      fwrite(&has_access, sizeof(uint8_t), 1, file);
    }
  }

  fclose(file);
  return 0;
}

int32_t
Import_Snapshot(RuntimeContext* ctx, const char* filename)
{
  DB_Log(DB_LOG_INFO, "Trying to open file %s for reading", filename);

  if (ctx == NULL || filename == NULL) {
    fprintf(stderr, "Error: \n");
    DB_Log(DB_LOG_ERROR, "NULL context or filename in Import_Snapshot");
    return -1;
  }

  int32_t fd = open(filename, O_RDONLY);
  if (fd < 0) {
    DB_Log(DB_LOG_ERROR, "Unable to open file %s for reading", filename);
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    DB_Log(DB_LOG_ERROR, "Failed to get file size for %s", filename);
    close(fd);
    return -1;
  }

  // map the file
  char* data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data == MAP_FAILED) {
    DB_Log(DB_LOG_ERROR, "Failed to mmap file %s", filename);
    close(fd);
    return -1;
  }

  char* ptr = data;
  char* end_of_mapped_region =
    data + st.st_size; // calculate the end of the region

  // read and verify header
  char* signature = read_string_mmap(&ptr, end_of_mapped_region);
  char* version = read_string_mmap(&ptr, end_of_mapped_region);

  if (strcmp(signature, TINYDB_SIGNATURE) != 0 ||
      strcmp(version, TINYDB_VERSION) != 0) {
    DB_Log(DB_LOG_ERROR, "Invalid file signature or version");
    free(signature);
    free(version);
    munmap(data, st.st_size);
    close(fd);
    return -1;
  }

  DB_Log(
    DB_LOG_INFO, "Importing TinyDB snapshot for version %s", TINYDB_VERSION);

  free(signature);
  free(version);

  // number of databases
  int32_t num_databases = *(int32_t*)ptr;
  ptr += sizeof(int32_t);

  if (ptr > end_of_mapped_region) {
    DB_Log(DB_LOG_ERROR, "Reached beyond the end of the mapped file.");
    munmap(data, st.st_size);
    close(fd);
    return -1;
  }

  // clean up old databases
  if (ctx->db_manager.databases) {
    for (int i = 0; i < ctx->db_manager.num_databases; i++) {
      free(ctx->db_manager.databases[i].name);
      for (int j = 0; j < NUM_SHARDS; j++) {
        HM_Destroy(ctx->db_manager.databases[i].shards[j].entries);
        pthread_rwlock_destroy(&ctx->db_manager.databases[i].shards[j].rwlock);
      }
    }
    free(ctx->db_manager.databases);
  }

  ctx->db_manager.num_databases = num_databases;
  ctx->db_manager.databases = malloc(sizeof(Database) * num_databases);
  if (!ctx->db_manager.databases) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for databases");
    munmap(data, st.st_size);
    close(fd);
    return -1;
  }

  for (int32_t i = 0; i < num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];

    db->ID = *(EntryID*)ptr;
    ptr += sizeof(EntryID);

    db->name = read_string_mmap(&ptr, end_of_mapped_region);

    for (int32_t j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];

      shard->num_entries = *(uint64_t*)ptr;
      ptr += sizeof(uint64_t);

      shard->entries = HM_Create();
      if (!shard->entries) {
        DB_Log(DB_LOG_ERROR, "Failed to create hash map for shard %d", j);
        munmap(data, st.st_size);
        close(fd);
        return -1;
      }

      for (uint64_t k = 0; k < shard->num_entries; k++) {
        DatabaseEntry* entry = malloc(sizeof(DatabaseEntry));
        if (!entry) {
          DB_Log(DB_LOG_ERROR, "Failed to allocate memory for database entry");
          munmap(data, st.st_size);
          close(fd);
          return -1;
        }

        entry->key = read_string_mmap(&ptr, end_of_mapped_region);

        entry->type = *(DB_ENTRY_TYPE*)ptr;
        ptr += sizeof(DB_ENTRY_TYPE);

        switch (entry->type) {
          case DB_ENTRY_NUMBER:
            entry->value.number.value = *(int64_t*)ptr;
            ptr += sizeof(int64_t);
            break;
          case DB_ENTRY_STRING:
            entry->value.string.value =
              read_string_mmap(&ptr, end_of_mapped_region);
            break;

          case DB_ENTRY_LIST: {
            size_t list_size = *(size_t*)ptr;
            ptr += sizeof(size_t);

            HPLinkedList* list = HPList_Create();
            for (size_t n = 0; n < list_size; n++) {
              ValueType node_type = *(ValueType*)ptr;
              ptr += sizeof(ValueType);

              switch (node_type) {
                case TYPE_STRING: {
                  char* str_value =
                    read_string_mmap(&ptr, end_of_mapped_region);
                  HPList_RPush_String(list, str_value);
                  free(str_value);
                } break;
                case TYPE_INT: {
                  int64_t int_value = *(int64_t*)ptr;
                  ptr += sizeof(int64_t);
                  HPList_RPush_Int(list, int_value);
                } break;
                case TYPE_FLOAT: {
                  double float_value = *(double*)ptr;
                  ptr += sizeof(double);
                  HPList_RPush_Float(list, float_value);
                } break;
              }
            }
            entry->value.list = list;
          } break;
          case DB_ENTRY_OBJECT:
            DB_Log(DB_LOG_WARNING,
                   "DB_ENTRY_OBJECT not implemented for key %s",
                   entry->key);
            break;
        }

        HM_Put(shard->entries, entry->key, entry);
      }

      if (pthread_rwlock_init(&shard->rwlock, NULL) != 0) {
        DB_Log(DB_LOG_ERROR, "Failed to initialize rwlock for shard %d", j);
        munmap(data, st.st_size);
        close(fd);
        return -1;
      }
    }
  }

  // users
  int32_t num_users = *(int32_t*)ptr;
  ptr += sizeof(int32_t);

  if (ctx->user_manager.users) {
    for (int32_t i = 0; i < ctx->user_manager.num_users; i++) {
      free(ctx->user_manager.users[i].name);
      free(ctx->user_manager.users[i].access);
    }
    free(ctx->user_manager.users);
  }

  ctx->user_manager.num_users = num_users;
  ctx->user_manager.users = malloc(sizeof(DB_User) * num_users);
  if (!ctx->user_manager.users) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for users");
    munmap(data, st.st_size);
    close(fd);
    return -1;
  }

  for (int32_t i = 0; i < num_users; i++) {
    DB_User* user = &ctx->user_manager.users[i];

    user->ID = *(EntryID*)ptr;
    ptr += sizeof(EntryID);

    user->name = read_string_mmap(&ptr, end_of_mapped_region);

    memcpy(user->password, ptr, 32);
    ptr += 32;

    uint8_t has_access = *(uint8_t*)ptr;
    ptr += sizeof(uint8_t);

    if (has_access) {
      user->access = malloc(sizeof(DB_Access));
      if (!user->access) {
        DB_Log(DB_LOG_ERROR, "Failed to allocate memory for user access");
        munmap(data, st.st_size);
        close(fd);
        return -1;
      }

      user->access->database = *(EntryID*)ptr;
      ptr += sizeof(EntryID);
      user->access->acl = *(DB_ACCESS_LEVEL*)ptr;
      ptr += sizeof(DB_ACCESS_LEVEL);
    } else {
      user->access = NULL;
    }
  }

  munmap(data, st.st_size);
  close(fd);
  return 0;
}

void
Print_Runtime_Context(RuntimeContext* ctx)
{
  if (ctx == NULL) {
    DB_Log(DB_LOG_ERROR, "RuntimeContext is NULL");
    return;
  }

  printf("DatabaseManager:\n");
  printf("  Number of databases: %d\n", ctx->db_manager.num_databases);
  for (int i = 0; i < ctx->db_manager.num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];
    printf("  Database %d:\n", i);
    printf("    ID: %lu\n", (unsigned long)db->ID);
    printf("    Name: %s\n", db->name ? db->name : "NULL");
    for (int j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];
      printf(
        "    Shard %d: %lu entries\n", j, (unsigned long)shard->num_entries);
    }
  }

  printf("UserManager:\n");
  printf("  Number of users: %d\n", ctx->user_manager.num_users);
  for (int i = 0; i < ctx->user_manager.num_users; i++) {
    DB_User* user = &ctx->user_manager.users[i];
    printf("  User %d:\n", i);
    printf("    ID: %lu\n", (unsigned long)user->ID);
    printf("    Name: %s\n", user->name ? user->name : "NULL");
    if (user->access) {
      printf("    Access - Database: %lu, ACL: %d\n",
             (unsigned long)user->access->database,
             user->access->acl);
    } else {
      printf("    Access: NULL\n");
    }
  }

  printf("Active:\n");
  printf("  Active DB: %p\n", (void*)ctx->Active.db);
  printf("  Active User: %p\n", (void*)ctx->Active.user);
}
