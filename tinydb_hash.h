#ifndef __TINY_DB_HASH
#define __TINY_DB_HASH

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE    64
#define TOTAL_LEN_LEN 8

typedef struct Buffer_State
{
  const uint8_t* p;
  size_t len;
  size_t total_len;
  int32_t single_one_delivered;
  int32_t total_len_delivered;
} Buffer_State;

static inline uint32_t
Right_Rot(uint32_t value, uint32_t count);

static void
Init_Buffer_State(Buffer_State* state, const void* input, size_t len);

static int32_t
Calculate_Chunk(uint8_t chunk[CHUNK_SIZE], Buffer_State* state);

static void
SHA256_Transform(uint32_t* h, const uint8_t* chunk);

void
SHA256(uint8_t hash[32], const void* input, size_t len);

uint64_t
DJB2_Hash_String(const char* str);

#endif // __TINY_DB_HASH
