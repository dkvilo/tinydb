#ifndef __TINY_DB_HASHMAP_ITERATOR
#define __TINY_DB_HASHMAP_ITERATOR

#include "tinydb_database.h"
#include "tinydb_hashmap.h"

typedef struct HashMapIterator
{
  HashMap* map;
  size_t current_index;
} HashMapIterator;

HashMapIterator
HM_Iterator(HashMap* map);

int32_t
HM_IteratorHasNext(HashMapIterator* it);

DatabaseEntry*
HM_IteratorNext(HashMapIterator* it);

#endif // __TINY_DB_HASHMAP_ITERATOR