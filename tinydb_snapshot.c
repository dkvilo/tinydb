#include "tinydb_snapshot.h"

#include <stdio.h>
#include <stdlib.h>

void
write_string(FILE* file, const char* str)
{
  if (str == NULL) {
    fprintf(stderr, "Error: Attempting to write NULL string\n");
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
    fprintf(stderr, "Error: NULL context or filename in Export_Snapshot\n");
    return -1;
  }

  FILE* file = fopen(filename, "wb");
  if (!file) {
    fprintf(stderr, "Error: Unable to open file %s for writing\n", filename);
    return -1;
  }

  // Write header
  write_string(file, TINYDB_SIGNATURE);
  write_string(file, TINYDB_VERSION);

  // Write database manager
  fwrite(&ctx->db_manager.num_databases, sizeof(int32_t), 1, file);
  for (int i = 0; i < ctx->db_manager.num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];
    fwrite(&db->ID, sizeof(EntryID), 1, file);

    if (db->name == NULL) {
      fprintf(stderr,
              "Warning: NULL database name for database ID %lu\n",
              (unsigned long)db->ID);
    }
    write_string(file, db->name);

    for (int j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];
      fwrite(&shard->num_entries, sizeof(uint64_t), 1, file);
      for (uint64_t k = 0; k < shard->num_entries; k++) {
        DatabaseEntry* entry = &shard->entries[k];
        if (entry->key == NULL) {
          fprintf(stderr,
                  "Warning: NULL key for entry %lu in shard %d\n",
                  (unsigned long)k,
                  j);
        }
        write_string(file, entry->key);
        fwrite(&entry->type, sizeof(DB_ENTRY_TYPE), 1, file);
        switch (entry->type) {
          case DB_ENTRY_NUMBER:
            fwrite(&entry->value.number.value, sizeof(int64_t), 1, file);
            break;
          case DB_ENTRY_STRING:
            if (entry->value.string.value == NULL) {
              fprintf(
                stderr, "Warning: NULL string value for key %s\n", entry->key);
            }
            write_string(file, entry->value.string.value);
            break;
          case DB_ENTRY_OBJECT:
            fprintf(stderr,
                    "Warning: DB_ENTRY_OBJECT not implemented for key %s\n",
                    entry->key);
            break;
        }
      }
    }
  }

  // Write user manager
  // fwrite(&ctx->user_manager.num_users, sizeof(int32_t), 1, file);
  // for (int i = 0; i < ctx->user_manager.num_users; i++) {
  //     DB_User* user = &ctx->user_manager.users[i];
  //     fwrite(&user->ID, sizeof(EntryID), 1, file);
  //     if (user->name == NULL) {
  //         fprintf(stderr, "Warning: NULL username for user ID %lu\n",
  //         (unsigned long)user->ID);
  //     }
  //     write_string(file, user->name);
  //     fwrite(user->password, sizeof(char), 32, file);
  //     if (user->access == NULL) {
  //         fprintf(stderr, "Warning: NULL access for user ID %lu\n", (unsigned
  //         long)user->ID);
  //         // Write dummy values
  //         EntryID dummy_db = 0;
  //         DB_ACCESS_LEVEL dummy_acl = DB_READ;
  //         fwrite(&dummy_db, sizeof(EntryID), 1, file);
  //         fwrite(&dummy_acl, sizeof(DB_ACCESS_LEVEL), 1, file);
  //     } else {
  //         fwrite(&user->access->database, sizeof(EntryID), 1, file);
  //         fwrite(&user->access->acl, sizeof(DB_ACCESS_LEVEL), 1, file);
  //     }
  // }

  fclose(file);
  return 0;
}

int32_t
Import_Snapshot(RuntimeContext* ctx, const char* filename)
{
  FILE* file = fopen(filename, "rb");
  if (!file)
    return -1;

  // header
  char* signature = read_string(file);
  char* version = read_string(file);
  if (strcmp(signature, TINYDB_SIGNATURE) != 0 ||
      strcmp(version, TINYDB_VERSION) != 0) {
    free(signature);
    free(version);
    fclose(file);
    return -1;
  }

  free(signature);
  free(version);

  // db manager
  fread(&ctx->db_manager.num_databases, sizeof(int32_t), 1, file);
  ctx->db_manager.databases =
    malloc(sizeof(Database) * ctx->db_manager.num_databases);
  for (int i = 0; i < ctx->db_manager.num_databases; i++) {
    Database* db = &ctx->db_manager.databases[i];
    fread(&db->ID, sizeof(EntryID), 1, file);
    db->name = read_string(file);

    for (int j = 0; j < NUM_SHARDS; j++) {
      DatabaseShard* shard = &db->shards[j];
      fread(&shard->num_entries, sizeof(uint64_t), 1, file);
      shard->entries = malloc(sizeof(DatabaseEntry) * shard->num_entries);
      for (int k = 0; k < shard->num_entries; k++) {
        DatabaseEntry* entry = &shard->entries[k];
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
            // we dont handle objects yet.
            break;
        }
      }
      pthread_mutex_init(&shard->mutex, NULL);
    }
  }

  // user manager
  fread(&ctx->user_manager.num_users, sizeof(int32_t), 1, file);
  ctx->user_manager.users =
    malloc(sizeof(DB_User) * ctx->user_manager.num_users);
  for (int i = 0; i < ctx->user_manager.num_users; i++) {
    DB_User* user = &ctx->user_manager.users[i];
    fread(&user->ID, sizeof(EntryID), 1, file);
    user->name = read_string(file);
    fread(user->password, sizeof(char), 32, file);
    user->access = malloc(sizeof(DB_Access));
    fread(&user->access->database, sizeof(EntryID), 1, file);
    fread(&user->access->acl, sizeof(DB_ACCESS_LEVEL), 1, file);
  }

  fclose(file);
  return 0;
}

void
Print_Runtime_Context(RuntimeContext* ctx)
{
  if (ctx == NULL) {
    printf("RuntimeContext is NULL\n");
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
