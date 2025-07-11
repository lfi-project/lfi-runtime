#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>

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

int
_lfi_errno(void)
{
    return errno;
}

void *
_lfi_dlopen(const char *filename, int flags)
{
    return dlopen(filename, flags);
}

void *
_lfi_dlsym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

void *
_lfi_malloc(size_t size)
{
    return malloc(size);
}

void *
_lfi_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void *
_lfi_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void
_lfi_free(void *ptr)
{
    free(ptr);
}

void *symbols[] = {
    &_lfi_thread_create,
    &_lfi_thread_destroy,
    &_lfi_errno,
    &_lfi_dlopen,
    &_lfi_dlsym,
    &_lfi_malloc,
    &_lfi_realloc,
    &_lfi_calloc,
    &_lfi_free,
};

int
main(void)
{
    lfi_pause(NULL);
}
