#include "tinydb_log.h"
#include "tinydb_tcp_server.h"
#include "tinydb.h"
#include "tinydb_utils.h"
#include "tinydb_snapshot.h"

extern Database *db;
extern RuntimeContext *context;

// note (David): this is temporary
void
parse_command(const char* input, char* command, char* key, char* value)
{
  sscanf(input, "%s %s %s", command, key, value);
}

void
client_handler(void* socket_desc)
{
  int sock = *(int*)socket_desc;
  char buffer[BUFFER_SIZE];
  int read_size;

  const char* prompt = "(127.0.0.1)> ";
  write(sock, prompt, strlen(prompt));

  while ((read_size = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
    buffer[strcspn(buffer, "\r\n")] = 0;

    char command[10], key[255], value[255];
    parse_command(buffer, command, key, value);

    if (strcmp(command, "SET") == 0) {
      char* value_copy = strdup(value);

      if (value_copy == NULL) {
        DB_Log(DB_LOG_ERROR, "TCP_SERVER Could not allocate memory, value was not copied.");
        continue;
      }

      DB_Value val_def = { .string = { value_copy } };
      DB_Atomic_Store(db, key, val_def, DB_ENTRY_STRING);

      write(sock, "Ok\n", strlen("Ok\n"));
    }

    else if (strcmp(command, "GET") == 0) {
      DB_Value val = DB_Atomic_Get(db, key, DB_ENTRY_STRING);

      char payload[255];
      sprintf(payload, "%d,%s\n", DB_ENTRY_STRING, val.string.value);
      write(sock, payload, strlen((const char*)payload));
    }

    else if (strcmp(command, "EXP") == 0) {
      DB_Utils_Save_To_File(db, key);
      Export_Snapshot(context, "snapshot.bin");
    }

    else if (strcmp(command, "LOAD") == 0)
    {
      Import_Snapshot(context, "snapshot.bin");
    }

    else if (strcmp(command, "INSPECT") == 0)
    {
      Print_Runtime_Context(context); 
    }

    memset(buffer, 0, BUFFER_SIZE);
    write(sock, prompt, strlen(prompt));
  }

  if (read_size == 0) {
    DB_Log(DB_LOG_WARNING, "TCP_SERVER Client disconnected");
  }

  else if (read_size == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER recv failed.");
  }

  close(sock);
  free(socket_desc);
}
