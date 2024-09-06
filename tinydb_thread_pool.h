#ifndef __TINY_DB_THREAD_POOL
#define __TINY_DB_THREAD_POOL

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tinydb_task_queue.h"

#define THREAD_POOL_SIZE 10

typedef struct
{
  pthread_t threads[THREAD_POOL_SIZE];
  Task_Queue task_queue;
  bool stop;
} Thread_Pool;

void*
Thread_Function(void* arg);

void
Thread_Pool_Init();

void
Thread_Pool_Add_Task(void (*function)(void*), void* argument);

void
Thread_Pool_Destroy();

#endif // __TINY_DB_THREAD_POOL