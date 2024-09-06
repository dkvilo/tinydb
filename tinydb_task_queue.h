#ifndef __TINY_DB_TASK_QUEUE
#define __TINY_DB_TASK_QUEUE

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_QUEUE_SIZE 100

typedef struct
{
  void (*function)(void*);
  void* argument;
} Task;

typedef struct
{
  Task* tasks;
  int front;
  int rear;
  int count;
  int size;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
} Task_Queue;

void
Task_Queue_Init(Task_Queue* queue);

void
Task_Queue_Push(Task_Queue* queue, Task task);

Task
Task_Queue_Pop(Task_Queue* queue);

#endif // __TINY_DB_TASK_QUEUE