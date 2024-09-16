#include <stdint.h>
#include <stdio.h>

int main() {
  printf("uint64_t: %llu\n", (uint64_t) -1);
  printf("uint32_t: %llu\n", (uint32_t) -1);
  printf("unsigned int: %u\n", (unsigned int) - 1);
  printf("unsigned long int: %lu\n", (unsigned long int) -1);
  printf("unsigned long long int: %llu\n", (unsigned long long int) -1);
  return 0;
}