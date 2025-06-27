#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define LFI_SYS_pause 1024

__attribute__((noreturn)) static void *
lfi_pause(void *arg)
{
    (void) arg;
    syscall(LFI_SYS_pause);
    while (1) {
    }
}

void *
_lfi_thread_create(void)
{
    pthread_t *t = malloc(sizeof(pthread_t));
    pthread_create(t, NULL, &lfi_pause, NULL);
    return t;
}

void
_lfi_thread_destroy(void *arg)
{
    free((pthread_t *) arg);
}

void *symbols[] = {
    &_lfi_thread_create,
    &_lfi_thread_destroy,
    &malloc,
    &realloc,
    &calloc,
    &free,
};

int
main(void)
{
    lfi_pause(NULL);
}
