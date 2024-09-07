#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "tinydb.h"
#include "tinydb_log.h"
#include "tinydb_tcp_client_handler.h"
#include "tinydb_query_parser.h"
#include "tinydb_command_executor.h"
#include "config.h"

extern Database* db;
extern RuntimeContext* context;

void
TCP_Client_Handler(void* socket_desc)
{
  int sock = *(int*)socket_desc;
  char buffer[COMMAND_BUFFER_SIZE];
  int read_size;
  
#if defined(SEND_PROMPT)
  const char* prompt = "> ";
  if (write(sock, prompt, strlen(prompt)) == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER Failed to send prompt, closing connection.");
    close(sock);
    free(socket_desc);
    return;
  }
#endif

  while ((read_size = recv(sock, buffer, COMMAND_BUFFER_SIZE, 0)) > 0) {
    buffer[strcspn(buffer, "\r\n")] = 0; // newline
    ParsedCommand* cmd = Parse_Command(buffer);

    if (cmd != NULL) {
      Execute_Command(sock, cmd, db);
      Free_Parsed_Command(cmd);
    } else {
      write(sock, "Invalid command\n", strlen("Invalid command\n"));
    }

    memset(buffer, 0, COMMAND_BUFFER_SIZE);
#if defined(SEND_PROMPT)
    if (write(sock, prompt, strlen(prompt)) == -1) {
      DB_Log(DB_LOG_ERROR, "TCP_SERVER Failed to send prompt, closing connection.");
      break;
    }
#endif

  }

  if (read_size == 0) {
    DB_Log(DB_LOG_WARNING, "TCP_SERVER Client disconnected.");
  } else if (read_size == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER recv failed: %s", strerror(errno));
  }

  close(sock);
  free(socket_desc);
}
