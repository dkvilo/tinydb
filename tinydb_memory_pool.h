#ifndef __TINY_DB_MEMORY_POOL
#define __TINY_DB_MEMORY_POOL

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_POOL_SIZE 1024

typedef struct MemoryBlock
{
  char* memory;
  size_t used;
  struct MemoryBlock* next;
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

char*
Memory_Pool_Alloc(MemoryPool* pool, size_t size);

#endif // __TINY_DB_MEMORY_POOL