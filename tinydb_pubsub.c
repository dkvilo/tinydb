#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tinydb_log.h"
#include "tinydb_pubsub.h"
#include "tinydb_webhook.h"

PubSubSystem*
Create_PubSub_System()
{
  PubSubSystem* system = (PubSubSystem*)malloc(sizeof(PubSubSystem));
  system->channels = NULL;
  pthread_mutex_init(&system->lock, NULL);
  return system;
}

void
Subscribe(PubSubSystem* system, const char* channel_name, int32_t socket_fd)
{
  if (channel_name == NULL) {
    DB_Log(DB_LOG_ERROR, "Channel name cannot be NULL\n");
    return;
  }

  pthread_mutex_lock(&system->lock);
  Channel* channel = Find_Channel(system, channel_name);
  if (!channel) {
    channel = malloc(sizeof(Channel));
    channel->name = strdup(channel_name);
    channel->subscribers = NULL;
    channel->next = system->channels;
    system->channels = channel;
  }

  Subscriber* new_sub = malloc(sizeof(Subscriber));
  new_sub->socket_fd = socket_fd;
  new_sub->next = channel->subscribers;
  channel->subscribers = new_sub;

  pthread_mutex_unlock(&system->lock);
}

void
Unsubscribe(PubSubSystem* system, const char* channel_name, int32_t socket_fd)
{
  pthread_mutex_lock(&system->lock);
  Channel* channel = Find_Channel(system, channel_name);
  if (!channel) {
    pthread_mutex_unlock(&system->lock);
    return;
  }
  Subscriber* prev = NULL;
  Subscriber* sub = channel->subscribers;
  while (sub) {
    if (sub->socket_fd == socket_fd) {
      if (prev) {
        prev->next = sub->next;
      } else {
        channel->subscribers = sub->next;
      }
      free(sub);
      break;
    }
    prev = sub;
    sub = sub->next;
  }

  // if channel is empty and remove
  if (channel->subscribers == NULL) {
    Remove_Empty_Channel(system, channel_name);
  }

  pthread_mutex_unlock(&system->lock);
}

void
Unsubscribe_All(PubSubSystem* system, int32_t socket_fd)
{
  pthread_mutex_lock(&system->lock);
  Channel* channel = system->channels;
  Channel* prev_channel = NULL;

  while (channel) {
    Subscriber* prev_sub = NULL;
    Subscriber* sub = channel->subscribers;

    while (sub) {
      if (sub->socket_fd == socket_fd) {
        if (prev_sub) {
          prev_sub->next = sub->next;
        } else {
          channel->subscribers = sub->next;
        }
        Subscriber* to_free = sub;
        sub = sub->next;
        free(to_free);
      } else {
        prev_sub = sub;
        sub = sub->next;
      }
    }

    // channel is empty
    if (channel->subscribers == NULL) {
      if (prev_channel) {
        prev_channel->next = channel->next;
      } else {
        system->channels = channel->next;
      }
      Channel* to_free = channel;
      channel = channel->next;
      free(to_free->name);
      free(to_free);
    } else {
      prev_channel = channel;
      channel = channel->next;
    }
  }

  pthread_mutex_unlock(&system->lock);
}

Channel*
Find_Channel(PubSubSystem* system, const char* channel_name)
{
  Channel* current = system->channels;
  while (current != NULL) {
    if (strcmp(current->name, channel_name) == 0) {
      return current; // found
    }
    current = current->next;
  }
  return NULL; // not found
}

void
Remove_Empty_Channel(PubSubSystem* system, const char* channel_name)
{
  Channel* prev = NULL;
  Channel* current = system->channels;

  while (current != NULL) {
    if (strcmp(current->name, channel_name) == 0) {
      if (prev) {
        prev->next = current->next;
      } else {
        system->channels = current->next;
      }
      free(current->name);
      free(current);
      break;
    }
    prev = current;
    current = current->next;
  }
}

void
Publish(PubSubSystem* system, const char* channel_name, const char* message)
{
  pthread_mutex_lock(&system->lock);
  Channel* channel = Find_Channel(system, channel_name);
  if (!channel) {
    pthread_mutex_unlock(&system->lock);
    return;
  }

  Subscriber* sub = channel->subscribers;
  while (sub) {
    SendMessageArgs* args = (SendMessageArgs*)malloc(sizeof(SendMessageArgs));
    args->socket_fd = sub->socket_fd;
    args->message = strdup(message);
    Thread_Pool_Add_Task(Send_Message, (void*)args);
    sub = sub->next;
  }

  Trigger_Webhooks(channel_name, message);

  pthread_mutex_unlock(&system->lock);
}

void
Send_Message(void* args)
{
  SendMessageArgs* send_args = (SendMessageArgs*)args;
  int32_t sock = send_args->socket_fd;
  char* message = send_args->message;
  if (write(sock, message, strlen(message)) == -1) {
    DB_Log(DB_LOG_ERROR, "Unable to send message to subscriber");
  }
  write(sock, "\n", 1);
  free(message);
  free(send_args);
}

void
Destroy_PubSub_System(PubSubSystem* system)
{
  pthread_mutex_lock(&system->lock);

  Channel* channel = system->channels;
  while (channel != NULL) {
    Subscriber* sub = channel->subscribers;
    while (sub != NULL) {
      Subscriber* next_sub = sub->next;
      free(sub);
      sub = next_sub;
    }
    Channel* next_channel = channel->next;
    free(channel);
    channel = next_channel;
  }

  pthread_mutex_unlock(&system->lock);
  pthread_mutex_destroy(&system->lock);
  free(system);
}
