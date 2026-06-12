#include <stddef.h>
#include <threads.h>
#include <unistd.h>

// volatile so that the read cannot be hoisted out of the loop: each iteration
// performs a real thread-local access.
volatile thread_local int x;

// These benchmarks run their loop inside the sandbox so that the cost being
// measured is the operation itself (a TLS access, a sandbox system call, or a
// callback into the host), rather than the lfi_trampoline call overhead that
// would be paid on every iteration if the loop ran on the host side. The host
// invokes each of these once and divides the elapsed time by iters.

int
bench_tls(size_t iters)
{
    int sum = 0;
    for (size_t i = 0; i < iters; i++)
        sum += x;
    return sum;
}

int
bench_syscall(size_t iters)
{
    int pid = 0;
    for (size_t i = 0; i < iters; i++)
        pid = getpid();
    return pid;
}

void
bench_call(void)
{
    return;
}

void
bench_callback(void (*fn)(void), size_t iters)
{
    for (size_t i = 0; i < iters; i++)
        fn();
}
