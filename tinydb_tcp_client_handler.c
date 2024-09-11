#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "tinydb_command_executor.h"
#include "tinydb_context.h"
#include "tinydb_database.h"
#include "tinydb_log.h"
#include "tinydb_query_parser.h"
#include "tinydb_tcp_client_handler.h"

extern Database* db;
extern RuntimeContext* context;

void
TCP_Client_Handler(void* socket_desc)
{
  int32_t sock = *(int32_t*)socket_desc;
  char buffer[COMMAND_BUFFER_SIZE];
  ssize_t read_size;

  while (1) {

    memset(buffer, 0, COMMAND_BUFFER_SIZE);
    read_size = recv(sock, buffer, COMMAND_BUFFER_SIZE - 1, 0);

    if (read_size <= 0)
      break;

    buffer[read_size] = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0;

    ParsedCommand* cmd = Parse_Command(buffer);
    if (cmd != NULL) {
      Execute_Command(sock, cmd, db);
      Free_Parsed_Command(cmd);
    } else {
      const char* error_msg = "Invalid command\n";
      if (write(sock, error_msg, strlen(error_msg)) == -1) {
        DB_Log(DB_LOG_ERROR,
               "TCP_SERVER Failed to send error message, closing connection.");
        break;
      }
    }
  }

  if (read_size == 0) {
    DB_Log(DB_LOG_WARNING, "TCP_SERVER Client disconnected.");
  } else if (read_size == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER recv failed: %s", strerror(errno));
  }

  close(sock);
  free(socket_desc);
}