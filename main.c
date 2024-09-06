/**
 * @author David Kviloria
 * 
 * main.c
 * 
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tinydb.h"
#include "tinydb_utils.h"
#include "tinydb_log.h"
#include "tinydb_tcp_server.h"
#include "tinydb_thread_pool.h"
#include "tinydb_hash.h"
#include "tinydb_snapshot.h"

Database *db = NULL;
RuntimeContext *context = NULL;

void
After_Exit_Hook()
{
  DB_Log(DB_LOG_INFO, "BYE");
}

void
Signal_Handler(int signum)
{
  DB_Log(DB_LOG_INFO, "Interrupt signal (%d) received.", signum);
  exit(signum);
}

void
client_handler(void* socket_desc);

int
main(int argc, char const* argv[])
{
  DB_Log(DB_LOG_INFO, "> %s Version %s Dev.\n", TINYDB_SIGNATURE, TINYDB_VERSION);

  context = Initialize_Context(1);
  DB_Log(DB_LOG_INFO, "RuntimeContext has been allocated and initialized.");

  db = context->db_manager.databases;
  db->name = "default";
  DB_Log(DB_LOG_INFO, "Default Database (%s) has been created.", db->name);

  Thread_Pool_Init();
  DB_Log(DB_LOG_INFO, "Thread Pool has been created.");

  TCP_Server tcp_server = { 0 };
  TCP_Client tcp_client = { 0 };

  TCP_create_server(&tcp_server);
  DB_Log(DB_LOG_INFO, "TCP Server has been initialized.", db->name);
  DB_Log(DB_LOG_INFO, " - Host: %s", "127.0.0.1");
  DB_Log(DB_LOG_INFO, " - Port: %d", PORT);
  
  TCP_server_process_connections(&tcp_server, &tcp_client, client_handler);

  atexit(After_Exit_Hook);
  signal(SIGINT, Signal_Handler);

  return 0;
}
