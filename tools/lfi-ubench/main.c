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

    fn = lfi_proc_sym(proc, "bench_tls");
    BENCHMARK("bench_tls", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, int, (void) ));

    fn = lfi_proc_sym(proc, "bench_syscall");
    BENCHMARK("bench_syscall", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, int, (void) ));

    fn = lfi_proc_sym(proc, "bench_call");
    BENCHMARK("bench_call", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, void, (void) ));

    void *box_callback = lfi_box_register_cb(lfi_proc_box(proc),
        (void *) callback);

    fn = lfi_proc_sym(proc, "bench_callback");
    BENCHMARK("bench_callback", iters,
        LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn, void,
            (void (*)(void)), box_callback));

    lfi_thread_free(t);

    lfi_proc_free(proc);

    lfi_linux_free(linux_);

    lfi_free(engine);

    return 0;
}
