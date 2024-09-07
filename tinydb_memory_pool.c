#include "tinydb_memory_pool.h"

void
Memory_Pool_Init(MemoryPool* pool)
{
  pool->head = NULL;
  pthread_mutex_init(&pool->lock, NULL);
}

void
Memory_Pool_Destroy(MemoryPool* pool)
{
  MemoryBlock* current = pool->head;
  while (current) {
    MemoryBlock* next = current->next;
    free(current->memory);
    free(current);
    current = next;
  }
  pthread_mutex_destroy(&pool->lock);
}

char*
Memory_Pool_Alloc(MemoryPool* pool, size_t size)
{
  pthread_mutex_lock(&pool->lock);

  if (!pool->head || pool->head->used + size > MEMORY_POOL_SIZE) {
    MemoryBlock* new_block = malloc(sizeof(MemoryBlock));
    new_block->memory = malloc(MEMORY_POOL_SIZE);
    new_block->used = 0;
    new_block->next = pool->head;
    pool->head = new_block;
  }

  char* result = pool->head->memory + pool->head->used;
  pool->head->used += size;

  pthread_mutex_unlock(&pool->lock);
  return result;
}

