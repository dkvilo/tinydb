#include <stdlib.h>

#include "tinydb_database_entry_destructor.h"
#include "tinydb_datatype.h"
#include "tinydb_list.h"

void
Database_Entry_Destructor(void* value)
{
  if (value == NULL)
    return;

  DatabaseEntry* entry = (DatabaseEntry*)value;
  free(entry->key);

  switch (entry->type) {
    case DB_ENTRY_STRING:
      if (entry->value.string.value != NULL) {
        free(entry->value.string.value);
      }
      break;
    case DB_ENTRY_NUMBER:
      // number do not store values on heap
      break;
    case DB_ENTRY_OBJECT:
      if (entry->value.object != NULL) {
        Destroy_DB_Object(entry->value.object);
      }
      break;
    case DB_ENTRY_LIST:
      if (entry->value.list != NULL) {
        HPLinkedList_Destroy(entry->value.list);
      }
      break;
    default:
      // should never happen
      break;
  }

  free(entry);
}

void
Destroy_DB_Object(DB_Object* obj)
{
  if (obj == NULL)
    return;

  HM_Destroy(obj->fields);
  free(obj);
}

void
HPLinkedList_Destroy(HPLinkedList* list)
{
  if (list == NULL)
    return;

  ListNode* current = list->head;
  while (current != NULL) {
    ListNode* next = current->next;
    Database_Entry_Destructor(&current->value);
    free(current);
    current = next;
  }

  free(list);
}
