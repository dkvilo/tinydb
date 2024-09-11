#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tinydb_list.h"
#include "tinydb_log.h"
#include "tinydb_utils.h"

HPLinkedList*
HPList_Create()
{
  HPLinkedList* list = (HPLinkedList*)malloc(sizeof(HPLinkedList));
  if (!list)
    return NULL;

  list->head = list->tail = NULL;
  list->count = 0;
  list->freed_node_count = 0;
  pthread_rwlock_init(&list->rwlock, NULL);
  Memory_Pool_Init(&list->node_pool);
  Memory_Pool_Init(&list->string_pool);
  return list;
}

void
HPList_FreeNode(HPLinkedList* list, ListNode* node)
{
  if (!node)
    return;
  if (node->type == TYPE_STRING) {
    // Memory_Pool_Free(&list->string_pool, node->value.string_value);
    node->value.string_value = NULL;
  }

  // do not free the node, add to the reuse pool
  if (list->freed_node_count < MAX_FREED_NODES) {
    list->freed_nodes[list->freed_node_count++] = node;
  }
}

void
HPList_LazyFreeNodes(HPLinkedList* list)
{
  while (list->freed_node_count > 0) {
    HPList_FreeNode(list, list->freed_nodes[--list->freed_node_count]);
  }
}

void
HPList_Destroy(HPLinkedList* list)
{
  if (!list)
    return;

  pthread_rwlock_wrlock(&list->rwlock);
  ListNode* current = list->head;
  while (current) {
    ListNode* next = current->next;
    HPList_FreeNode(list, current);
    current = next;
  }
  HPList_LazyFreeNodes(list);
  pthread_rwlock_unlock(&list->rwlock);
  pthread_rwlock_destroy(&list->rwlock);
  Memory_Pool_Destroy(&list->node_pool);
  Memory_Pool_Destroy(&list->string_pool);
  free(list);
}

ListNode*
reuse_or_create_node(HPLinkedList* list)
{
  if (list->freed_node_count > 0) {
    ListNode* node = list->freed_nodes[--list->freed_node_count];
    node->next = node->prev = NULL;
    return node;
  }

  return (ListNode*)Memory_Pool_Alloc(&list->node_pool, sizeof(ListNode));
}

ListNode*
create_node_int(HPLinkedList* list, int64_t value)
{
  ListNode* node = reuse_or_create_node(list);
  if (!node)
    return NULL;

  node->type = TYPE_INT;
  node->value.int_value = value;
  node->next = node->prev = NULL;
  return node;
}

ListNode*
create_node_float(HPLinkedList* list, double value)
{
  ListNode* node = reuse_or_create_node(list);

  if (!node)
    return NULL;

  node->type = TYPE_FLOAT;
  node->value.float_value = value;
  node->next = node->prev = NULL;
  return node;
}

ListNode*
create_node_string(HPLinkedList* list, const char* value)
{
  if (!value)
    return NULL;

  size_t value_length = strlen(value);
  if (value_length > MAX_STRING_LENGTH) {
    DB_Log(DB_LOG_WARNING,
           "CREATE_NODE_STRING Buffer was way too long. Current Max=%d",
           MAX_STRING_LENGTH);
    return NULL;
  }

  ListNode* node = reuse_or_create_node(list);
  if (!node)
    return NULL;

  char* new_value =
    (char*)Memory_Pool_Alloc(&list->string_pool, value_length + 1);
  if (!new_value) {
    DB_Log(DB_LOG_WARNING, "CREATE_NODE_STRING Memory allocation failed");
    if (list->freed_node_count < MAX_FREED_NODES) {
      list->freed_nodes[list->freed_node_count++] = node;
    }
    return NULL;
  }

  strncpy(new_value, value, value_length);
  new_value[value_length] = '\0';

  node->type = TYPE_STRING;
  node->value.string_value = new_value;
  node->next = node->prev = NULL;
  return node;
}

int32_t
HPList_LPush(HPLinkedList* list, ListNode* node)
{
  if (!node)
    return 0;

  if (list->head) {
    list->head->prev = node;
    node->next = list->head;
    list->head = node;
  } else {
    list->head = list->tail = node;
  }

  list->count++;
  return list->count;
}

int32_t
HPList_RPush_Int(HPLinkedList* list, int64_t value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, create_node_int(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush_Float(HPLinkedList* list, double value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, create_node_float(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush_String(HPLinkedList* list, const char* value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, create_node_string(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_Int(HPLinkedList* list, int64_t value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_LPush(list, create_node_int(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_Float(HPLinkedList* list, double value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_LPush(list, create_node_float(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_String(HPLinkedList* list, const char* value)
{
  pthread_rwlock_wrlock(&list->rwlock);
  int result = HPList_LPush(list, create_node_string(list, value));
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush(HPLinkedList* list, ListNode* node)
{
  if (!node)
    return 0;

  if (list->tail) {
    list->tail->next = node;
    node->prev = list->tail;
    list->tail = node;
  } else {
    list->head = list->tail = node;
  }

  list->count++;
  HPList_LazyFreeNodes(list);
  return list->count;
}

ListNode*
HPList_RPop(HPLinkedList* list)
{
  pthread_rwlock_wrlock(&list->rwlock);

  if (!list->tail) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  ListNode* node = list->tail;

  if (list->head == list->tail) {
    list->head = list->tail = NULL;
  } else {
    list->tail = list->tail->prev;
    list->tail->next = NULL;
  }

  list->count--;
  node->next = node->prev = NULL;

  // add node to the reuse pool
  if (list->freed_node_count < MAX_FREED_NODES) {
    list->freed_nodes[list->freed_node_count++] = node;
  } else {
    HPList_FreeNode(list, node);
  }

  pthread_rwlock_unlock(&list->rwlock);
  return node;
}

ListNode*
HPList_LPop(HPLinkedList* list)
{
  pthread_rwlock_wrlock(&list->rwlock);

  if (!list->head) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  ListNode* node = list->head;

  if (list->head == list->tail) {
    list->head = list->tail = NULL;
  } else {
    list->head = list->head->next;
    list->head->prev = NULL;
  }

  list->count--;
  node->next = node->prev = NULL;

  // add node to the reuse pool
  if (list->freed_node_count < MAX_FREED_NODES) {
    list->freed_nodes[list->freed_node_count++] = node;
  } else {
    HPList_FreeNode(list, node);
  }

  pthread_rwlock_unlock(&list->rwlock);
  return node;
}

// temporary solution, we will convert to bytes in future
char*
HPList_ToString(HPLinkedList* list)
{
  if (!list || list->count == 0) {
    return strdup("[]");
  }

  pthread_rwlock_rdlock(&list->rwlock);

  // first pass: calculate the buffer size
  size_t buffer_size = 2; // for '[' and ']'
  ListNode* current = list->head;
  while (current) {
    if (current != list->head) {
      buffer_size += 2; // for ", "
    }

    if (current->type == TYPE_STRING) {
      buffer_size += strlen(current->value.string_value) + 2; // +2 for quotes
    } else if (current->type == TYPE_INT) {
      buffer_size += snprintf(NULL, 0, "%" PRId64, current->value.int_value);
    } else if (current->type == TYPE_FLOAT) {
      buffer_size += snprintf(NULL, 0, "%f", current->value.float_value);
    }

    current = current->next;
  }
  buffer_size++; // reserve for null terminator
  char* buffer = (char*)malloc(buffer_size);
  if (!buffer) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  // second pass: fill the buffer
  char* ptr = buffer;
  *ptr++ = '[';

  current = list->head;
  while (current) {
    if (current != list->head) {
      *ptr++ = ',';
      *ptr++ = ' ';
    }

    if (current->type == TYPE_STRING) {
      ptr += sprintf(ptr, "\"%s\"", current->value.string_value);
    } else if (current->type == TYPE_INT) {
      ptr += sprintf(ptr, "%" PRId64, current->value.int_value);
    } else if (current->type == TYPE_FLOAT) {
      ptr += sprintf(ptr, "%f", current->value.float_value);
    }

    current = current->next;
  }

  *ptr++ = ']';
  *ptr = '\0';

  pthread_rwlock_unlock(&list->rwlock);
  return buffer;
}

// temporary solution, we will convert to bytes in future
char*
HPList_RangeToString(HPLinkedList* list, int32_t start, int32_t stop)
{
  if (stop > list->count || stop > list->count)
  {
    stop = list->count;
  }

  if (!list || list->count == 0 || start >= list->count || start > stop) {
    return strdup("[]");
  }

  pthread_rwlock_rdlock(&list->rwlock);

  // first pass: calculate the buffer size
  size_t buffer_size = 2; // for '[' and ']'
  ListNode* current = list->head;
  int index = 0;

  // skip to the starting index
  while (current && index < start) {
    current = current->next;
    index++;
  }

  // iterate through the range [start, stop]
  while (current && index <= stop) {
    if (index != start) {
      buffer_size += 2; // for ", "
    }

    if (current->type == TYPE_STRING) {
      buffer_size += strlen(current->value.string_value) + 2; // +2 for quotes
    } else if (current->type == TYPE_INT) {
      buffer_size += snprintf(NULL, 0, "%" PRId64, current->value.int_value);
    } else if (current->type == TYPE_FLOAT) {
      buffer_size += snprintf(NULL, 0, "%f", current->value.float_value);
    }

    current = current->next;
    index++;
  }

  buffer_size++; // reserve for null terminator
  char* buffer = (char*)malloc(buffer_size);
  if (!buffer) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  // second pass: fill the buffer
  char* ptr = buffer;
  *ptr++ = '[';

  // reset index and current node to starting point
  current = list->head;
  index = 0;
  while (current && index < start) {
    current = current->next;
    index++;
  }

  // iterate through the range [start, stop] and fill the buffer
  while (current && index <= stop) {
    if (index != start) {
      *ptr++ = ',';
      *ptr++ = ' ';
    }

    if (current->type == TYPE_STRING) {
      ptr += sprintf(ptr, "\"%s\"", current->value.string_value);
    } else if (current->type == TYPE_INT) {
      ptr += sprintf(ptr, "%" PRId64, current->value.int_value);
    } else if (current->type == TYPE_FLOAT) {
      ptr += sprintf(ptr, "%f", current->value.float_value);
    }

    current = current->next;
    index++;
  }

  *ptr++ = ']';
  *ptr = '\0';

  pthread_rwlock_unlock(&list->rwlock);
  return buffer;
}
