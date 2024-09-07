#include <stdio.h>
#include <stdlib.h>

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
read_string(FILE* file)
{
  uint32_t len;
  fread(&len, sizeof(uint32_t), 1, file);
  char* str = malloc(len + 1);
  fread(str, 1, len, file);
  str[len] = '\0';
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
  if (ctx == NULL || filename == NULL) {
    fprintf(stderr, "Error: \n");
    DB_Log(DB_LOG_ERROR, "NULL context or filename in Import_Snapshot");
    return -1;
  }

  FILE* file = fopen(filename, "rb");
  if (!file) {
    DB_Log(DB_LOG_ERROR, "Unable to open file %s for reading\n", filename);
    return -1;
  }

  // read and verify header
  char* signature = read_string(file);
  char* version = read_string(file);
  if (strcmp(signature, TINYDB_SIGNATURE) != 0 ||
      strcmp(version, TINYDB_VERSION) != 0) {
    DB_Log(DB_LOG_ERROR, "Invalid file signature or version\n");
    free(signature);
    free(version);
    fclose(file);
    return -1;
  }
  free(signature);
  free(version);

  int32_t num_databases;
  fread(&num_databases, sizeof(int32_t), 1, file);

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
    fclose(file);
    return -1;
  }

  for (int i = 0; i < num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];
    fread(&db->ID, sizeof(EntryID), 1, file);
    db->name = read_string(file);

    for (int j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];
      fread(&shard->num_entries, sizeof(uint64_t), 1, file);
      shard->entries = HM_Create();
      if (!shard->entries) {
        DB_Log(DB_LOG_ERROR, "Failed to create hash map for shard %d", j);
        fclose(file);
        return -1;
      }

      for (uint64_t k = 0; k < shard->num_entries; k++) {
        DatabaseEntry* entry = malloc(sizeof(DatabaseEntry));
        if (!entry) {
          DB_Log(DB_LOG_ERROR, "Failed to allocate memory for database entry");
          fclose(file);
          return -1;
        }

        entry->key = read_string(file);
        fread(&entry->type, sizeof(DB_ENTRY_TYPE), 1, file);

        switch (entry->type) {
          case DB_ENTRY_NUMBER:
            fread(&entry->value.number.value, sizeof(int64_t), 1, file);
            break;
          case DB_ENTRY_STRING:
            entry->value.string.value = read_string(file);
            break;
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
        fclose(file);
        return -1;
      }
    }
  }

  // UserManager
  int32_t num_users;
  fread(&num_users, sizeof(int32_t), 1, file);

  // free existing users if there was any, this should not be case, since we are
  // importing data on start
  if (ctx->user_manager.users) {
    for (int i = 0; i < ctx->user_manager.num_users; i++) {
      free(ctx->user_manager.users[i].name);
      free(ctx->user_manager.users[i].access);
    }
    free(ctx->user_manager.users);
  }

  ctx->user_manager.num_users = num_users;
  ctx->user_manager.users = malloc(sizeof(DB_User) * num_users);
  if (!ctx->user_manager.users) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for users");
    fclose(file);
    return -1;
  }

  for (int i = 0; i < num_users; i++) {
    DB_User* user = &ctx->user_manager.users[i];
    fread(&user->ID, sizeof(EntryID), 1, file);
    user->name = read_string(file);
    fread(user->password, sizeof(char), 32, file);

    uint8_t has_access;
    fread(&has_access, sizeof(uint8_t), 1, file);
    if (has_access) {
      user->access = malloc(sizeof(DB_Access));
      if (!user->access) {
        DB_Log(DB_LOG_ERROR, "Failed to allocate memory for user access");
        fclose(file);
        return -1;
      }
      fread(&user->access->database, sizeof(EntryID), 1, file);
      fread(&user->access->acl, sizeof(DB_ACCESS_LEVEL), 1, file);
    } else {
      user->access = NULL;
    }
  }

  fclose(file);
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
