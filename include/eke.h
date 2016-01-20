// The biggest 64bit prime
#ifndef	EKE_H
#define	EKE_H

#define P 0xffffffffffffffc5ull
#define G 5

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "md5.h"
#include "aes.h"

char * md5Hash(const char *str, int length);

// calc a * b % p , avoid 64bit overflow
static inline uint64_t mul_mod_p(uint64_t a, uint64_t b);

static inline uint64_t pow_mod_p(uint64_t a, uint64_t b);

// calc a^b % p
uint64_t powmodp(uint64_t a, uint64_t b);

uint64_t randomint64();

uint32_t randomint32();

void printBits(uint64_t num);

#endif //EKE_H
