#ifndef __TINY_DB_MEMORY_POOL
#define __TINY_DB_MEMORY_POOL

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_POOL_SIZE 4096

typedef struct FreeChunk
{
  struct FreeChunk* next;
} FreeChunk;

typedef struct MemoryBlock
{
  char* memory;
  size_t size;
  size_t used;
  struct MemoryBlock* next;
  FreeChunk* free_list;
} MemoryBlock;

typedef struct MemoryPool
{
  MemoryBlock* head;
  pthread_mutex_t lock;
} MemoryPool;

void
Memory_Pool_Init(MemoryPool* pool);

void
Memory_Pool_Destroy(MemoryPool* pool);

void*
Memory_Pool_Alloc(MemoryPool* pool, size_t size);

void
Memory_Pool_Free(MemoryPool* pool, void* ptr, size_t size);

#endif // __TINY_DB_MEMORY_POOL
