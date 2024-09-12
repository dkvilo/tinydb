/**

At this point query parser is very primitive and do not support handling object creating
since i want to have binary data exchange protocol to implement there is no point of 
extending current query parser implementation. - David.

  #include "tinydb_utils.h"

  #include "tinydb_atomic_proc.h"
  #include "tinydb_hashmap_iterator.h"
  #include "tinydb_object.h"
  #include <inttypes.h>

  DB_Log(DB_LOG_INFO, "-----------------[%s]-----------------.", "ObjectExample");

  DB_Value obj_value;
  obj_value.object = CreateDBObject();
  DB_Atomic_Store(db, "user:1", obj_value, DB_ENTRY_OBJECT);

  DB_Value name_value;
  name_value.string.value = strdup("David Kviloria");

  DB_Value age_value;
  age_value.number.value = 26;

  DatabaseEntry user_entry = DB_Atomic_Get(db, "user:1");
  if (user_entry.type == DB_ENTRY_OBJECT) {
    DB_Object* user_object = user_entry.value.object;
    DBObject_AddField(user_object, "name", name_value, DB_ENTRY_STRING);
    DBObject_AddField(user_object, "age", age_value, DB_ENTRY_NUMBER);

    DB_Value email_list_value;
    email_list_value.list = HPList_Create();
    HPList_RPush_String(email_list_value.list, "david@skystargems.com");
    HPList_RPush_String(email_list_value.list, "dkviloria@gmail.com");
    DBObject_AddField(user_object, "emails", email_list_value, DB_ENTRY_LIST);
  }

  user_entry = DB_Atomic_Get(db, "user:1");
  if (user_entry.type == DB_ENTRY_OBJECT) {
    DB_Object* user_object = user_entry.value.object;
    DatabaseEntry* name_entry = DBObject_GetField(user_object, "name");
    if (name_entry && name_entry->type == DB_ENTRY_STRING) {
      printf("User name: %s\n", name_entry->value.string.value);
    }

    DatabaseEntry* age_entry = DBObject_GetField(user_object, "age");
    if (age_entry && age_entry->type == DB_ENTRY_NUMBER) {
      printf("User age: %" PRId64 "\n",
             atomic_load(&age_entry->value.number.value));
    }

    DatabaseEntry* emails_entry = DBObject_GetField(user_object, "emails");
    if (emails_entry && emails_entry->type == DB_ENTRY_LIST) {
      HPLinkedList* email_list = emails_entry->value.list;
      ListNode* current_node = email_list->head;

      printf("User emails:\n");
      while (current_node != NULL) {
        if (current_node->type == TYPE_STRING) {
          printf("  %s\n", current_node->value.string_value);
        }
        current_node = current_node->next;
      }
    }
  }

  // @note (David) Serialize_* functions are allocating buffer on heap, it's your responsibility to free them
  // anyway those functions are for testing purposes since we ere going to use binary representation.

  printf("JSONObj: %s\n", Serialize_DB_Object_ToJSON(obj_value.object));
  printf("LISTStyle:\n%s\n", Serialize_DB_Object_ToListStyleWrapper(obj_value.object));

  user_entry = DB_Atomic_Get(db, "user:1");
  if (user_entry.type == DB_ENTRY_OBJECT) {
    DB_Object* user_object = user_entry.value.object;
    DBObject_RemoveField(user_object, "age");
  }

  user_entry = DB_Atomic_Get(db, "user:1");
  if (user_entry.type == DB_ENTRY_OBJECT) {
    DB_Object* user_object = user_entry.value.object;
    DatabaseEntry* age_entry = DBObject_GetField(user_object, "age");
    if (age_entry == NULL) {
      printf("Age field removed successfully\n");
    }
  }

  printf("JSONObj: %s\n", Serialize_DB_Object_ToJSON(obj_value.object));
  printf("LISTStyle:\n%s", Serialize_DB_Object_ToListStyleWrapper(obj_value.object));
  
  DB_Log(DB_LOG_INFO, "-----------------[%s]-----------------.", "Object Example");
*/

#ifndef __TINY_DB_OBJECT
#define __TINY_DB_OBJECT

#include <stdlib.h>
#include <string.h>

#include "tinydb_datatype.h"
#include "tinydb_hashmap.h"

DB_Object*
CreateDBObject();

int32_t
DBObject_AddField(DB_Object* obj,
                  const char* field_name,
                  DB_Value value,
                  DB_ENTRY_TYPE type);

DatabaseEntry*
DBObject_GetField(DB_Object* obj, const char* field_name);

int32_t
DBObject_RemoveField(DB_Object* obj, const char* field_name);

#endif // __TINY_DB_OBJECT