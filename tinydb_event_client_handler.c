/**
 * TinyDB Event-based Client Handler Implementation
 * Handles client connections using the event-based TCP server
 */
#include "tinydb_event_client_handler.h"
#include "tinydb_command_executor.h"
#include "tinydb_context.h"
#include "tinydb_log.h"
#include "tinydb_query_parser.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

extern RuntimeContext* context;

bool
EventClientHandler_Init()
{
  return true;
}

void
EventClientHandler_Cleanup()
{
}

void
EventClientHandler_OnConnect(EventTcpClient* client)
{
  if (!client) {
    return;
  }

  DB_Log(DB_LOG_INFO, "Client connected: fd=%d", client->fd);
}

void
EventClientHandler_OnData(EventTcpClient* client)
{
  if (!client) {
    return;
  }

  client->buffer[client->data_len] = '\0';
  client->buffer[strcspn(client->buffer, "\r\n")] = '\0';

  ParsedCommand* cmd =
    Parse_Command(client->buffer, client->buffer_size, &client->data_len);
  if (cmd != NULL) {
    Execute_Command(client->fd, cmd, context->Active.db);
    Free_Parsed_Command(cmd);
  } else {
    const char* error_msg = "Invalid command\n";
    EventTcpClient_Send(client, error_msg, strlen(error_msg));
  }

  client->data_len = 0;
}

void
EventClientHandler_OnDisconnect(EventTcpClient* client)
{
  if (!client) {
    return;
  }

  DB_Log(DB_LOG_INFO, "Client disconnected: fd=%d", client->fd);
}