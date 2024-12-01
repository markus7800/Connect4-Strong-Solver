#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

int main() {
  printf("uint64_t: %"PRIu64"\n", (uint64_t) -1);
  printf("uint32_t: %"PRIu32"\n", (uint32_t) -1);
  printf("unsigned int: %u\n", (unsigned int) - 1);
  printf("unsigned long int: %lu\n", (unsigned long int) -1);
  printf("unsigned long long int: %llu\n", (unsigned long long int) -1);


  printf("__builtin_popcount: %d\n", __builtin_popcount((uint64_t) -1));
  printf("__builtin_popcountll: %d\n", __builtin_popcountll((uint64_t) -1));

  return 0;
}