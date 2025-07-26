#include <threads.h>
#include <unistd.h>

thread_local int x;

int
bench_tls(void)
{
    return x;
}

int
bench_syscall(void)
{
    return getpid();
}

void
bench_call(void)
{
    return;
}

void
bench_callback(void (*fn)(void))
{
    fn();
}
