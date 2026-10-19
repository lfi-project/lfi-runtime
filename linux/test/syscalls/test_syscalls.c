// Differential syscall tests: runs each case in cases.c natively and inside
// an LFI sandbox and compares the results (see cases.h for the expectations
// of each configuration). Output is TAP.

#include "cases.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int
exit_prog_main(int argc, char **argv);

#define LINUX ((long) 0x4c494e5558) // sentinel: expect the native result
#define HOST_FAILED ((long) 0x484f5354) // sentinel: the native run failed

struct Case {
    const char *name;
    long (*host)(void);
    long expect_minimal;
    long expect_full;
};

#define CASE_ENTRY(name, min, full) { "case_" #name, case_##name, min, full },
static const struct Case cases[] = {
    SYSCALL_CASES(CASE_ENTRY)
    SYSCALL_CASES_ARCH(CASE_ENTRY)
};
#undef CASE_ENTRY

struct ExitCase {
    const char *name;
    const char *code;
    const char *how;
};

static const struct ExitCase exit_cases[] = {
    { "exit_group_0", "0", "g" },
    { "exit_group_42", "42", "g" },
    { "exit_group_300", "300", "g" },
    { "exit_7", "7", "e" },
};

static int ntests;
static int nfailed;

static void
report(const char *name, bool ok, long got, long want)
{
    ntests++;
    if (ok) {
        printf("ok %d - %s\n", ntests, name);
    } else {
        nfailed++;
        printf("not ok %d - %s # sandbox returned %ld, expected %ld\n", ntests,
            name, got, want);
    }
    fflush(stdout);
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
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    assert(p != (void *) -1);
    fclose(f);
    return (struct Buf) { .data = p, .size = sz };
}

static struct LFILinuxEngine *
make_engine(struct LFIEngine *engine)
{
    // Unknown syscalls must come back as ENOSYS rather than killing the
    // process, since several cases test exactly that.
    return lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .exit_unknown_syscalls = false,
        });
}

static struct LFIEngine *
make_lfi(void)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .no_verify = false,
        },
        1);
    assert(engine);
    return engine;
}

// Runs a case natively in a child and returns its result. Several cases map,
// unmap, or move the break at fixed addresses chosen relative to the break,
// which could clobber the driver's own memory if they ran in this process.
static long
host_result(const struct Case *c)
{
    int fds[2];
    int r = pipe(fds);
    assert(r == 0);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        close(fds[0]);
        long res = c->host();
        ssize_t n = write(fds[1], &res, sizeof(res));
        _exit(n == (ssize_t) sizeof(res) ? 0 : 1);
    }
    close(fds[1]);
    long res;
    ssize_t n = read(fds[0], &res, sizeof(res));
    close(fds[0]);
    int status;
    pid_t w = waitpid(pid, &status, 0);
    assert(w == pid);
    if (n != (ssize_t) sizeof(res) || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        fprintf(stderr, "%s: native run failed\n", c->name);
        return HOST_FAILED;
    }
    return res;
}

static long
expected(const struct Case *c)
{
#ifdef LINUX_FULL
    long e = c->expect_full;
#else
    long e = c->expect_minimal;
#endif
    if (e == LINUX)
        return host_result(c);
    return e;
}

static void
run_cases(const char *libpath)
{
    struct Buf lib = readfile(libpath);
    struct LFIEngine *engine = make_lfi();
    struct LFILinuxEngine *linux_ = make_engine(engine);
    assert(linux_);

    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);
    bool ok = lfi_proc_load(proc, lib.data, lib.size, libpath);
    assert(ok);
    lfiptr lfi_ret = lfi_proc_sym(proc, "_lfi_ret");
    assert(lfi_ret);
    lfi_box_register_ret(lfi_proc_box(proc), lfi_ret);

    const char *argv[] = { libpath, NULL };
    const char *envp[] = { NULL };
    struct LFILinuxThread *t = lfi_thread_new(proc, 1, argv, envp);
    assert(t);
    // Runs the library's startup, which pauses and hands the context back.
    int r = lfi_thread_run(t);
    assert(r == 0);

    struct LFIContext **ctxp = lfi_thread_ctxp(t);
    struct LFIBox *box = lfi_proc_box(proc);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const struct Case *c = &cases[i];
        lfiptr fn = lfi_proc_sym(proc, c->name);
        if (!fn) {
            report(c->name, false, 0, 0);
            fprintf(stderr, "%s: not found in sandbox\n", c->name);
            continue;
        }
        long want = expected(c);
        long got = LFI_INVOKE(box, ctxp, fn, long, (void));
        report(c->name, got == want, got, want);
    }

    lfi_thread_free(t);
    lfi_proc_free(proc);
    lfi_linux_free(linux_);
    lfi_free(engine);
    munmap(lib.data, lib.size);
}

// Runs exit_prog natively in a child and returns its exit status.
static int
host_exit_status(const struct ExitCase *e)
{
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        char *argv[] = { "exit_prog", (char *) e->code, (char *) e->how, NULL };
        exit_prog_main(3, argv);
        _exit(97);
    }
    int status;
    pid_t w = waitpid(pid, &status, 0);
    assert(w == pid);
    if (!WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

static void
run_exit_cases(const char *progpath)
{
    struct Buf prog = readfile(progpath);

    for (size_t i = 0; i < sizeof(exit_cases) / sizeof(exit_cases[0]); i++) {
        const struct ExitCase *e = &exit_cases[i];

        struct LFIEngine *engine = make_lfi();
        struct LFILinuxEngine *linux_ = make_engine(engine);
        assert(linux_);
        struct LFILinuxProc *proc = lfi_proc_new(linux_);
        assert(proc);
        bool ok = lfi_proc_load(proc, prog.data, prog.size, progpath);
        assert(ok);

        const char *argv[] = { progpath, e->code, e->how, NULL };
        const char *envp[] = { NULL };
        struct LFILinuxThread *t = lfi_thread_new(proc, 3, argv, envp);
        assert(t);
        // Linux truncates the exit status to 8 bits when it is reported to
        // the parent; the runtime reports the raw code.
        int got = lfi_thread_run(t) & 0xff;
        int code = 0;
        bool exited = lfi_proc_exited(proc, &code);

        int want = host_exit_status(e);
        bool group = e->how[0] == 'g';
        bool pass = got == want && exited == group &&
            (!group || (code & 0xff) == want);
        report(e->name, pass, got, want);
        if (exited != group)
            fprintf(stderr, "%s: lfi_proc_exited = %d, expected %d\n",
                e->name, exited, group);

        lfi_thread_free(t);
        lfi_proc_free(proc);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    munmap(prog.data, prog.size);
}

int
main(int argc, const char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s cases.lfi exit_prog.lfi\n", argv[0]);
        return 1;
    }

    size_t n = sizeof(cases) / sizeof(cases[0]) +
        sizeof(exit_cases) / sizeof(exit_cases[0]);
    printf("1..%zu\n", n);

    run_cases(argv[1]);
    run_exit_cases(argv[2]);

    return nfailed == 0 ? 0 : 1;
}
