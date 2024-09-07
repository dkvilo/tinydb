#include "tinydb_tcp_server.h"
#include "tinydb_log.h"
#include "tinydb_thread_pool.h"

#include <errno.h>

void
TCP_Server_Create(TCP_Server* sv)
{
  sv->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sv->fd == -1) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER Unable to create socket");
    return;
  }

  sv->server.sin_family = AF_INET;
  sv->server.sin_addr.s_addr = INADDR_ANY;
  sv->server.sin_port = htons(PORT);

  if (bind(sv->fd, (struct sockaddr*)&sv->server, sizeof(sv->server)) < 0) {
    DB_Log(DB_LOG_ERROR, "TCP_SERVER Unable to bind");
    return;
  }

  listen(sv->fd, CONN_QUEUE_SIZE);
}

void
TCP_Server_Process_Connections(TCP_Server* sv,
                               TCP_Client* c,
                               void (*function)(void*))
{
  DB_Log(DB_LOG_INFO, "TCP_SERVER Waiting for incoming connections");

  while ((c->sock = accept(sv->fd, (struct sockaddr*)&c->client, &c->len))) {
    if (c->sock < 0) {
      DB_Log(DB_LOG_ERROR, "TCP_SERVER Accept failed: %s", strerror(errno));
      continue;
    }

    DB_Log(DB_LOG_INFO, "TCP_SERVER Connection accepted");

    pthread_t sniffer_thread;
    c->new_sock = malloc(sizeof(int));
    *c->new_sock = c->sock;

    Thread_Pool_Add_Task(function, (void*)c->new_sock);
    DB_Log(DB_LOG_INFO, "TCP_SERVER Client handler was added to task queue");
  }
}