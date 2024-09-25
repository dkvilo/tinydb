#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tinydb_context.h"
#include "tinydb_log.h"
#include "tinydb_thread_pool.h"
#include "tinydb_webhook.h"

#define MAX_RESPONSE_SIZE 4096
#define MAX_REQUEST_SIZE 2048

extern RuntimeContext* context;

void
Add_Webhook(const char* channel_name, const char* url)
{
  if (strncmp(channel_name, "@hook", 5) != 0) {
    DB_Log(
      DB_LOG_ERROR, "Cannot add webhook to non-hook channel: %s", channel_name);
    return;
  }

  DatabaseEntry entry = DB_Atomic_Get(context->Active.db, channel_name);
  HPLinkedList* webhook_list;

  if (entry.type == DB_ENTRY_LIST) {
    webhook_list = entry.value.list;
  } else {
    webhook_list = HPList_Create();
    DB_Value value = { .list = webhook_list };
    DB_Atomic_Store(context->Active.db, channel_name, value, DB_ENTRY_LIST);
  }

  HPList_RPush_String(webhook_list, url);
}

void
Remove_Webhook(const char* channel_name, const char* url)
{
  if (strncmp(channel_name, "@hook", 5) != 0) {
    DB_Log(DB_LOG_ERROR,
           "Cannot remove webhook from non-hook channel: %s",
           channel_name);
    return;
  }

  DatabaseEntry entry = DB_Atomic_Get(context->Active.db, channel_name);
  if (entry.type == DB_ENTRY_LIST) {
    HPLinkedList* webhook_list = entry.value.list;
    // todo (David) implement HPList_Remove_String for HList
    // HPList_Remove_String(webhook_list, url);
  }
}

void
List_Webhooks(const char* channel_name)
{
  if (strncmp(channel_name, "@hook", 5) != 0) {
    printf("Not a hook channel: %s\n", channel_name);
    return;
  }

  DatabaseEntry entry = DB_Atomic_Get(context->Active.db, channel_name);
  if (entry.type == DB_ENTRY_LIST) {
    HPLinkedList* webhook_list = entry.value.list;
    printf("Webhooks for channel %s:\n", channel_name);
    ListNode* node = webhook_list->head;
    while (node != NULL) {
      if (node->type == TYPE_STRING) {
        printf("- %s\n", node->value.string_value);
      }
      node = node->next;
    }
  } else {
    printf("No webhooks found for channel %s\n", channel_name);
  }
}

void
Trigger_Webhooks(const char* channel_name, const char* message)
{
  if (strncmp(channel_name, "@hook", 5) != 0) {
    return; // not a hook channel
  }

  DatabaseEntry entry = DB_Atomic_Get(context->Active.db, channel_name);
  if (entry.type == DB_ENTRY_LIST) {
    HPLinkedList* webhook_list = entry.value.list;
    ListNode* node = webhook_list->head;
    while (node != NULL) {
      if (node->type == TYPE_STRING) {
        WebhookArgs* args = (WebhookArgs*)malloc(sizeof(WebhookArgs));
        strncpy(args->url, node->value.string_value, MAX_URL_LENGTH);
        args->message = strdup(message);
        Thread_Pool_Add_Task(Send_Webhook, (void*)args);
      }
      node = node->next;
    }
  }
}

static void
Send_Webhook(void* args)
{
  WebhookArgs* webhook_args = (WebhookArgs*)args;
  char* json_string = malloc(sizeof(char) * MAX_REQUEST_SIZE);
  sprintf(json_string,
          "{\"event\": \"%s\", \"data\": \"%s\"}",
          webhook_args->message,
          "Hello, Sailor!");

  Send_Http_Post(webhook_args->url, json_string);
  free(json_string);
  free(webhook_args->message);
  free(webhook_args);
}

// todo (David) this do not support SSL (for now we are only parsing the schema https)
void
Send_Http_Post(const char* url, const char* json_data)
{
  char schema[6];
  char host[256];
  char path[256];
  int port = 80;

  if (sscanf(url, "%5[^:]://%255[^/]%255s", schema, host, path) < 2) {
    DB_Log(DB_LOG_ERROR, "Failed to parse URL: %s", url);
    return;
  }

  if (strcmp(schema, "https") == 0) {
    port = 443;
  } else if (strcmp(schema, "http") != 0) {
    DB_Log(DB_LOG_ERROR, "Unsupported schema: %s", schema);
    return;
  }

  char* colon = strchr(host, ':');
  if (colon != NULL) {
    *colon = '\0';
    port = atoi(colon + 1);
  }

  if (path[0] == '\0') {
    strcpy(path, "/");
  }

  int32_t sock;
  struct sockaddr_in server_addr;
  struct hostent* host_info;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    DB_Log(DB_LOG_ERROR, "Socket creation failed");
    return;
  }

  host_info = gethostbyname(host);
  if (host_info == NULL) {
    DB_Log(DB_LOG_ERROR, "Could not resolve host %s", host);
    close(sock);
    return;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  memcpy(&server_addr.sin_addr.s_addr, host_info->h_addr, host_info->h_length);

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    DB_Log(DB_LOG_ERROR, "Connection failed to %s://%s:%d", schema, host, port);
    close(sock);
    return;
  }

  char request[MAX_REQUEST_SIZE];
  snprintf(request,
           sizeof(request),
           "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           path,
           host,
           strlen(json_data),
           json_data);

  if (send(sock, request, strlen(request), 0) < 0) {
    DB_Log(
      DB_LOG_ERROR, "Failed to send request to %s://%s:%d", schema, host, port);
    close(sock);
    return;
  }

  char response[MAX_RESPONSE_SIZE];
  int bytes_received = recv(sock, response, sizeof(response) - 1, 0);
  if (bytes_received > 0) {
    response[bytes_received] = '\0';
    DB_Log(DB_LOG_INFO,
           "Webhook response from %s://%s:%d: %s",
           schema,
           host,
           port,
           response);
  }

  close(sock);
}
