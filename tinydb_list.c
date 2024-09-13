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
  if (!list) {
    DB_Log(DB_LOG_WARNING, "HPList_Create failed, could not allocate memory");
    return NULL;
  }

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
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "HPList_FreeNode could not free the node it was NULL");
    return;
  }

  if (node->type == TYPE_STRING && node->value.string_value) {
    size_t string_size = strlen(node->value.string_value) + 1;
    Memory_Pool_Free(&list->string_pool, node->value.string_value, string_size);
    node->value.string_value = NULL;
  }

  // add node to the reuse pool
  if (list->freed_node_count < MAX_FREED_NODES) {
    list->freed_nodes[list->freed_node_count++] = node;
  } else {
    // we need to free the node if reuse pool is full
    Memory_Pool_Free(&list->node_pool, node, sizeof(ListNode));
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
  if (!list) {
    DB_Log(DB_LOG_WARNING,
           "HPList_Destroy could not free the list it was NULL");
    return;
  }

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
  if (!node) {
    DB_Log(DB_LOG_WARNING, "create_node_int could not create a node");
    return NULL;
  }

  node->type = TYPE_INT;
  node->value.int_value = value;
  node->next = node->prev = NULL;
  return node;
}

ListNode*
create_node_float(HPLinkedList* list, double value)
{
  ListNode* node = reuse_or_create_node(list);
  if (!node) {
    DB_Log(DB_LOG_WARNING, "create_node_float could not create a node");
    return NULL;
  }

  node->type = TYPE_FLOAT;
  node->value.float_value = value;
  node->next = node->prev = NULL;
  return node;
}

ListNode*
create_node_string(HPLinkedList* list, const char* value)
{
  if (!value) {
    DB_Log(DB_LOG_WARNING, "create_node_string could not create a node");
    return NULL;
  }

  size_t value_length = strlen(value);
  if (value_length > MAX_STRING_LENGTH) {
    DB_Log(DB_LOG_WARNING,
           "CREATE_NODE_STRING Buffer was way too long. Current Max=%d",
           MAX_STRING_LENGTH);
    return NULL;
  }

  ListNode* node = reuse_or_create_node(list);
  if (!node) {
    DB_Log(
      DB_LOG_WARNING,
      "In create_node_string, reuse_or_create_node failed to create node.");
    return NULL;
  }

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
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "HPList_LPush could not access the node. node was NULL");
    return 0;
  }

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
  ListNode* node = create_node_int(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_int failed while trying to RPUSH the int");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush_Float(HPLinkedList* list, double value)
{
  ListNode* node = create_node_float(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_float failed while trying to RPUSH the float");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush_String(HPLinkedList* list, const char* value)
{
  ListNode* node = create_node_string(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_string failed while trying to RPUSH the string");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_RPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_Int(HPLinkedList* list, int64_t value)
{
  ListNode* node = create_node_int(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_int failed while trying to LPUSH the int");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_LPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_Float(HPLinkedList* list, double value)
{
  ListNode* node = create_node_float(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_float failed while trying to LPUSH the float");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_LPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_LPush_String(HPLinkedList* list, const char* value)
{
  ListNode* node = create_node_string(list, value);
  if (!node) {
    DB_Log(DB_LOG_WARNING,
           "create_node_string failed while trying to LPUSH the string");
    return -1;
  }

  pthread_rwlock_wrlock(&list->rwlock);
  int32_t result = HPList_LPush(list, node);
  pthread_rwlock_unlock(&list->rwlock);
  return result;
}

int32_t
HPList_RPush(HPLinkedList* list, ListNode* node)
{
  if (!node)
    return list->count;

  if (list->tail) {
    list->tail->next = node;
    node->prev = list->tail;
    list->tail = node;
  } else {
    list->head = list->tail = node;
  }

  list->count++;
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

  size_t estimated_size = list->count * 32;
  char** elements = (char**)malloc(list->count * sizeof(char*));
  if (!elements) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  size_t total_size = 2; // for '[' and ']'
  size_t index = 0;
  ListNode* current = list->head;

  while (current) {
    char* element_str = NULL;
    size_t len = 0;

    if (current->type == TYPE_STRING) {
      len = strlen(current->value.string_value) +
            3; // for quotes and null terminator
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "\"%s\"", current->value.string_value);
      }
    } else if (current->type == TYPE_INT) {
      len = 32;
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "%" PRId64, current->value.int_value);
      }
    } else if (current->type == TYPE_FLOAT) {
      len = 32;
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "%f", current->value.float_value);
      }
    }

    if (element_str) {
      elements[index++] = element_str;
      total_size += strlen(element_str) + 2; // For ', '
    }

    current = current->next;
  }

  pthread_rwlock_unlock(&list->rwlock);
  char* result = (char*)malloc(total_size + 1); // +1 for null terminator
  if (!result) {
    for (size_t i = 0; i < index; i++) {
      free(elements[i]);
    }
    free(elements);
    return NULL;
  }

  char* ptr = result;
  *ptr++ = '[';

  for (size_t i = 0; i < index; i++) {
    if (i > 0) {
      *ptr++ = ',';
      *ptr++ = ' ';
    }
    size_t len = strlen(elements[i]);
    memcpy(ptr, elements[i], len);
    ptr += len;
    free(elements[i]);
  }

  *ptr++ = ']';
  *ptr = '\0';

  free(elements);
  return result;
}

char*
HPList_RangeToString(HPLinkedList* list, int32_t start, int32_t stop)
{
  if (!list || list->count == 0) {
    return strdup("[]");
  }

  if (start < 0)
    start = 0;
  if (stop >= list->count)
    stop = list->count - 1;
  if (start > stop)
    return strdup("[]");

  pthread_rwlock_rdlock(&list->rwlock);

  int32_t range_count = stop - start + 1;
  char** elements = (char**)malloc(range_count * sizeof(char*));
  if (!elements) {
    pthread_rwlock_unlock(&list->rwlock);
    return NULL;
  }

  size_t total_size = 2; // for '[' and ']'
  int32_t index = 0;
  int32_t element_index = 0;
  ListNode* current = list->head;

  // skip to the starting index
  while (current && index < start) {
    current = current->next;
    index++;
  }

  while (current && index <= stop) {
    char* element_str = NULL;
    size_t len = 0;

    if (current->type == TYPE_STRING) {
      len = strlen(current->value.string_value) +
            3; // for quotes and null terminator
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "\"%s\"", current->value.string_value);
      }
    } else if (current->type == TYPE_INT) {
      len = 32;
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "%" PRId64, current->value.int_value);
      }
    } else if (current->type == TYPE_FLOAT) {
      len = 32;
      element_str = (char*)malloc(len);
      if (element_str) {
        snprintf(element_str, len, "%f", current->value.float_value);
      }
    }

    if (element_str) {
      elements[element_index++] = element_str;
      total_size += strlen(element_str) + 2; // for ', '
    }

    current = current->next;
    index++;
  }

  pthread_rwlock_unlock(&list->rwlock);

  char* result = (char*)malloc(total_size + 1); // +1 for null terminator
  if (!result) {
    for (int32_t i = 0; i < element_index; i++) {
      free(elements[i]);
    }
    free(elements);
    return NULL;
  }

  char* ptr = result;
  *ptr++ = '[';

  for (int32_t i = 0; i < element_index; i++) {
    if (i > 0) {
      *ptr++ = ',';
      *ptr++ = ' ';
    }
    size_t len = strlen(elements[i]);
    memcpy(ptr, elements[i], len);
    ptr += len;
    free(elements[i]);
  }

  *ptr++ = ']';
  *ptr = '\0';

  free(elements);
  return result;
}
