#include "lfi_arch.h"
#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

char *
xasprintf(const char *fmt, ...);

struct ThreadArgs {
    struct LFILinuxProc *proc;
};

static void *
thread_function(void *arg)
{
    struct ThreadArgs *args = (struct ThreadArgs *) arg;
    printf("new thread\n");

    struct LFIContext *ctx = NULL;

    int x = LFI_INVOKE(lfi_proc_box(args->proc), &ctx,
        lfi_proc_sym(args->proc, "add"), int, (int, int), 40, 2);
    assert(x == 42);
    x = LFI_INVOKE(lfi_proc_box(args->proc), &ctx,
        lfi_proc_sym(args->proc, "tls"), int, (void) );
    assert(x == 0);
    x = LFI_INVOKE(lfi_proc_box(args->proc), &ctx,
        lfi_proc_sym(args->proc, "tls"), int, (void) );
    assert(x == 1);

    return NULL;
}

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

int
main(int argc, const char **argv)
{
    size_t pagesize = getpagesize();
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .no_verify = false,
            .verbose = true,
        },
        1);
    assert(engine);

    char cwd[FILENAME_MAX];
    char *d = getcwd(cwd, sizeof(cwd));
    assert(d == cwd);
    const char *maps[] = {
        xasprintf("/=%s", cwd),
        NULL,
    };

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .verbose = true,
            .exit_unknown_syscalls = true,
            .wd = "/",
            .dir_maps = maps,
        });
    assert(linux_);

    if (argc <= 1) {
        fprintf(stderr, "no input program provided\n");
        return 1;
    }

    struct Buf prog = readfile(argv[1]);
    assert(prog.data);

    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);

    bool ok = lfi_proc_load(proc, prog.data, prog.size, argv[1]);
    assert(ok);

    lfi_box_init_ret(lfi_proc_box(proc));

    const char *envp[] = {
        "LFI=1",
        "USER=sandbox",
        "HOME=/home",
        NULL,
    };

    struct LFILinuxThread *t = lfi_thread_new(proc, argc - 1, &argv[1],
        &envp[0]);
    assert(t);

    int result = lfi_thread_run(t);
    assert(result == 0);

    printf("hello address: %lx\n", lfi_proc_sym(proc, "hello"));

    LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t),
        lfi_proc_sym(proc, "hello"), void, (void) );
    int x = LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t),
        lfi_proc_sym(proc, "add"), int, (int, int), 40, 2);
    assert(x == 42);

    lfi_linux_init_clone(t);

    struct ThreadArgs args = (struct ThreadArgs) {
        .proc = proc,
    };

    pthread_t t1, t2;
    int r = pthread_create(&t1, NULL, thread_function, (void *) &args);
    assert(r == 0);
    r = pthread_create(&t2, NULL, thread_function, (void *) &args);
    assert(r == 0);
    r = pthread_join(t1, NULL);
    assert(r == 0);
    r = pthread_join(t2, NULL);
    assert(r == 0);

    lfi_thread_free(t);

    lfi_proc_free(proc);

    lfi_linux_free(linux_);

    lfi_free(engine);

    free((void *) maps[0]);

    return 0;
}
