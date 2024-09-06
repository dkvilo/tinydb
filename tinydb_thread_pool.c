#include "tinydb_thread_pool.h"

Thread_Pool thread_pool;

void*
Thread_Function(void* arg)
{
  while (!thread_pool.stop) {
    Task task = Task_Queue_Pop(&thread_pool.task_queue);
    (task.function)(task.argument);
  }
  return NULL;
}

void
Thread_Pool_Init()
{
  thread_pool.stop = false;
  Task_Queue_Init(&thread_pool.task_queue);
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_create(&thread_pool.threads[i], NULL, Thread_Function, NULL);
  }
}

void
Thread_Pool_Add_Task(void (*function)(void*), void* argument)
{
  Task task = { function, argument };
  Task_Queue_Push(&thread_pool.task_queue, task);
}

void
Thread_Pool_Destroy()
{
  thread_pool.stop = true;
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_join(thread_pool.threads[i], NULL);
  }
  free(thread_pool.task_queue.tasks);
}