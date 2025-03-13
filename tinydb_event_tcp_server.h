/**
 * TinyDB Event-based TCP Server
 * Provides a non-blocking TCP server implementation using the event queue
 * abstraction
 */
#ifndef __TINY_DB_EVENT_TCP_SERVER_H
#define __TINY_DB_EVENT_TCP_SERVER_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "tinydb_event_queue.h"
#include "config.h"

/* maximum number of events to process at once */
#define MAX_EVENTS 1024

typedef struct
{
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len;
  char* buffer;
  size_t buffer_size;
  size_t data_len;
} EventTcpClient;

typedef struct
{
  int fd;
  struct sockaddr_in addr;
  EventQueue event_queue;
  int port;

  void (*on_client_connect)(EventTcpClient* client);
  void (*on_client_data)(EventTcpClient* client);
  void (*on_client_disconnect)(EventTcpClient* client);

  bool running;
} EventTcpServer;

bool
EventTcpServer_Init(EventTcpServer* server, int port);

bool
EventTcpServer_Start(EventTcpServer* server,
                     void (*on_client_connect)(EventTcpClient* client),
                     void (*on_client_data)(EventTcpClient* client),
                     void (*on_client_disconnect)(EventTcpClient* client));

int
EventTcpServer_Process_Events(EventTcpServer* server, int timeout_ms);

void
EventTcpServer_Stop(EventTcpServer* server);

void
EventTcpServer_Destroy(EventTcpServer* server);

ssize_t
EventTcpClient_Send(EventTcpClient* client, const void* data, size_t len);

void
EventTcpClient_Close(EventTcpClient* client);

#endif // __TINY_DB_EVENT_TCP_SERVER_H