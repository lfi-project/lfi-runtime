#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "lfi_linux.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifdef UBENCH_BASELINES
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#endif

const char *library = "tools/lfi-ubench/libbench.lfi";

struct Buf {
    void *data;
    size_t size;
};

static struct Buf
readfile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return (struct Buf) { 0 };
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    assert(p != (void *) -1);
    fclose(f);
    return (struct Buf) {
        .data = p,
        .size = sz,
    };
}

static void
callback(void)
{
}

#ifdef UBENCH_BASELINES
// native_call is a baseline for the cost of an ordinary (non-sandboxed) host
// function call. The empty volatile asm is a side effect that prevents the
// compiler from eliding the call, and noinline forces a real call to be made.
__attribute__((noinline)) static void
native_call(void)
{
    __asm__ volatile("");
}

// pin_cpu pins the calling process to a single CPU so that the context-switch
// benchmark measures actual switches rather than parallel execution on
// separate cores.
static void
pin_cpu(int cpu)
{
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
#else
    (void) cpu;
#endif
}
#endif // UBENCH_BASELINES

static inline uint64_t
time_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        exit(1);
    }
    return ((uint64_t) ts.tv_sec) * 1000000000LLU + (uint64_t) ts.tv_nsec;
}

static bool
has_bench(const char **benches, const char *bench)
{
    while (*benches) {
        if (strcmp(*benches, bench) == 0)
            return true;
        benches++;
    }
    return false;
}

#define BENCHMARK(name, iters, expr)                                    \
    if (argc == 1 || has_bench(argv, name)) {                           \
        printf("%s... ", name);                                         \
        uint64_t start = time_ns();                                     \
        for (size_t i = 0; i < (iters); ++i) {                          \
            expr;                                                       \
        }                                                               \
        uint64_t end = time_ns();                                       \
        double time_per_iter = (double) (end - start) / (double) iters; \
        printf("%f ns\n", time_per_iter);                               \
    }

// BENCHMARK_INNER is for benchmarks whose loop runs inside the sandbox: expr
// is a single invocation that itself iterates `iters` times, so the per-call
// trampoline overhead is amortized away and only the inner operation is timed.
#define BENCHMARK_INNER(name, iters, expr)                              \
    if (argc == 1 || has_bench(argv, name)) {                           \
        printf("%s... ", name);                                         \
        uint64_t start = time_ns();                                     \
        expr;                                                           \
        uint64_t end = time_ns();                                       \
        double time_per_iter = (double) (end - start) / (double) iters; \
        printf("%f ns\n", time_per_iter);                               \
    }

int
main(int argc, const char **argv)
{
    size_t pagesize = getpagesize();
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = 4UL * 1024 * 1024 * 1024,
            .pagesize = pagesize,
            .no_verify = false,
            .verbose = true,
        },
        1);
    assert(engine);

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = 2UL * 1024 * 1024,
            .exit_unknown_syscalls = true,
        });
    assert(linux_);

    struct Buf prog = readfile(library);
    assert(prog.data);

    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);

    bool ok = lfi_proc_load(proc, prog.data, prog.size, library);
    assert(ok);

    lfi_box_init_ret(lfi_proc_box(proc));

    const char *box_argv[] = {
        library,
        NULL,
    };

    const char *box_envp[] = {
        "LFI=1",
        "USER=sandbox",
        "HOME=/home",
        NULL,
    };

    struct LFILinuxThread *t = lfi_thread_new(proc, 1, &box_argv[0],
        &box_envp[0]);
    assert(t);

    int result = lfi_thread_run(t);
    assert(result == 0);

    size_t iters = 100000000;

    lfiptr fn;

    // These loop inside the sandbox (see lib.c): a single invocation runs all
    // `iters` iterations, so the measured time excludes the per-call trampoline
    // overhead and reflects only the operation itself.
    fn = lfi_proc_sym(proc, "bench_tls");
    BENCHMARK_INNER("bench_tls", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, int, (size_t),
            iters));

    fn = lfi_proc_sym(proc, "bench_syscall");
    BENCHMARK_INNER("bench_syscall", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, int, (size_t),
            iters));

    // bench_call still invokes once per iteration: it measures the trampoline
    // call overhead itself (the baseline the inner-loop benchmarks exclude).
    fn = lfi_proc_sym(proc, "bench_call");
    BENCHMARK("bench_call", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, void, (void) ));

    void *box_callback = lfi_box_register_cb(lfi_proc_box(proc),
        (void *) callback);

    fn = lfi_proc_sym(proc, "bench_callback");
    BENCHMARK_INNER("bench_callback", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, void,
            (void (*)(void), size_t), box_callback, iters));

#ifdef UBENCH_BASELINES
    // Host baselines for comparison with the sandboxed benchmarks above.

    // An ordinary host function call (baseline for bench_call/bench_callback).
    BENCHMARK("native_call", iters, native_call());

    // A real host system call (baseline for bench_syscall). We invoke the raw
    // syscall to bypass glibc's cached getpid() and force a kernel trap.
    BENCHMARK("host_syscall", iters, (void) syscall(SYS_getpid));

    // A context switch between two host processes. A byte is ping-ponged
    // between this process and a forked child over a pair of pipes, with both
    // processes pinned to the same CPU so each pipe operation forces a switch.
    // One iteration is a round trip and so involves two context switches.
    if (argc == 1 || has_bench(argv, "host_ctxswitch")) {
        size_t ctx_iters = 1000000;

        int p2c[2], c2p[2];
        int rc = pipe(p2c);
        assert(rc == 0);
        rc = pipe(c2p);
        assert(rc == 0);

        pid_t pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            // Child: echo each byte back to the parent.
            pin_cpu(0);
            close(p2c[1]);
            close(c2p[0]);
            char b;
            while (read(p2c[0], &b, 1) == 1) {
                if (write(c2p[1], &b, 1) != 1)
                    break;
            }
            _exit(0);
        }

        pin_cpu(0);
        close(p2c[0]);
        close(c2p[1]);

        char b = 0;
        printf("host_ctxswitch... ");
        uint64_t start = time_ns();
        for (size_t i = 0; i < ctx_iters; ++i) {
            ssize_t n = write(p2c[1], &b, 1);
            assert(n == 1);
            n = read(c2p[0], &b, 1);
            assert(n == 1);
        }
        uint64_t end = time_ns();
        double time_per_iter = (double) (end - start) / (double) ctx_iters;
        printf("%f ns (round trip, 2 context switches)\n", time_per_iter);

        close(p2c[1]);
        close(c2p[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
#endif // UBENCH_BASELINES

    lfi_thread_free(t);

    lfi_proc_free(proc);

    lfi_linux_free(linux_);

    lfi_free(engine);

    return 0;
}
