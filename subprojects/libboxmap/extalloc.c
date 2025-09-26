#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "map.h"

struct ExtAlloc *
extalloc_new(uintptr_t base, size_t size, size_t chunksize)
{
    assert(base % chunksize == 0);
    assert(size % chunksize == 0);
    base /= chunksize;
    size /= chunksize;

    struct ExtAlloc *a = malloc(sizeof(struct ExtAlloc));
    if (!a)
        return NULL;
    uint8_t *bitvec = calloc(size / 8 + 1, 1);
    if (!bitvec) {
        free(a);
        return NULL;
    }
    *a = (struct ExtAlloc) {
        .base = base,
        .size = size,
        .bitvec = bitvec,
        .chunksize = chunksize,
    };
    return a;
}

static size_t
bit(uint8_t *bitvec, size_t bit)
{
    size_t byte = bit / 8;
    size_t bit_off = 7 - (bit % 8);
    return (bitvec[byte] >> bit_off) & 1;
}

static ssize_t
bitvec_find_zeroes(uint8_t *bitvec, size_t bitvec_size, size_t n)
{
    if (n <= 0 || bitvec_size == 0)
        return -1;

    size_t total_bits = bitvec_size * 8;
    size_t count = 0;

    for (size_t i = 0; i < total_bits; i++) {
        if (bit(bitvec, i) == 0) {
            count++;
            if (count == n)
                return i - n + 1;
        } else {
            count = 0;
        }
    }

    return -1;
}

static void
bitvec_set(uint8_t *bitvec, size_t start, size_t length, int val) {
    if (length == 0)
        return;

    for (size_t i = 0; i < length; i++) {
        size_t bit = start + i;
        size_t byte = bit / 8;
        size_t bit_off = 7 - (bit % 8);
        if (val)
            bitvec[byte] |= (1 << bit_off);
        else
            bitvec[byte] &= ~(1 << bit_off);
    }
}

bool
extalloc_is_full(struct ExtAlloc *a)
{
    for (size_t i = 0; i < a->size; i++) {
        if (bit(a->bitvec, i) != 1)
            return false;
    }
    return true;
}

uintptr_t
extalloc_alloc(struct ExtAlloc *a, size_t n)
{
    assert(n % a->chunksize == 0);
    n /= a->chunksize;
    ssize_t idx = bitvec_find_zeroes(a->bitvec, a->size, n);
    if (idx == -1)
        return 0;
    bitvec_set(a->bitvec, idx, n, 1);
    return (a->base + idx) * a->chunksize;
}

void
extalloc_allocat(struct ExtAlloc *a, uintptr_t at, size_t n)
{
    assert(at % a->chunksize == 0);
    assert(n % a->chunksize == 0);
    at /= a->chunksize;
    n /= a->chunksize;
    bitvec_set(a->bitvec, at - a->base, n, 1);
}

void
extalloc_free(struct ExtAlloc *a, uintptr_t at, size_t n)
{
    assert(at % a->chunksize == 0);
    assert(n % a->chunksize == 0);
    at /= a->chunksize;
    n /= a->chunksize;
    bitvec_set(a->bitvec, at - a->base, n, 0);
}

void
extalloc_delete(struct ExtAlloc *a)
{
    free(a->bitvec);
    free(a);
}
