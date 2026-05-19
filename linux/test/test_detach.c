// Exercises lfi_linux_detach_thread and the deferred-free path in
// lfi_proc_free. Runs against the same lib.lfi target used by test_lib.

#include "lfi_arch.h"
#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

char *
xasprintf(const char *fmt, ...);

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

static struct LFILinuxProc *
make_proc(struct LFILinuxEngine *linux_, struct Buf prog, const char **argv,
    int argc, const char **envp)
{
    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);

    bool ok = lfi_proc_load(proc, prog.data, prog.size, argv[0]);
    assert(ok);

    lfiptr lfi_ret = lfi_proc_sym(proc, "_lfi_ret");
    assert(lfi_ret);
    lfi_box_register_ret(lfi_proc_box(proc), lfi_ret);

    struct LFILinuxThread *t = lfi_thread_new(proc, argc, argv, envp);
    assert(t);
    int result = lfi_thread_run(t);
    assert(result == 0);

    lfi_linux_init_clone(t);

    lfi_thread_free(t);
    return proc;
}

// Invokes "add" via the sandbox. The first call on a host thread lazily
// attaches it to the proc via the clone callback.
static void
invoke_add(struct LFILinuxProc *proc, struct LFIContext **ctxp, int a, int b)
{
    int x = LFI_INVOKE(lfi_proc_box(proc), ctxp,
        lfi_proc_sym(proc, "add"), int, (int, int), a, b);
    assert(x == a + b);
}

static void *
attach_detach_thread(void *arg)
{
    struct LFILinuxProc *proc = arg;

    struct LFIContext *ctx = NULL;
    invoke_add(proc, &ctx, 40, 2);

    lfi_linux_detach_thread(proc);

    // Calling detach a second time should be a noop.
    lfi_linux_detach_thread(proc);

    // Re-attach by invoking again with a fresh ctx.
    ctx = NULL;
    invoke_add(proc, &ctx, 1, 2);

    lfi_linux_detach_thread(proc);
    return NULL;
}

// Thread that never invokes into the sandbox; detach should be a noop.
static void *
noop_detach_thread(void *arg)
{
    struct LFILinuxProc *proc = arg;
    lfi_linux_detach_thread(proc);
    return NULL;
}

struct DeferredArgs {
    struct LFILinuxProc *proc;
    pthread_mutex_t lk;
    pthread_cond_t attached_cond;
    pthread_cond_t go_cond;
    bool attached;
    bool go;
};

static void *
deferred_free_thread(void *arg)
{
    struct DeferredArgs *da = arg;

    struct LFIContext *ctx = NULL;
    invoke_add(da->proc, &ctx, 1, 1);

    pthread_mutex_lock(&da->lk);
    da->attached = true;
    pthread_cond_signal(&da->attached_cond);
    while (!da->go)
        pthread_cond_wait(&da->go_cond, &da->lk);
    pthread_mutex_unlock(&da->lk);

    // After this call returns, the proc may have been freed (we hold the
    // last reference). Do not touch da->proc after this point.
    lfi_linux_detach_thread(da->proc);
    return NULL;
}

// Thread that attaches implicitly via the clone callback but never detaches
// explicitly; the pthread destructor must clean up on exit.
static void *
destructor_only_thread(void *arg)
{
    struct LFILinuxProc *proc = arg;
    struct LFIContext *ctx = NULL;
    invoke_add(proc, &ctx, 5, 6);
    return NULL;
}

// Explicit-detach scenario: a host thread attaches, detaches, re-attaches,
// detaches again. Then lfi_proc_free runs with no attached threads.
static void
test_explicit_detach(struct LFILinuxEngine *linux_, struct Buf prog,
    const char **argv, int argc, const char **envp)
{
    struct LFILinuxProc *proc = make_proc(linux_, prog, argv, argc, envp);

    pthread_t th;
    int r;

    r = pthread_create(&th, NULL, attach_detach_thread, proc);
    assert(r == 0);
    r = pthread_join(th, NULL);
    assert(r == 0);

    // Detach from the main thread (which never attached): noop.
    lfi_linux_detach_thread(proc);

    r = pthread_create(&th, NULL, noop_detach_thread, proc);
    assert(r == 0);
    r = pthread_join(th, NULL);
    assert(r == 0);

    lfi_proc_free(proc);
    printf("explicit detach OK\n");
}

// Deferred-free scenario: helper attaches and waits. Main calls free while
// helper is still attached (must defer). Helper then detaches; that last
// detach must run proc_destroy.
static void
test_deferred_free(struct LFILinuxEngine *linux_, struct Buf prog,
    const char **argv, int argc, const char **envp)
{
    struct LFILinuxProc *proc = make_proc(linux_, prog, argv, argc, envp);

    struct DeferredArgs da = { 0 };
    da.proc = proc;
    pthread_mutex_init(&da.lk, NULL);
    pthread_cond_init(&da.attached_cond, NULL);
    pthread_cond_init(&da.go_cond, NULL);

    pthread_t th;
    int r = pthread_create(&th, NULL, deferred_free_thread, &da);
    assert(r == 0);

    pthread_mutex_lock(&da.lk);
    while (!da.attached)
        pthread_cond_wait(&da.attached_cond, &da.lk);
    pthread_mutex_unlock(&da.lk);

    lfi_proc_free(proc);

    pthread_mutex_lock(&da.lk);
    da.go = true;
    pthread_cond_signal(&da.go_cond);
    pthread_mutex_unlock(&da.lk);

    r = pthread_join(th, NULL);
    assert(r == 0);

    pthread_mutex_destroy(&da.lk);
    pthread_cond_destroy(&da.attached_cond);
    pthread_cond_destroy(&da.go_cond);
    printf("deferred free OK\n");
}

// Destructor-only scenario: helper attaches, never detaches explicitly. The
// pthread destructor on thread exit must perform the cleanup.
static void
test_destructor_only(struct LFILinuxEngine *linux_, struct Buf prog,
    const char **argv, int argc, const char **envp)
{
    struct LFILinuxProc *proc = make_proc(linux_, prog, argv, argc, envp);

    pthread_t th;
    int r = pthread_create(&th, NULL, destructor_only_thread, proc);
    assert(r == 0);
    r = pthread_join(th, NULL);
    assert(r == 0);

    lfi_proc_free(proc);
    printf("destructor-only OK\n");
}

static struct LFILinuxEngine *
make_engine(struct LFIEngine *engine, const char **maps)
{
    return lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .exit_unknown_syscalls = true,
            .wd = "/",
            .dir_maps = maps,
        });
}

static struct LFIEngine *
make_lfi(size_t pagesize)
{
    return lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .no_verify = false,
        },
        1);
}

int
main(int argc, const char **argv)
{
    if (argc <= 1) {
        fprintf(stderr, "no input program provided\n");
        return 1;
    }

    size_t pagesize = getpagesize();

    char cwd[FILENAME_MAX];
    char *d = getcwd(cwd, sizeof(cwd));
    assert(d == cwd);
    char *map = xasprintf("/=%s", cwd);
    const char *maps[] = { map, NULL };

    const char *envp[] = {
        "LFI=1",
        "USER=sandbox",
        "HOME=/home",
        NULL,
    };

    struct Buf prog = readfile(argv[1]);
    assert(prog.data);

    {
        struct LFIEngine *engine = make_lfi(pagesize);
        assert(engine);
        struct LFILinuxEngine *linux_ = make_engine(engine, maps);
        assert(linux_);
        test_explicit_detach(linux_, prog, &argv[1], argc - 1, envp);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    {
        struct LFIEngine *engine = make_lfi(pagesize);
        assert(engine);
        struct LFILinuxEngine *linux_ = make_engine(engine, maps);
        assert(linux_);
        test_deferred_free(linux_, prog, &argv[1], argc - 1, envp);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    {
        struct LFIEngine *engine = make_lfi(pagesize);
        assert(engine);
        struct LFILinuxEngine *linux_ = make_engine(engine, maps);
        assert(linux_);
        test_destructor_only(linux_, prog, &argv[1], argc - 1, envp);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    munmap(prog.data, prog.size);
    free(map);
    return 0;
}
