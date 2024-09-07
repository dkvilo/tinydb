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
#include "tinydb_tcp_client_handler.h"

Database *db = NULL;
RuntimeContext *context = NULL;

void
After_Exit_Hook()
{
  DB_Log(DB_LOG_INFO, "BYE");
  Export_Snapshot(context, DEFAULT_EXIT_SNAPSHOT_NAME);
}

void
Signal_Handler(int signum)
{
  DB_Log(DB_LOG_INFO, "Interrupt signal (%d) received. Exiting gracefully...", signum);
  exit(0);
}

int
main(int argc, char const* argv[])
{
  atexit(After_Exit_Hook);
  signal(SIGINT, Signal_Handler);

  DB_Log(DB_LOG_INFO, "> %s Version %s Dev.\n", TINYDB_SIGNATURE, TINYDB_VERSION);

  Thread_Pool_Init();
  DB_Log(DB_LOG_INFO, "Thread Pool has been created.");

  context = Initialize_Context(NUM_INITAL_DATABASES, DEFAULT_SNAPSHOT_NAME);
  DB_Log(DB_LOG_INFO, "RuntimeContext has been allocated and initialized.");

  db = context->db_manager.databases;
  if (!db->name) {
    db->name = "default";
  }

  DB_Log(DB_LOG_INFO, "Default Database (%s) has been created.", db->name);

  TCP_Server tcp_server = { 0 };
  TCP_Client tcp_client = { 0 };

  TCP_Server_Create(&tcp_server);
  DB_Log(DB_LOG_INFO, "TCP Server has been initialized.", db->name);
  DB_Log(DB_LOG_INFO, " - Host: %s", "127.0.0.1");
  DB_Log(DB_LOG_INFO, " - Port: %d", PORT);
  
  TCP_Server_Process_Connections(&tcp_server, &tcp_client, TCP_Client_Handler);
  
  return 0;
}
