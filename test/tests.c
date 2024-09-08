#include <assert.h>
#include <stdio.h>

#include "../tinydb_hashmap.h"

void
Test_Create_Destroy()
{
  HashMap* map = HM_Create();
  assert(map != NULL);
  assert(map->capacity == INITIAL_CAPACITY);
  assert(atomic_load(&map->size) == 0);

  HM_Destroy(map);
  printf("Test_Create_Destroy passed.\n");
}

void
Test_Insert()
{
  HashMap* map = HM_Create();
  char* key = "test_key";
  char* value = "test_value";

  int8_t result = HM_Put(map, key, value);
  assert(result == HM_ACTION_ADDED);
  assert(atomic_load(&map->size) == 1);

  char* stored_value = (char*)HM_Get(map, key);
  assert(stored_value != NULL);
  assert(strcmp(stored_value, value) == 0);

  HM_Destroy(map);
  printf("Test_Insert passed.\n");
}

void
Test_Modify()
{
  HashMap* map = HM_Create();
  char* key = "test_key";
  char* value = "test_value";
  char* new_value = "new_value";
  HM_Put(map, key, value);

  int8_t result = HM_Put(map, key, new_value);
  assert(result == HM_ACTION_MODIFIED);

  char* stored_value = (char*)HM_Get(map, key);
  assert(stored_value != NULL);
  assert(strcmp(stored_value, new_value) == 0);

  HM_Destroy(map);
  printf("Test_Modify passed.\n");
}

void
Test_Remove()
{
  HashMap* map = HM_Create();
  char* key = "test_key";
  char* value = "test_value";

  HM_Put(map, key, value);

  int result = HM_Remove(map, key);
  assert(result == 1);
  assert(atomic_load(&map->size) == 0);

  assert(HM_Get(map, key) == NULL);

  HM_Destroy(map);
  printf("Test_Remove passed.\n");
}

void
Test_Resize()
{
  HashMap* map = HM_Create();

  char key[10];
  char value[20];

  for (int i = 0; i < 100; i++) {
    sprintf(key, "key_%d", i);
    sprintf(value, "value_%d", i);
    HM_Put(map, key, strdup(value));
  }

  for (int i = 0; i < 100; i++) {
    sprintf(key, "key_%d", i);
    char* stored_value = (char*)HM_Get(map, key);
    sprintf(value, "value_%d", i);
    assert(stored_value != NULL);
    assert(strcmp(stored_value, value) == 0);
  }

  HM_Destroy(map);
  printf("Test_Resize passed.\n");
}

int
main()
{
  printf("Hash Map\n");
  printf("-------------------------------------\n");
  Test_Create_Destroy();
  Test_Insert();
  Test_Modify();
  Test_Remove();
  Test_Resize();
  printf("-------------------------------------\n");

  printf("All tests passed.\n");
  return 0;
}
