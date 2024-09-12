#include "tinydb_object.h"

DB_Object*
CreateDBObject()
{
  DB_Object* obj = (DB_Object*)malloc(sizeof(DB_Object));
  obj->fields = HM_Create();
  return obj;
}

int32_t
DBObject_AddField(DB_Object* obj,
                  const char* field_name,
                  DB_Value value,
                  DB_ENTRY_TYPE type)
{
  DatabaseEntry* new_entry = (DatabaseEntry*)malloc(sizeof(DatabaseEntry));
  new_entry->key = strdup(field_name);
  new_entry->value = value;
  new_entry->type = type;

  int8_t state = HM_Put(obj->fields, new_entry->key, new_entry);
  return state;
}

DatabaseEntry*
DBObject_GetField(DB_Object* obj, const char* field_name)
{
  return (DatabaseEntry*)HM_Get(obj->fields, field_name);
}

int32_t
DBObject_RemoveField(DB_Object* obj, const char* field_name)
{
  return HM_Remove(obj->fields, field_name);
}
