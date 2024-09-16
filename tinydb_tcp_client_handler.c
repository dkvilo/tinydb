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

#define INITIAL_BUFFER_SIZE COMMAND_BUFFER_SIZE
#define BUFFER_INCREMENT COMMAND_BUFFER_SIZE

extern RuntimeContext* context;

void
TCP_Client_Handler(void* socket_desc)
{
  if (socket_desc == NULL) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER socket_desc is NULL");
    return;
  }

  int32_t sock = *(int32_t*)socket_desc;
  free(socket_desc);

  size_t buffer_size = INITIAL_BUFFER_SIZE;
  char* buffer = (char*)malloc(buffer_size);
  if (buffer == NULL) {
    DB_Log(DB_LOG_ERROR,
           "TCP_SERVER Failed to allocate initial memory for buffer");
    close(sock);
    return;
  }

  ssize_t read_size;
  size_t total_read = 0;

#if 0
  struct timeval timeout;
  timeout.tv_sec = 5; /* we need to make this configurable. */
  timeout.tv_usec = 0;

  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

  while (1) {
    read_size =
      recv(sock, buffer + total_read, buffer_size - total_read - 1, 0);

    if (read_size <= 0)
      break;

    total_read += read_size;
    buffer[total_read] = '\0';
    buffer[strcspn(buffer, "\r\n")] = '\0';

    ParsedCommand* cmd = Parse_Command(buffer);
    if (cmd != NULL) {
      Execute_Command(sock, cmd, context->Active.db);
      Free_Parsed_Command(cmd);

      // reset the buffer
      total_read = 0;
      memset(buffer, 0, buffer_size);
    } else {
      const char* error_msg = "Invalid command\n";
      if (write(sock, error_msg, strlen(error_msg)) == -1) {
        DB_Log(DB_LOG_ERROR,
               "TCP_SERVER Failed to send error message, closing connection.");
        break;
      }
    }

    // if buffer is almost full, increase the size size
    if (total_read >= buffer_size - 1) {
      buffer_size += BUFFER_INCREMENT;
      char* temp = realloc(buffer, buffer_size);
      if (temp == NULL) {
        DB_Log(DB_LOG_ERROR,
               "TCP_SERVER Failed to reallocate memory for buffer");
        break;
      }
      buffer = temp;
    }
  }

  if (read_size == 0) {
    DB_Log(DB_LOG_WARNING, "TCP_SERVER Client disconnected.");
  } else if (read_size == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER recv failed: %s", strerror(errno));
  }

  free(buffer);
  close(sock);
}