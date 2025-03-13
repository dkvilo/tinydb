/**
 * TinyDB by David Kviloria <david@skystargames.com>
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tinydb_context.h"
#include "tinydb_hash.h"
#include "tinydb_log.h"
#include "tinydb_snapshot.h"
#include "tinydb_tcp_client_handler.h"
#include "tinydb_tcp_server.h"
#include "tinydb_thread_pool.h"
#include "tinydb_webhook.h"

RuntimeContext* context = NULL;

void
After_Exit_Hook()
{
  DB_Log(DB_LOG_INFO, "BYE");
#if 0
  Export_Snapshot(context, DEFAULT_EXIT_SNAPSHOT_NAME);
#endif
}

void
Signal_Handler(int signum)
{
  DB_Log(DB_LOG_INFO,
         "Interrupt signal (%d) received. Exiting gracefully...",
         signum);
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

#if 1
  context->user_manager.users = (DB_User*)malloc(sizeof(DB_User));
  context->user_manager.users[0].ID = 0;
  context->user_manager.users[0].name = "default";
  context->user_manager.users[0].access = (DB_Access*)malloc(sizeof(DB_Access));
  context->user_manager.users[0].access[0].database = 0;
  context->user_manager.users[0].access[0].acl = DB_READ | DB_WRITE | DB_DELETE;

  SHA256(context->user_manager.users[0].password, "123", 3);
  context->user_manager.num_users++;
#endif

  context->Active.user = &context->user_manager.users[0];
  context->Active.db = context->db_manager.databases;
  if (!context->Active.db->name) {
    context->Active.db->name = "default";
  }
  DB_Log(DB_LOG_INFO,
         "Default Database (%s) has been assigned.",
         context->Active.db->name);

  TCP_Server tcp_server = { 0 };
  TCP_Client tcp_client = { 0 };

  Add_Webhook("@hook_test", "http://localhost/webhook");

  List_Webhooks("@hook_test");

  TCP_Server_Create(&tcp_server);
  DB_Log(
    DB_LOG_INFO, "TCP Server has been initialized.", context->Active.db->name);
  DB_Log(DB_LOG_INFO, " - Host: %s", "127.0.0.1");
  DB_Log(DB_LOG_INFO, " - Port: %d", PORT);

  TCP_Server_Process_Connections(&tcp_server, &tcp_client, TCP_Client_Handler);

  return 0;
}
