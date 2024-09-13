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
  pool->head = NULL;
  pthread_mutex_unlock(&pool->lock);
  pthread_mutex_destroy(&pool->lock);
}

void*
Memory_Pool_Alloc(MemoryPool* pool, size_t size)
{
  pthread_mutex_lock(&pool->lock);

  // align size to 8 bytes
  size = (size + 7) & ~7;

  // check free lists in existing blocks
  MemoryBlock* block = pool->head;
  while (block) {
    FreeChunk* prev = NULL;
    FreeChunk* chunk = block->free_list;

    while (chunk) {
      if (size <= MEMORY_POOL_SIZE) {
        // remove chunk from free list and return it
        if (prev) {
          prev->next = chunk->next;
        } else {
          block->free_list = chunk->next;
        }
        pthread_mutex_unlock(&pool->lock);
        return (void*)chunk;
      }
      prev = chunk;
      chunk = chunk->next;
    }
    block = block->next;
  }

  // if size is larger than MEMORY_POOL_SIZE, allocate dedicated memory
  if (size > MEMORY_POOL_SIZE) {
    void* memory = malloc(size);
    pthread_mutex_unlock(&pool->lock);
    return memory;
  }

  // find a block with enough space or create a new one
  block = pool->head;
  MemoryBlock* prev_block = NULL;
  while (block && block->used + size > block->size) {
    prev_block = block;
    block = block->next;
  }

  if (!block) {
    block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    if (!block) {
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    block->memory = (char*)malloc(MEMORY_POOL_SIZE);
    if (!block->memory) {
      free(block);
      pthread_mutex_unlock(&pool->lock);
      return NULL;
    }
    block->size = MEMORY_POOL_SIZE;
    block->used = 0;
    block->next = NULL;
    block->free_list = NULL;

    if (prev_block) {
      prev_block->next = block;
    } else {
      pool->head = block;
    }
  }

  void* result = block->memory + block->used;
  block->used += size;

  pthread_mutex_unlock(&pool->lock);
  return result;
}

void
Memory_Pool_Free(MemoryPool* pool, void* ptr, size_t size)
{
  if (!ptr || !pool)
    return;

  pthread_mutex_lock(&pool->lock);

  // try to find the block that contains the pointer
  MemoryBlock* block = pool->head;
  while (block) {
    if (ptr >= (void*)block->memory &&
        ptr < (void*)(block->memory + block->size)) {
      FreeChunk* chunk = (FreeChunk*)ptr;
      chunk->next = block->free_list;
      block->free_list = chunk;

      block->used -= size;

      // if block is free, reclaim it
      if (block->used == 0) {
        if (block == pool->head) {
          pool->head = block->next;
        } else {
          MemoryBlock* prev = pool->head;
          while (prev->next != block) {
            prev = prev->next;
          }
          prev->next = block->next;
        }

        free(block->memory);
        free(block);
      }

      pthread_mutex_unlock(&pool->lock);
      return;
    }
    block = block->next;
  }

  free(ptr);
  pthread_mutex_unlock(&pool->lock);
}
