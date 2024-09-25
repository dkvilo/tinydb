#ifndef __TINY_DB_PUBSUB
#define __TINY_DB_PUBSUB

#include "tinydb_thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct Subscriber
{
  int32_t socket_fd;
  struct Subscriber* next;
} Subscriber;

typedef struct Channel
{
  char* name;
  Subscriber* subscribers;
  struct Channel* next;
} Channel;

typedef struct PubSubSystem
{
  Channel* channels;
  pthread_mutex_t lock;
} PubSubSystem;

typedef struct
{
  int32_t socket_fd;
  char* message;
} SendMessageArgs;

PubSubSystem*
Create_PubSub_System();

void
Subscribe(PubSubSystem* system, const char* channel_name, int32_t socket_fd);

void
Unsubscribe(PubSubSystem* system, const char* channel_name, int32_t socket_fd);

void
Unsubscribe_All(PubSubSystem* system, int32_t socket_fd);

Channel*
Find_Channel(PubSubSystem* system, const char* channel_name);

void
Remove_Empty_Channel(PubSubSystem* system, const char* channel_name);

void
Publish(PubSubSystem* system, const char* channel_name, const char* message);

void
Send_Message(void* socket_desc);

void
Destroy_PubSub_System(PubSubSystem* system);

#endif // __TINY_DB_PUBSUB