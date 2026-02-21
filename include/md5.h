#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct{
    uint64_t size;        // Size of input in bytes
    uint32_t buffer[4];   // Current accumulation of hash
    uint8_t input[64];    // Input to be used in the next step
    uint8_t digest[16];   // Result of algorithm
} MD5Context;

void md5String(char *restrict input, uint8_t *restrict result);
void md5File(FILE *file, uint8_t *result);
