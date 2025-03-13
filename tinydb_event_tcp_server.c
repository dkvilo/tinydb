/**
 * TinyDB Event-based TCP Server Implementation
 * Provides a non-blocking TCP server implementation using the event queue
 * abstraction
 */
#include "tinydb_event_tcp_server.h"
#include "config.h"
#include "tinydb_log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* initial buffer size for client connections */
#define INITIAL_BUFFER_SIZE COMMAND_BUFFER_SIZE

static bool
set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to get socket flags: %s", strerror(errno));
    return false;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    DB_Log(DB_LOG_ERROR,
           "Failed to set socket to non-blocking mode: %s",
           strerror(errno));
    return false;
  }

  return true;
}

static bool
set_reuseaddr(int fd)
{
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    DB_Log(DB_LOG_ERROR,
           "Failed to set socket to reuse address: %s",
           strerror(errno));
    return false;
  }

  return true;
}

static void
handle_accept_event(EventTcpServer* server)
{
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  int client_fd =
    accept(server->fd, (struct sockaddr*)&client_addr, &client_addr_len);
  if (client_fd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      DB_Log(DB_LOG_ERROR, "Failed to accept connection: %s", strerror(errno));
    }
    return;
  }

  if (!set_nonblocking(client_fd)) {
    close(client_fd);
    return;
  }

  EventTcpClient* client = (EventTcpClient*)malloc(sizeof(EventTcpClient));
  if (!client) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for client");
    close(client_fd);
    return;
  }

  client->fd = client_fd;
  client->addr = client_addr;
  client->addr_len = client_addr_len;
  client->buffer = (char*)malloc(INITIAL_BUFFER_SIZE);
  if (!client->buffer) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for client buffer");
    free(client);
    close(client_fd);
    return;
  }
  client->buffer_size = INITIAL_BUFFER_SIZE;
  client->data_len = 0;

  if (!EventQueue_Add(&server->event_queue, client_fd, EVENT_READ, client)) {
    DB_Log(DB_LOG_ERROR, "Failed to add client socket to event queue");
    free(client->buffer);
    free(client);
    close(client_fd);
    return;
  }

  DB_Log(DB_LOG_INFO,
         "Accepted connection from %s:%d",
         inet_ntoa(client_addr.sin_addr),
         ntohs(client_addr.sin_port));

  if (server->on_client_connect) {
    server->on_client_connect(client);
  }
}

static void
handle_client_read_event(EventTcpServer* server, EventTcpClient* client)
{
  if (client->data_len >= client->buffer_size - 1) {
    size_t new_size = client->buffer_size * 2;
    char* new_buffer = (char*)realloc(client->buffer, new_size);
    if (!new_buffer) {
      DB_Log(DB_LOG_ERROR, "Failed to resize client buffer");
      EventTcpClient_Close(client);
      return;
    }
    client->buffer = new_buffer;
    client->buffer_size = new_size;
  }

  ssize_t bytes_read = recv(client->fd,
                            client->buffer + client->data_len,
                            client->buffer_size - client->data_len - 1,
                            0);

  if (bytes_read <= 0) {
    if (bytes_read == 0 || errno != EAGAIN) {
      if (bytes_read == 0) {
        DB_Log(DB_LOG_INFO, "Client disconnected");
      } else {
        DB_Log(DB_LOG_ERROR, "Error reading from client: %s", strerror(errno));
      }

      if (server->on_client_disconnect) {
        server->on_client_disconnect(client);
      }

      EventTcpClient_Close(client);
    }
    return;
  }

  client->data_len += bytes_read;
  client->buffer[client->data_len] = '\0';

  if (server->on_client_data) {
    server->on_client_data(client);
  }
}

static void
handle_client_error_event(EventTcpServer* server, EventTcpClient* client)
{
  DB_Log(DB_LOG_ERROR, "Error on client socket");
  if (server->on_client_disconnect) {
    server->on_client_disconnect(client);
  }

  EventTcpClient_Close(client);
}

bool
EventTcpServer_Init(EventTcpServer* server, int port)
{
  if (!server) {
    DB_Log(DB_LOG_ERROR, "Invalid server pointer");
    return false;
  }

  memset(server, 0, sizeof(EventTcpServer));
  server->port = port;

  server->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->fd == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to create server socket: %s", strerror(errno));
    return false;
  }

  if (!set_nonblocking(server->fd)) {
    close(server->fd);
    return false;
  }

  if (!set_reuseaddr(server->fd)) {
    close(server->fd);
    return false;
  }

  server->addr.sin_family = AF_INET;
  server->addr.sin_addr.s_addr = INADDR_ANY;
  server->addr.sin_port = htons(port);

  if (bind(server->fd, (struct sockaddr*)&server->addr, sizeof(server->addr)) ==
      -1) {
    DB_Log(DB_LOG_ERROR, "Failed to bind server socket: %s", strerror(errno));
    close(server->fd);
    return false;
  }

  if (listen(server->fd, CONN_QUEUE_SIZE) == -1) {
    DB_Log(
      DB_LOG_ERROR, "Failed to listen on server socket: %s", strerror(errno));
    close(server->fd);
    return false;
  }

  if (!EventQueue_Init(&server->event_queue, MAX_EVENTS)) {
    DB_Log(DB_LOG_ERROR, "Failed to initialize event queue");
    close(server->fd);
    return false;
  }

  DB_Log(DB_LOG_INFO, "TCP server initialized on port %d", port);
  return true;
}

bool
EventTcpServer_Start(EventTcpServer* server,
                     void (*on_client_connect)(EventTcpClient* client),
                     void (*on_client_data)(EventTcpClient* client),
                     void (*on_client_disconnect)(EventTcpClient* client))
{
  if (!server) {
    DB_Log(DB_LOG_ERROR, "Invalid server pointer");
    return false;
  }

  server->on_client_connect = on_client_connect;
  server->on_client_data = on_client_data;
  server->on_client_disconnect = on_client_disconnect;

  if (!EventQueue_Add(&server->event_queue, server->fd, EVENT_READ, server)) {
    DB_Log(DB_LOG_ERROR, "Failed to add server socket to event queue");
    return false;
  }

  server->running = true;

  DB_Log(DB_LOG_INFO, "TCP server started on port %d", server->port);
  return true;
}

int
EventTcpServer_Process_Events(EventTcpServer* server, int timeout_ms)
{
  if (!server || !server->running) {
    return -1;
  }

  Event events[MAX_EVENTS];

  int num_events =
    EventQueue_Wait(&server->event_queue, events, MAX_EVENTS, timeout_ms);
  if (num_events == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to wait for events");
    return -1;
  }

  // event processor
  for (int i = 0; i < num_events; i++) {
    if (events[i].fd == server->fd) { // server socket
      if (events[i].type & EVENT_READ) {
        handle_accept_event(server);
      }
    } else { // client socket
      EventTcpClient* client = (EventTcpClient*)events[i].user_data;
      if (events[i].type & EVENT_ERROR) {
        handle_client_error_event(server, client);
      } else if (events[i].type & EVENT_READ) {
        handle_client_read_event(server, client);
      }
    }
  }

  return num_events;
}

void
EventTcpServer_Stop(EventTcpServer* server)
{
  if (!server) {
    return;
  }

  server->running = false;

  DB_Log(DB_LOG_INFO, "TCP server stopped");
}

void
EventTcpServer_Destroy(EventTcpServer* server)
{
  if (!server) {
    return;
  }

  if (server->running) {
    EventTcpServer_Stop(server);
  }

  EventQueue_Remove(&server->event_queue, server->fd);
  if (server->fd != -1) {
    close(server->fd);
    server->fd = -1;
  }

  EventQueue_Destroy(&server->event_queue);

  DB_Log(DB_LOG_INFO, "TCP server destroyed");
}

ssize_t
EventTcpClient_Send(EventTcpClient* client, const void* data, size_t len)
{
  if (!client || !data || len == 0) {
    return -1;
  }

  return send(client->fd, data, len, 0);
}

void
EventTcpClient_Close(EventTcpClient* client)
{
  if (!client) {
    return;
  }

  if (client->fd != -1) {
    close(client->fd);
    client->fd = -1;
  }

  if (client->buffer) {
    free(client->buffer);
    client->buffer = NULL;
  }

  free(client);
}