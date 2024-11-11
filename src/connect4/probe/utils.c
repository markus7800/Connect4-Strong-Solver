#ifndef UTILS
#define UTILS

#include <stdint.h>
#include <time.h>

uint64_t hash_64(uint64_t a) {
    a = ~a + (a << 21);
    a =  a ^ (a >> 24);
    a =  a + (a << 3) + (a << 8);
    a =  a ^ (a >> 14);
    a =  a + (a << 2) + (a << 4);
    a =  a ^ (a >> 28);
    a =  a + (a << 31);
    return a;
}

double get_elapsed_time(struct timespec t0, struct timespec t1) {
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}

#endif