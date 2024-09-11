#ifndef __TINY_DB_LIST
#define __TINY_DB_LIST

#include "tinydb_memory_pool.h"
#include <pthread.h>
#include <stdint.h>

#define MAX_FREED_NODES 1000

typedef enum
{
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_STRING
} ValueType;

typedef union
{
  int64_t int_value;
  double float_value;
  char* string_value;
} Value;

typedef struct ListNode
{
  ValueType type;
  Value value;
  struct ListNode* next;
  struct ListNode* prev;
} ListNode;

typedef struct
{
  ListNode* head;
  ListNode* tail;
  size_t count;
  pthread_rwlock_t rwlock;
  MemoryPool node_pool;
  MemoryPool string_pool;
  ListNode* freed_nodes[MAX_FREED_NODES];
  size_t freed_node_count;
} HPLinkedList;

HPLinkedList*
HPList_Create();

void
HPList_Destroy(HPLinkedList* list);

int32_t
HPList_RPush(HPLinkedList* list, ListNode* node);

int32_t
HPList_RPush_Int(HPLinkedList* list, int64_t value);

int32_t
HPList_RPush_Float(HPLinkedList* list, double value);

int32_t
HPList_RPush_String(HPLinkedList* list, const char* value);

int32_t
HPList_LPush_Int(HPLinkedList* list, int64_t value);

int32_t
HPList_LPush_Float(HPLinkedList* list, double value);

int32_t
HPList_LPush_String(HPLinkedList* list, const char* value);

ListNode*
HPList_RPop(HPLinkedList* list);

ListNode*
HPList_LPop(HPLinkedList* list);

char*
HPList_ToString(HPLinkedList* list);

char*
HPList_RangeToString(HPLinkedList* list, int32_t start, int32_t stop);

#endif // __TINY_DB_LIST