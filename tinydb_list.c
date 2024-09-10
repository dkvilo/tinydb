#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "tinydb_list.h"
#include "tinydb_utils.h"

#define NODE_POOL_BLOCK_SIZE 1024
#define STRING_POOL_BLOCK_SIZE 4096

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
  ListNode* node = reuse_or_create_node(list);
  if (!node)
    return NULL;
  char* new_value =
    (char*)Memory_Pool_Alloc(&list->string_pool, strlen(value) + 1);
  if (!new_value) {
    return NULL;
  }

  strcpy(new_value, value);
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

char*
HPList_ToString(HPLinkedList* list)
{
  if (!list || list->count == 0) {
    return strdup("[]");
  }

  pthread_rwlock_rdlock(&list->rwlock);

  size_t buffer_size = 1024;
  size_t buffer_len = 0;
  char* buffer = (char*)malloc(buffer_size);
  buffer[0] = '\0';

  Append_To_Buffer(&buffer, "[", &buffer_size, &buffer_len);

  ListNode* current = list->head;
  while (current) {
    char value_buffer[128];
    if (current->type == TYPE_STRING) {
      snprintf(value_buffer,
               sizeof(value_buffer),
               "\"%s\"",
               current->value.string_value);
    } else if (current->type == TYPE_INT) {
      snprintf(value_buffer,
               sizeof(value_buffer),
               "%" PRId64,
               current->value.int_value);
    } else if (current->type == TYPE_FLOAT) {
      snprintf(
        value_buffer, sizeof(value_buffer), "%f", current->value.float_value);
    }

    Append_To_Buffer(&buffer, value_buffer, &buffer_size, &buffer_len);
    if (current->next) {
      Append_To_Buffer(&buffer, ", ", &buffer_size, &buffer_len);
    }

    current = current->next;
  }

  Append_To_Buffer(&buffer, "]", &buffer_size, &buffer_len);

  pthread_rwlock_unlock(&list->rwlock);

  return buffer;
}