#ifndef __TINY_DB_TCP_SERVER
#define __TINY_DB_TCP_SERVER

#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PORT        8079
#define BUFFER_SIZE 1023

typedef struct TCP_Client
{
  int       sock, *new_sock;
  struct    sockaddr_in client;
  socklen_t len;
} TCP_Client;

typedef struct TCP_Server
{
  int       fd, *sock;
  struct    sockaddr_in server;
} TCP_Server;

void TCP_create_server(TCP_Server *sv);

void TCP_server_process_connections(TCP_Server *sv, TCP_Client *c, void (*function)(void*));

#endif // __TINY_DB_TCP_SERVER
