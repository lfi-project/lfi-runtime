#pragma once

#include "config.h"
#include "list.h"

#include <pthread.h>
#include <stdint.h>

#define FUTEX_CONTAINER(ptr) LIST_CONTAINER(struct Futex, elem, ptr)

struct Futex {
    struct List elem;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    uintptr_t addr;
    int waiters;
};

struct Futexes {
    pthread_mutex_t lock;
    struct List *active;
    struct List *free;
    struct Futex futexes[CONFIG_MAX_FUTEXES];
};
