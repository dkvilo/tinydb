#include "tinydb_hash.h"

static const uint32_t kernel[] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
  0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
  0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
  0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
  0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t
Right_Rot(uint32_t value, uint32_t count)
{
  return value >> count | value << (32 - count);
}

static void
Init_Buffer_State(Buffer_State* state, const void* input, size_t len)
{
  state->p = input;
  state->len = len;
  state->total_len = len;
  state->single_one_delivered = 0;
  state->total_len_delivered = 0;
}

static int
Calculate_Chunk(uint8_t chunk[CHUNK_SIZE], Buffer_State* state)
{
  size_t space_in_chunk;

  if (state->total_len_delivered) {
    return 0;
  }

  if (state->len >= CHUNK_SIZE) {
    memcpy(chunk, state->p, CHUNK_SIZE);
    state->p += CHUNK_SIZE;
    state->len -= CHUNK_SIZE;
    return 1;
  }

  memcpy(chunk, state->p, state->len);
  chunk += state->len;
  space_in_chunk = CHUNK_SIZE - state->len;
  state->p += state->len;
  state->len = 0;

  if (!state->single_one_delivered) {
    *chunk++ = 0x80;
    space_in_chunk--;
    state->single_one_delivered = 1;
  }

  if (space_in_chunk >= TOTAL_LEN_LEN) {
    const size_t left = space_in_chunk - TOTAL_LEN_LEN;
    size_t len = state->total_len;
    int i;
    memset(chunk, 0x00, left);
    chunk += left;

    chunk[7] = (uint8_t)(len << 3);
    len >>= 5;
    for (i = 6; i >= 0; i--) {
      chunk[i] = (uint8_t)len;
      len >>= 8;
    }
    state->total_len_delivered = 1;
  } else {
    memset(chunk, 0x00, space_in_chunk);
  }

  return 1;
}

static void
SHA256_Transform(uint32_t* h, const uint8_t* chunk)
{
  uint32_t ah[8];
  uint32_t w[64];
  const uint8_t* p = chunk;
  int i;
  int j;

  for (i = 0; i < 16; i++) {
    w[i] = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
           (uint32_t)p[3];
    p += 4;
  }

  for (i = 16; i < 64; i++) {
    const uint32_t s0 =
      Right_Rot(w[i - 15], 7) ^ Right_Rot(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 =
      Right_Rot(w[i - 2], 17) ^ Right_Rot(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  for (i = 0; i < 8; i++)
    ah[i] = h[i];

  for (i = 0; i < 64; i++) {
    const uint32_t s1 =
      Right_Rot(ah[4], 6) ^ Right_Rot(ah[4], 11) ^ Right_Rot(ah[4], 25);
    const uint32_t ch = (ah[4] & ah[5]) ^ (~ah[4] & ah[6]);
    const uint32_t temp1 = ah[7] + s1 + ch + kernel[i] + w[i];
    const uint32_t s0 =
      Right_Rot(ah[0], 2) ^ Right_Rot(ah[0], 13) ^ Right_Rot(ah[0], 22);
    const uint32_t maj = (ah[0] & ah[1]) ^ (ah[0] & ah[2]) ^ (ah[1] & ah[2]);
    const uint32_t temp2 = s0 + maj;

    ah[7] = ah[6];
    ah[6] = ah[5];
    ah[5] = ah[4];
    ah[4] = ah[3] + temp1;
    ah[3] = ah[2];
    ah[2] = ah[1];
    ah[1] = ah[0];
    ah[0] = temp1 + temp2;
  }

  for (i = 0; i < 8; i++)
    h[i] += ah[i];
}

void
SHA256(uint8_t hash[32], const void* input, size_t len)
{
  uint32_t h[] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
  uint8_t chunk[64];
  Buffer_State state;

  Init_Buffer_State(&state, input, len);

  while (Calculate_Chunk(chunk, &state)) {
    SHA256_Transform(h, chunk);
  }

  for (int i = 0; i < 8; i++) {
    hash[i * 4 + 0] = (uint8_t)(h[i] >> 24);
    hash[i * 4 + 1] = (uint8_t)(h[i] >> 16);
    hash[i * 4 + 2] = (uint8_t)(h[i] >> 8);
    hash[i * 4 + 3] = (uint8_t)h[i];
  }
}

uint64_t
DJB2_Hash_String(const char* str)
{
  uint64_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}
