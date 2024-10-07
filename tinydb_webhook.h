#ifndef __TINY_DB_WEBHOOK
#define __TINY_DB_WEBHOOK

#include "tinydb_atomic_proc.h"
#include "tinydb_list.h"

#define MAX_URL_LENGTH 256

typedef struct
{
  char url[MAX_URL_LENGTH];
  char* message;
} WebhookArgs;

static void
Send_Http_Post(const char* url, const char* json_data);

static void
Send_Webhook(void* args);

void
Add_Webhook(const char* channel_name, const char* url);

void
Remove_Webhook(const char* channel_name, const char* url);
void
List_Webhooks(const char* channel_name);

void
Trigger_Webhooks(const char* channel_name, const char* message);

#endif // __TINY_DB_WEBHOOK