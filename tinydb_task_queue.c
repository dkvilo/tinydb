#include "tinydb_task_queue.h"

void
Task_Queue_Init(Task_Queue* queue)
{
  queue->tasks = (Task*)malloc(sizeof(Task) * MAX_QUEUE_SIZE);
  queue->front = queue->rear = queue->count = 0;
  queue->size = MAX_QUEUE_SIZE;
  pthread_mutex_init(&(queue->mutex), NULL);
  pthread_cond_init(&(queue->not_empty), NULL);
  pthread_cond_init(&(queue->not_full), NULL);
}

void
Task_Queue_Push(Task_Queue* queue, Task task)
{
  pthread_mutex_lock(&(queue->mutex));
  while (queue->count == queue->size) {
    pthread_cond_wait(&(queue->not_full), &(queue->mutex));
  }
  queue->tasks[queue->rear] = task;
  queue->rear = (queue->rear + 1) % queue->size;
  queue->count++;
  pthread_cond_signal(&(queue->not_empty));
  pthread_mutex_unlock(&(queue->mutex));
}

Task
Task_Queue_Pop(Task_Queue* queue)
{
  pthread_mutex_lock(&(queue->mutex));
  while (queue->count == 0) {
    pthread_cond_wait(&(queue->not_empty), &(queue->mutex));
  }
  Task task = queue->tasks[queue->front];
  queue->front = (queue->front + 1) % queue->size;
  queue->count--;
  pthread_cond_signal(&(queue->not_full));
  pthread_mutex_unlock(&(queue->mutex));
  return task;
}