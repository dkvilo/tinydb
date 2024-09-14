#include "tinydb_pubsub.h"
#include "tinydb_log.h"

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

  free(message);
  free(send_args);
}
