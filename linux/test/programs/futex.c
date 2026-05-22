// Exercises futex via pthread mutex + condvar. The consumer (main) blocks
// in pthread_cond_wait — which compiles down to FUTEX_WAIT on the cond's
// internal sequence counter — until the producer thread runs
// pthread_cond_signal (FUTEX_WAKE).

#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int ready = 0;
static int value = 0;

static void *
producer(void *arg)
{
    (void) arg;
    pthread_mutex_lock(&mu);
    value = 42;
    ready = 1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mu);
    return NULL;
}

int
main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, producer, NULL);

    pthread_mutex_lock(&mu);
    while (!ready)
        pthread_cond_wait(&cv, &mu);
    int v = value;
    pthread_mutex_unlock(&mu);

    pthread_join(t, NULL);
    printf("value: %d\n", v);
    return 0;
}
