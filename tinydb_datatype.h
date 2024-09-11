#ifndef __TINY_DB_DATATYPE
#define __TINY_DB_DATATYPE

#include "tinydb_list.h"
#include <stdatomic.h>

typedef uint64_t EntryID;

typedef enum
{
  DB_ENTRY_STRING,
  DB_ENTRY_NUMBER,
  DB_ENTRY_OBJECT,
  DB_ENTRY_LIST
} DB_ENTRY_TYPE;

typedef struct DB_Number
{
  atomic_int_least64_t value;
} DB_Number;

typedef struct DB_String
{
  char* value;
} DB_String;

typedef struct DB_Object
{
  void* value;
} DB_Object;

typedef union
{
  DB_Number number;
  DB_String string;
  DB_Object object;
  HPLinkedList* list;
} DB_Value;

typedef struct
{
  char* key;
  DB_Value value;
  DB_ENTRY_TYPE type;
} DatabaseEntry;

#endif // __TINY_DB_DATATYPE