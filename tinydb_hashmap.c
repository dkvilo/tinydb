/**
 * note (David)
 * /COLLISION HANDLING/
 * when collision happens we are using quadratic probing to find a free index
 * because of the nature of quadratic probing algorithm to keep it reasonably
 * performant we need to make sure that size of the buffer is always power of
 * 2 when we are creating/resizing the buffer.
 */
#include "tinydb_hashmap.h"
#include "tinydb_log.h"

static size_t
hash(const char* key, size_t capacity)
{
  size_t hash = 0;
  while (*key) {
    hash = (hash * 31) + *key;
    key++;
  }

  return hash & (capacity - 1);
}

static size_t
Quad_Probe(size_t index, size_t i, size_t cap)
{
  return (index + i * i) & (cap - 1);
}

HashMap*
HM_Create(ValueDestructor value_destructor)
{
  HashMap* map = (HashMap*)malloc(sizeof(HashMap));
  if (!map)
    return NULL;

  map->capacity = INITIAL_CAPACITY;
  atomic_init(&map->size, 0);
  map->entries = (HashEntry*)calloc(map->capacity, sizeof(HashEntry));
  map->locks =
    (pthread_rwlock_t*)malloc(map->capacity * sizeof(pthread_rwlock_t));

  if (!map->entries || !map->locks) {
    free(map->entries);
    free(map->locks);
    free(map);
    return NULL;
  }

  for (size_t i = 0; i < map->capacity; i++) {
    pthread_rwlock_init(&map->locks[i], NULL);
    atomic_flag_clear(&map->entries[i].is_migrating);
  }

  pthread_mutex_init(&map->resize_lock, NULL);
  atomic_init(&map->is_resizing, false);
  atomic_init(&map->resize_progress, 0);
  map->old_entries = NULL;
  map->old_capacity = 0;
  Memory_Pool_Init(&map->key_pool);

  map->value_destructor = value_destructor;
  return map;
}

void
HM_Destroy(HashMap* map)
{
  if (!map)
    return;

  for (size_t i = 0; i < map->capacity; i++) {
    pthread_rwlock_destroy(&map->locks[i]);
    if (map->entries[i].is_occupied && !map->entries[i].is_deleted) {
      Memory_Pool_Free(
        &map->key_pool, map->entries[i].key, map->entries[i].key_size);
      if (map->value_destructor && map->entries[i].value != NULL) {
        map->value_destructor(map->entries[i].value);
      }
    }
  }

  if (map->old_entries != NULL) {
    for (size_t i = 0; i < map->old_capacity; i++) {
      if (map->old_entries[i].is_occupied && !map->old_entries[i].is_deleted) {
        Memory_Pool_Free(&map->key_pool,
                         map->old_entries[i].key,
                         map->old_entries[i].key_size);
        if (map->value_destructor && map->old_entries[i].value != NULL) {
          map->value_destructor(map->old_entries[i].value);
        }
      }
    }
    free(map->old_entries);
  }

  pthread_mutex_destroy(&map->resize_lock);
  free(map->entries);
  free(map->locks);
  Memory_Pool_Destroy(&map->key_pool);
  free(map);
}

static void
resize_increment(HashMap* map)
{
  if (!atomic_load(&map->is_resizing))
    return;

  pthread_mutex_lock(&map->resize_lock);

  size_t start = atomic_load(&map->resize_progress);
  size_t end = start + RESIZE_WORK_INCREMENT;
  if (end > map->old_capacity)
    end = map->old_capacity;

  for (size_t i = start; i < end; i++) {
    if (map->old_entries[i].is_occupied && !map->old_entries[i].is_deleted) {

      // spin until we can migrate this entry
      while (atomic_flag_test_and_set(&map->old_entries[i].is_migrating)) {
      }

      size_t index = hash(map->old_entries[i].key, map->capacity);
      size_t j = 1;
      while (map->entries[index].is_occupied) {
        index = Quad_Probe(index, j, map->capacity);
        j++;
      }
      map->entries[index] = map->old_entries[i];
      atomic_flag_clear(&map->entries[index].is_migrating);

    } else if (map->old_entries[i].is_occupied) {
      while (atomic_flag_test_and_set(&map->old_entries[i].is_migrating)) {
      }

      Memory_Pool_Free(
        &map->key_pool, map->old_entries[i].key, map->old_entries[i].key_size);
      if (map->value_destructor && map->old_entries[i].value != NULL) {
        map->value_destructor(map->old_entries[i].value);
      }

      map->old_entries[i].key = NULL;
      map->old_entries[i].value = NULL;
      map->old_entries[i].is_occupied = false;
      map->old_entries[i].is_deleted = false;

      atomic_flag_clear(&map->old_entries[i].is_migrating);
    }
  }

  atomic_store(&map->resize_progress, end);

  if (end == map->old_capacity) {
    free(map->old_entries);
    map->old_entries = NULL;
    map->old_capacity = 0;
    atomic_store(&map->is_resizing, false);
  }

  pthread_mutex_unlock(&map->resize_lock);
}

static void
resize_if_needed(HashMap* map)
{
  float load_factor = (float)atomic_load(&map->size) / map->capacity;
  if (load_factor < LOAD_FACTOR_THRESHOLD)
    return;

  if (atomic_exchange(&map->is_resizing, true))
    return; // another thread is already resizing

  pthread_mutex_lock(&map->resize_lock);

  size_t new_capacity = map->capacity << 1; // double the capacity
  HashEntry* new_entries = (HashEntry*)calloc(new_capacity, sizeof(HashEntry));
  pthread_rwlock_t* new_locks =
    (pthread_rwlock_t*)malloc(new_capacity * sizeof(pthread_rwlock_t));

  if (!new_entries || !new_locks) {
    free(new_entries);
    free(new_locks);
    atomic_store(&map->is_resizing, false);
    pthread_mutex_unlock(&map->resize_lock);
    return;
  }

  for (size_t i = 0; i < new_capacity; i++) {
    pthread_rwlock_init(&new_locks[i], NULL);
    atomic_flag_clear(&new_entries[i].is_migrating);
  }

  map->old_entries = map->entries;
  map->old_capacity = map->capacity;
  map->entries = new_entries;
  free(map->locks);
  map->locks = new_locks;
  map->capacity = new_capacity;
  atomic_store(&map->resize_progress, 0);

  pthread_mutex_unlock(&map->resize_lock);

  resize_increment(map);
}

int8_t
HM_Put(HashMap* map, const char* key, void* value)
{
  if (key == NULL) {
    return HM_ACTION_FAILED;
  }

  resize_if_needed(map);
  resize_increment(map);

  size_t index = hash(key, map->capacity);
  size_t i = 0;

  for (;;) {
    pthread_rwlock_wrlock(&map->locks[index]);

    if (!map->entries[index].is_occupied || map->entries[index].is_deleted) {
      if (map->entries[index].is_deleted) {
        Memory_Pool_Free(&map->key_pool,
                         map->entries[index].key,
                         map->entries[index].key_size);
        if (map->value_destructor && map->entries[index].value != NULL) {
          map->value_destructor(map->entries[index].value);
        }
      } else {
        atomic_fetch_add(&map->size, 1);
      }

      size_t key_len = strlen(key) + 1;
      char* new_key = Memory_Pool_Alloc(&map->key_pool, key_len);
      memcpy(new_key, key, key_len);

      map->entries[index].key = new_key;
      map->entries[index].key_size = key_len;
      map->entries[index].value = value;
      map->entries[index].is_occupied = true;
      map->entries[index].is_deleted = false;

      pthread_rwlock_unlock(&map->locks[index]);
      return HM_ACTION_ADDED;
    }

    if (strcmp(map->entries[index].key, key) == 0) {
      if (map->value_destructor && map->entries[index].value != NULL) {
        map->value_destructor(map->entries[index].value);
      }
      map->entries[index].value = value;
      map->entries[index].is_deleted = false;
      pthread_rwlock_unlock(&map->locks[index]);
      return HM_ACTION_MODIFIED;
    }

    pthread_rwlock_unlock(&map->locks[index]);
    i++;

    index = Quad_Probe(index, i, map->capacity);
  }

  return HM_ACTION_FAILED;
}

void*
HM_Get(HashMap* map, const char* key)
{
  if (map == NULL || key == NULL) {
    DB_Log(DB_LOG_ERROR, "HashMap or key is NULL");
    return NULL;
  }

  resize_increment(map);

  size_t index = hash(key, map->capacity);
  size_t i = 0;
  size_t start_index = index;

  do {
    if (pthread_rwlock_rdlock(&map->locks[index]) != 0) {
      DB_Log(DB_LOG_ERROR, "Failed to acquire read lock for index %zu", index);
      return NULL;
    }

    if (!map->entries[index].is_occupied) {
      pthread_rwlock_unlock(&map->locks[index]);
      return NULL;
    }

    if (!map->entries[index].is_deleted &&
        strcmp(map->entries[index].key, key) == 0) {
      void* value = map->entries[index].value;
      pthread_rwlock_unlock(&map->locks[index]);
      return value;
    }

    pthread_rwlock_unlock(&map->locks[index]);
    i++;

    index = Quad_Probe(index, i, map->capacity);
  } while (index != start_index);

  return NULL;
}

int
HM_Remove(HashMap* map, const char* key)
{
  if (map == NULL || key == NULL) {
    return 0;
  }

  resize_increment(map);

  size_t index = hash(key, map->capacity);
  size_t i = 0;
  size_t start_index = index;

  do {
    pthread_rwlock_wrlock(&map->locks[index]);

    if (!map->entries[index].is_occupied) {
      pthread_rwlock_unlock(&map->locks[index]);
      return 0;
    }

    if (!map->entries[index].is_deleted &&
        strcmp(map->entries[index].key, key) == 0) {
      // waiting if the entry is being migrated
      while (atomic_flag_test_and_set(&map->entries[index].is_migrating)) {
        pthread_rwlock_unlock(&map->locks[index]);
        pthread_rwlock_wrlock(&map->locks[index]);
      }

      Memory_Pool_Free(
        &map->key_pool, map->entries[index].key, map->entries[index].key_size);
      if (map->value_destructor && map->entries[index].value != NULL) {
        map->value_destructor(map->entries[index].value);
      }

      map->entries[index].key = NULL;
      map->entries[index].value = NULL;
      map->entries[index].is_deleted = true;
      map->entries[index].is_occupied = false;

      atomic_fetch_sub(&map->size, 1);
      atomic_flag_clear(&map->entries[index].is_migrating);
      pthread_rwlock_unlock(&map->locks[index]);
      return 1;
    }

    pthread_rwlock_unlock(&map->locks[index]);
    i++;

    index = Quad_Probe(index, i, map->capacity);

  } while (index != start_index);

  return 0;
}
