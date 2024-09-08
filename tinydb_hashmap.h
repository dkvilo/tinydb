#ifndef __TINY_DB_HASHMAP
#define __TINY_DB_HASHMAP

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_memory_pool.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75
#define RESIZE_WORK_INCREMENT 64

typedef struct HashEntry
{
  char* key;
  void* value;
  bool is_occupied;
  bool is_deleted;
  atomic_flag is_migrating;
} HashEntry;

typedef struct HashMap
{
  HashEntry* entries;
  size_t capacity;
  atomic_size_t size;
  pthread_rwlock_t* locks;
  pthread_mutex_t resize_lock;
  atomic_bool is_resizing;
  atomic_size_t resize_progress;
  HashEntry* old_entries;
  size_t old_capacity;
  MemoryPool key_pool;
} HashMap;

HashMap*
HM_Create();

void
HM_Destroy(HashMap* map);

void*
HM_Put(HashMap* map, const char* key, void* value);

void*
HM_Get(HashMap* map, const char* key);

int
HM_Remove(HashMap* map, const char* key);

#endif // __TINY_DB_HASHMAP