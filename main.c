/**
 * TinyDB by David Kviloria <david@skystargames.com>
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "tinydb_context.h"
#include "tinydb_database.h"
#include "tinydb_event_client_handler.h"
#include "tinydb_event_tcp_server.h"
#include "tinydb_hash.h"
#include "tinydb_log.h"
#include "tinydb_snapshot.h"
#include "tinydb_thread_pool.h"
#include "tinydb_ttl.h"
#include "tinydb_user_manager.h"
#include "tinydb_webhook.h"

RuntimeContext* context = NULL;
EventTcpServer tcp_server = { 0 };

bool server_running = true;

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

  server_running = false;
  EventTcpServer_Stop(&tcp_server);

  exit(EXIT_SUCCESS);
}

void
log_tinydb_ascii_art()
{
  const char* tinydb_ascii_art =
    "       _          _          _           _        _       _            _  "
    "     \n"
    "      /\\ \\       /\\ \\       /\\ \\     _  /\\ \\     /\\_\\    /\\ \\ "
    "        / /\\     \n"
    "      \\_\\ \\      \\ \\ \\     /  \\ \\   /\\_\\\\ \\ \\   / / /   /  "
    "\\ \\____   / /  \\    \n"
    "      /\\__ \\     /\\ \\_\\   / /\\ \\ \\_/ / / \\ \\ \\_/ / /   / /\\ "
    "\\_____\\ / / /\\ \\   \n"
    "     / /_ \\ \\   / /\\/_/  / / /\\ \\___/ /   \\ \\___/ /   / / /\\/___  "
    "// / /\\ \\ \\  \n"
    "    / / /\\ \\ \\ / / /    / / /  \\/____/     \\ \\ \\_/   / / /   / / "
    "// / /\\ \\_\\ \\ \n"
    "   / / /  \\/_// / /    / / /    / / /       \\ \\ \\   / / /   / / // / "
    "/\\ \\ \\___\\\n"
    "  / / /      / / /    / / /    / / /         \\ \\ \\ / / /   / / // / /  "
    "\\ \\ \\__/ \n"
    " / / /   ___/ / /__  / / /    / / /           \\ \\ \\\\ \\ \\__/ / // / "
    "/____\\_\\ \\   \n"
    "/_/ /   /\\__/\\/_/__\\/ / /    / / /             \\ \\_\\\\ \\___\\ / // "
    "/ /__________\\ \n"
    "\\_\\/    \\/_________/\\/ /     \\/_/               \\/_/ \\/_____/ "
    "\\/_____________/ \n";

  printf("\n%s\n", tinydb_ascii_art);
}

int
main(int argc, char const* argv[])
{
  atexit(After_Exit_Hook);
  signal(SIGINT, Signal_Handler);

  log_tinydb_ascii_art();
  DB_Log(DB_LOG_INFO, "> %s Version %s Dev.", TINYDB_SIGNATURE, TINYDB_VERSION);

  Thread_Pool_Init();
  DB_Log(DB_LOG_INFO, "Thread Pool has been created.");

  context = Initialize_Context(NUM_INITAL_DATABASES, DEFAULT_SNAPSHOT_NAME);
  DB_Log(DB_LOG_INFO, "RuntimeContext has been allocated and initialized.");

  if (context->snapshot_config.enabled) {
    context->snapshot_config.snapshot_filename = DEFAULT_SNAPSHOT_NAME;
    DB_Log(DB_LOG_INFO, "Background snapshots enabled with interval of %d seconds",
           context->snapshot_config.interval_seconds);
  }

  // user manager with default user
  context->user_manager.users = (DB_User*)malloc(sizeof(DB_User));
  if (!context->user_manager.users) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for users");
    return EXIT_FAILURE;
  }
  memset(context->user_manager.users, 0, sizeof(DB_User));
 
  context->user_manager.users[0].ID = 0;
  context->user_manager.users[0].name = strdup("default");
  if (!context->user_manager.users[0].name) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for default username");
    free(context->user_manager.users);
    return EXIT_FAILURE;
  }
  
  context->user_manager.users[0].access = (DB_Access*)malloc(sizeof(DB_Access));
  if (!context->user_manager.users[0].access) {
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for user access");
    free(context->user_manager.users[0].name);
    free(context->user_manager.users);
    return EXIT_FAILURE;
  }
  
  context->user_manager.users[0].access[0].database = 0;
  context->user_manager.users[0].access[0].acl = DB_READ | DB_WRITE | DB_DELETE;

  SHA256(context->user_manager.users[0].password, "default", 3);
  context->user_manager.num_users++;

  context->Active.user = &context->user_manager.users[0];
  context->Active.db = context->db_manager.databases;
  if (!context->Active.db->name) {
    context->Active.db->name = "default";
  }
  DB_Log(DB_LOG_INFO, "Default Database (%s) has been assigned.", context->Active.db->name);

  if (!EventClientHandler_Init()) {
    DB_Log(DB_LOG_ERROR, "Failed to initialize event-based client handler");
    return EXIT_FAILURE;
  }

  if (!EventTcpServer_Init(&tcp_server, PORT)) {
    DB_Log(DB_LOG_ERROR, "Failed to initialize event-based TCP server");
    return EXIT_FAILURE;
  }

  if (!EventTcpServer_Start(
        &tcp_server,
        EventClientHandler_OnConnect,
        EventClientHandler_OnData,
        EventClientHandler_OnDisconnect)) {
    DB_Log(DB_LOG_ERROR, "Failed to start event-based TCP server");
    return EXIT_FAILURE;
  }

  DB_Log(DB_LOG_INFO, "TCP Server has been initialized.");
  DB_Log(DB_LOG_INFO, " - Host: %s", "127.0.0.1");
  DB_Log(DB_LOG_INFO, " - Port: %d", PORT);
  DB_Log(DB_LOG_INFO, "Waiting for incoming connections...");

  while (server_running) {
    // events are processed with a timeout of 100ms
    EventTcpServer_Process_Events(&tcp_server, 100);
  }

  EventTcpServer_Destroy(&tcp_server);
  EventClientHandler_Cleanup();

  return 0;
}
