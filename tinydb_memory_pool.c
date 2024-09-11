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
  pthread_mutex_lock(&pool->lock);
  MemoryBlock* current = pool->head;
  while (current) {
    MemoryBlock* next = current->next;
    free(current->memory);
    free(current);
    current = next;
  }
  pthread_mutex_unlock(&pool->lock);
  pthread_mutex_destroy(&pool->lock);
}

void*
Memory_Pool_Alloc(MemoryPool* pool, size_t size)
{
  pthread_mutex_lock(&pool->lock);

  // align size to 8 bytes
  size = (size + 7) & ~7;

  // if size is larger than MEMORY_POOL_SIZE, allocate a dedicated block
  if (size > MEMORY_POOL_SIZE) {
    MemoryBlock* new_block = malloc(sizeof(MemoryBlock));
    if (!new_block) {
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    new_block->memory = malloc(size);
    if (!new_block->memory) {
      free(new_block);
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    new_block->size = size;
    new_block->used = size;
    new_block->next = pool->head;
    pool->head = new_block;
    pthread_mutex_unlock(&pool->lock);
    return new_block->memory;
  }

  // find a block with enough space or create a new one
  MemoryBlock* current = pool->head;
  MemoryBlock* prev = NULL;
  while (current && current->used + size > current->size) {
    prev = current;
    current = current->next;
  }

  if (!current) {
    current = malloc(sizeof(MemoryBlock));
    if (!current) {
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    current->memory = malloc(MEMORY_POOL_SIZE);
    if (!current->memory) {
      free(current);
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    current->size = MEMORY_POOL_SIZE;
    current->used = 0;
    current->next = NULL;
    if (prev) {
      prev->next = current;
    } else {
      pool->head = current;
    }
  }

  void* result = current->memory + current->used;
  current->used += size;

  pthread_mutex_unlock(&pool->lock);
  return result;
}

void
Memory_Pool_Free(MemoryPool* pool, void* ptr)
{
  // we need to track of allocations and free them when possible
  (void)pool;
  (void)ptr;
}