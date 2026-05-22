// Portable pthread futex backend (pool of mutex+condvar pairs keyed by host
// address). Used on systems without a native futex/wait-on-address syscall.
//
// Not yet implemented.

#include "config.h"
#include "futex.h"
#include "linux.h"
#include "list.h"
#include "log.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

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
    struct Futex pool[CONFIG_MAX_FUTEXES];
};

struct Futexes *
futexes_new(void)
{
    struct Futexes *fxs = calloc(1, sizeof(*fxs));
    if (!fxs)
        return NULL;
    pthread_mutex_init(&fxs->lock, NULL);
    for (size_t i = 0; i < CONFIG_MAX_FUTEXES; i++) {
        struct Futex *f = &fxs->pool[i];
        pthread_mutex_init(&f->lock, NULL);
        pthread_cond_init(&f->cond, NULL);
        list_init(&f->elem);
        list_make_first(&fxs->free, &f->elem);
    }
    return fxs;
}

void
futexes_free(struct Futexes *fxs)
{
    if (!fxs)
        return;
    for (size_t i = 0; i < CONFIG_MAX_FUTEXES; i++) {
        pthread_mutex_destroy(&fxs->pool[i].lock);
        pthread_cond_destroy(&fxs->pool[i].cond);
    }
    pthread_mutex_destroy(&fxs->lock);
    free(fxs);
}

long
host_futex_wait(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val,
    struct TimeSpec *timeout)
{
    (void) t;
    (void) uaddr;
    (void) op;
    (void) val;
    (void) timeout;
    ERROR("pthread futex backend: futex_wait not implemented");
    return -LINUX_ENOSYS;
}

long
host_futex_wake(struct LFILinuxThread *t, uint32_t *uaddr, int op, uint32_t val)
{
    (void) t;
    (void) uaddr;
    (void) op;
    (void) val;
    ERROR("pthread futex backend: futex_wake not implemented");
    return -LINUX_ENOSYS;
}

long
host_futex_requeue(struct LFILinuxThread *t, uint32_t *uaddr, int op,
    uint32_t val, uint32_t val2, uint32_t *uaddr2)
{
    (void) t;
    (void) uaddr;
    (void) op;
    (void) val;
    (void) val2;
    (void) uaddr2;
    ERROR("pthread futex backend: futex_requeue not implemented");
    return -LINUX_ENOSYS;
}
