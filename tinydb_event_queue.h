/**
 * TinyDB Event Queue Abstraction
 * Provides a unified interface for event-based I/O using kqueue (macOS/BSD) or
 * epoll (Linux)
 */
#ifndef __TINY_DB_EVENT_QUEUE_H
#define __TINY_DB_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||      \
  defined(__NetBSD__)
#define USE_KQUEUE
#include <sys/event.h>
#elif defined(__linux__)
#define USE_EPOLL
#include <sys/epoll.h>
#else
#error "Unsupported platform: requires kqueue or epoll"
#endif

typedef enum
{
  EVENT_READ = 1,
  EVENT_WRITE = 2,
  EVENT_ERROR = 4
} EventType;

typedef struct
{
  int fd;
  EventType type;
  void* user_data;
} Event;

typedef struct
{
  int queue_fd;
  int max_events;

#ifdef USE_KQUEUE
  struct kevent* events;
#else
  struct epoll_event* events;
#endif
} EventQueue;

bool
EventQueue_Init(EventQueue* queue, int max_events);

void
EventQueue_Destroy(EventQueue* queue);

bool
EventQueue_Add(EventQueue* queue,
               int fd,
               EventType event_type,
               void* user_data);

bool
EventQueue_Modify(EventQueue* queue,
                  int fd,
                  EventType event_type,
                  void* user_data);

bool
EventQueue_Remove(EventQueue* queue, int fd);

int
EventQueue_Wait(EventQueue* queue,
                Event* out_events,
                int max_events,
                int timeout_ms);

#endif // __TINY_DB_EVENT_QUEUE_H