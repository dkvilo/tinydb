/**
 * TinyDB Event Queue Implementation
 * Provides a unified interface for event-based I/O using kqueue (macOS/BSD) or
 * epoll (Linux)
 */
#include "tinydb_event_queue.h"
#include "tinydb_log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static bool
set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to get socket flags: %s", strerror(errno));
    return false;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    DB_Log(DB_LOG_ERROR,
           "Failed to set socket to non-blocking mode: %s",
           strerror(errno));
    return false;
  }

  return true;
}

bool
EventQueue_Init(EventQueue* queue, int max_events)
{
  if (!queue || max_events <= 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to EventQueue_Init");
    return false;
  }

  queue->max_events = max_events;

#ifdef USE_KQUEUE
  // Create kqueue
  queue->queue_fd = kqueue();
  if (queue->queue_fd == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to create kqueue: %s", strerror(errno));
    return false;
  }

  // Allocate memory for events
  queue->events = (struct kevent*)malloc(sizeof(struct kevent) * max_events);
  if (!queue->events) {
    close(queue->queue_fd);
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for kqueue events");
    return false;
  }
#else
  // Create epoll
  queue->queue_fd = epoll_create1(0);
  if (queue->queue_fd == -1) {
    DB_Log(DB_LOG_ERROR, "Failed to create epoll: %s", strerror(errno));
    return false;
  }

  // Allocate memory for events
  queue->events =
    (struct epoll_event*)malloc(sizeof(struct epoll_event) * max_events);
  if (!queue->events) {
    close(queue->queue_fd);
    DB_Log(DB_LOG_ERROR, "Failed to allocate memory for epoll events");
    return false;
  }
#endif

  DB_Log(DB_LOG_INFO, "Event queue initialized with max_events=%d", max_events);
  return true;
}

void
EventQueue_Destroy(EventQueue* queue)
{
  if (!queue) {
    return;
  }

  if (queue->queue_fd != -1) {
    close(queue->queue_fd);
    queue->queue_fd = -1;
  }

  if (queue->events) {
    free(queue->events);
    queue->events = NULL;
  }

  DB_Log(DB_LOG_INFO, "Event queue destroyed");
}

bool
EventQueue_Add(EventQueue* queue, int fd, EventType event_type, void* user_data)
{
  if (!queue || fd < 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to EventQueue_Add");
    return false;
  }

  if (!set_nonblocking(fd)) {
    return false;
  }

#ifdef USE_KQUEUE
  struct kevent changes[2];
  int n_changes = 0;

  if (event_type & EVENT_READ) {
    EV_SET(&changes[n_changes++], fd, EVFILT_READ, EV_ADD, 0, 0, user_data);
  }

  if (event_type & EVENT_WRITE) {
    EV_SET(&changes[n_changes++], fd, EVFILT_WRITE, EV_ADD, 0, 0, user_data);
  }

  if (kevent(queue->queue_fd, changes, n_changes, NULL, 0, NULL) == -1) {
    DB_Log(
      DB_LOG_ERROR, "Failed to add fd %d to kqueue: %s", fd, strerror(errno));
    return false;
  }
#else
  struct epoll_event ev;
  ev.events = 0;

  if (event_type & EVENT_READ) {
    ev.events |= EPOLLIN;
  }

  if (event_type & EVENT_WRITE) {
    ev.events |= EPOLLOUT;
  }

  ev.events |= EPOLLERR | EPOLLHUP;
  ev.data.ptr = user_data;

  if (epoll_ctl(queue->queue_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
    DB_Log(
      DB_LOG_ERROR, "Failed to add fd %d to epoll: %s", fd, strerror(errno));
    return false;
  }
#endif

  DB_Log(DB_LOG_INFO,
         "Added fd %d to event queue with event_type=%d",
         fd,
         event_type);
  return true;
}

bool
EventQueue_Modify(EventQueue* queue,
                  int fd,
                  EventType event_type,
                  void* user_data)
{
  if (!queue || fd < 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to EventQueue_Modify");
    return false;
  }

#ifdef USE_KQUEUE
  struct kevent changes[4];
  int n_changes = 0;

  EV_SET(&changes[n_changes++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(&changes[n_changes++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

  if (event_type & EVENT_READ) {
    EV_SET(&changes[n_changes++], fd, EVFILT_READ, EV_ADD, 0, 0, user_data);
  }

  if (event_type & EVENT_WRITE) {
    EV_SET(&changes[n_changes++], fd, EVFILT_WRITE, EV_ADD, 0, 0, user_data);
  }

  // ignore errors from EV_DELETE as the filter might not exist
  if (kevent(queue->queue_fd, changes, n_changes, NULL, 0, NULL) == -1) {
    if (errno != ENOENT) {
      DB_Log(DB_LOG_ERROR,
             "Failed to modify fd %d in kqueue: %s",
             fd,
             strerror(errno));
      return false;
    }
  }
#else
  struct epoll_event ev;
  ev.events = 0;

  if (event_type & EVENT_READ) {
    ev.events |= EPOLLIN;
  }

  if (event_type & EVENT_WRITE) {
    ev.events |= EPOLLOUT;
  }

  ev.events |= EPOLLERR | EPOLLHUP;

  ev.data.ptr = user_data;

  if (epoll_ctl(queue->queue_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
    DB_Log(
      DB_LOG_ERROR, "Failed to modify fd %d in epoll: %s", fd, strerror(errno));
    return false;
  }
#endif

  DB_Log(DB_LOG_INFO,
         "Modified fd %d in event queue with event_type=%d",
         fd,
         event_type);
  return true;
}

bool
EventQueue_Remove(EventQueue* queue, int fd)
{
  if (!queue || fd < 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to EventQueue_Remove");
    return false;
  }

#ifdef USE_KQUEUE
  struct kevent changes[2];

  EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

  // ignore errors as the filters might not exist
  kevent(queue->queue_fd, changes, 2, NULL, 0, NULL);
#else
  if (epoll_ctl(queue->queue_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
    if (errno != ENOENT) {
      DB_Log(DB_LOG_ERROR,
             "Failed to remove fd %d from epoll: %s",
             fd,
             strerror(errno));
      return false;
    }
  }
#endif

  DB_Log(DB_LOG_INFO, "Removed fd %d from event queue", fd);
  return true;
}

int
EventQueue_Wait(EventQueue* queue,
                Event* out_events,
                int max_events,
                int timeout_ms)
{
  if (!queue || !out_events || max_events <= 0) {
    DB_Log(DB_LOG_ERROR, "Invalid arguments to EventQueue_Wait");
    return -1;
  }

  int num_events = 0;

#ifdef USE_KQUEUE
  struct timespec timeout;
  struct timespec* timeout_ptr = NULL;

  if (timeout_ms >= 0) {
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
    timeout_ptr = &timeout;
  }

  num_events =
    kevent(queue->queue_fd, NULL, 0, queue->events, max_events, timeout_ptr);

  if (num_events == -1) {
    if (errno == EINTR) {
      return 0;
    }

    DB_Log(DB_LOG_ERROR, "kevent failed: %s", strerror(errno));
    return -1;
  }

  for (int i = 0; i < num_events; i++) {
    out_events[i].fd = queue->events[i].ident;
    out_events[i].user_data = queue->events[i].udata;
    out_events[i].type = 0;

    if (queue->events[i].filter == EVFILT_READ) {
      out_events[i].type |= EVENT_READ;
    } else if (queue->events[i].filter == EVFILT_WRITE) {
      out_events[i].type |= EVENT_WRITE;
    }

    if (queue->events[i].flags & EV_ERROR) {
      out_events[i].type |= EVENT_ERROR;
    }
  }
#else
  int timeout_val = timeout_ms;
  if (timeout_ms < 0) {
    timeout_val = -1; // wait indefinitely
  }

  num_events =
    epoll_wait(queue->queue_fd, queue->events, max_events, timeout_val);

  if (num_events == -1) {
    if (errno == EINTR) {
      return 0;
    }

    DB_Log(DB_LOG_ERROR, "epoll_wait failed: %s", strerror(errno));
    return -1;
  }

  for (int i = 0; i < num_events; i++) {
    out_events[i].fd = queue->events[i].data.fd;
    out_events[i].user_data = queue->events[i].data.ptr;
    out_events[i].type = 0;

    if (queue->events[i].events & EPOLLIN) {
      out_events[i].type |= EVENT_READ;
    }

    if (queue->events[i].events & EPOLLOUT) {
      out_events[i].type |= EVENT_WRITE;
    }

    if (queue->events[i].events & (EPOLLERR | EPOLLHUP)) {
      out_events[i].type |= EVENT_ERROR;
    }
  }
#endif

  return num_events;
}