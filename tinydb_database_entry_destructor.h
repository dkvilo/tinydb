#ifndef __TINY_DB_DATABASE_ENTRY_DESTRUCTOR
#define __TINY_DB_DATABASE_ENTRY_DESTRUCTOR

#include "tinydb_datatype.h"

void
Database_Entry_Destructor(void* value);

void
Destroy_DB_Object(DB_Object* obj);

void
HPLinkedList_Destroy(HPLinkedList* list);

#endif // __TINY_DB_DATABASE_ENTRY_DESTRUCTOR