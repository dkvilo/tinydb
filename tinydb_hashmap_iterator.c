#include "tinydb_hashmap_iterator.h"

HashMapIterator
HM_Iterator(HashMap* map)
{
  HashMapIterator it;
  it.map = map;
  it.current_index = 0;
  return it;
}

// returns 1 when valid entry and 0 when there is no valid entry
int32_t
HM_IteratorHasNext(HashMapIterator* it)
{
  HashMap* map = it->map;

  while (it->current_index < map->capacity) {
    HashEntry* entry = &map->entries[it->current_index];
    if (entry->is_occupied && !entry->is_deleted) {
      return 1;
    }
    it->current_index++;
  }

  return 0;
}

DatabaseEntry*
HM_IteratorNext(HashMapIterator* it)
{
  if (HM_IteratorHasNext(it)) {
    HashEntry* entry = &it->map->entries[it->current_index];
    it->current_index++;
    return (DatabaseEntry*)entry->value;
  }
  return NULL;
}