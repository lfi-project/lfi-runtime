// Checks randomization of the sandbox memory layout: the main thread stack
// (including its sub-page offset), the ELF load base, and the brk region.
// With no_aslr, the layout must be deterministic.

#include "lfi_arch.h"
#include "lfi_linux.h"
#include "proc.h"
#include "test.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NPROC 4

struct Buf {
    void *data;
    size_t size;
};

struct Layout {
    lfiptr stack_off;
    lfiptr load_off;
    lfiptr brk_gap;
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

static lfiptr
initial_sp(struct LFILinuxThread *t)
{
    struct LFIRegs *regs = lfi_ctx_regs(*lfi_thread_ctxp(t));
#if defined(__x86_64__) || defined(_M_X64)
    return regs->rsp;
#else
    return regs->sp;
#endif
}

// Creates a proc with a main thread and returns its layout (stack pointer,
// ELF load base, and brk gap, all relative). The thread is never run.
static struct Layout
proc_layout(struct LFILinuxEngine *linux_, struct Buf prog, const char **argv,
    int argc, const char **envp, bool no_aslr)
{
    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);
    bool ok = lfi_proc_load(proc, prog.data, prog.size, argv[0]);
    assert(ok);
    struct LFILinuxThread *t = lfi_thread_new(proc, argc, argv, envp);
    assert(t);

    struct LFIBoxInfo info = lfi_box_info(lfi_proc_box(proc));
    lfiptr sp = initial_sp(t);
    assert(sp >= info.min && sp < info.max);
    assert(proc->loadbase >= info.min && proc->loadbase < info.max);
    assert(proc->brkbase >= proc->elfinfo.lastva);

    struct Layout l = {
        .stack_off = sp - info.min,
        .load_off = proc->loadbase - info.min,
        .brk_gap = proc->brkbase - proc->elfinfo.lastva,
    };
    assert(l.brk_gap < BRKRNDSIZE);
    if (no_aslr) {
        assert(l.load_off == 0);
        assert(l.brk_gap == 0);
        // The stack goes at the top of the sandbox.
        assert(sp >= info.max - mb(2));
    }

    lfi_thread_free(t);
    lfi_proc_free(proc);

    return l;
}

static struct LFIEngine *
make_lfi(size_t pagesize, bool no_aslr, size_t nsandboxes)
{
    return lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .no_verify = false,
            .no_aslr = no_aslr,
        },
        nsandboxes);
}

static struct LFILinuxEngine *
make_engine(struct LFIEngine *engine)
{
    static const char *maps[] = { "/=/", NULL };
    return lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .exit_unknown_syscalls = true,
            .wd = "/",
            .dir_maps = maps,
        });
}

int
main(int argc, const char **argv)
{
    if (argc <= 1) {
        fprintf(stderr, "no input program provided\n");
        return 1;
    }

    size_t pagesize = getpagesize();

    const char *envp[] = {
        "LFI=1",
        NULL,
    };

    struct Buf prog = readfile(argv[1]);
    assert(prog.data);

    // With ASLR (the default), the stack, ELF base, and brk gap should differ
    // across procs.
    {
        struct LFIEngine *engine = make_lfi(pagesize, false, NPROC);
        assert(engine);
        struct LFILinuxEngine *linux_ = make_engine(engine);
        assert(linux_);
        struct Layout l[NPROC];
        for (size_t i = 0; i < NPROC; i++)
            l[i] = proc_layout(linux_, prog, &argv[1], argc - 1, envp, false);
        bool stack_equal = true;
        bool sub_page_equal = true;
        bool load_equal = true;
        bool brk_equal = true;
        for (size_t i = 1; i < NPROC; i++) {
            stack_equal = stack_equal && l[i].stack_off == l[0].stack_off;
            sub_page_equal = sub_page_equal &&
                (l[i].stack_off & (pagesize - 1)) ==
                    (l[0].stack_off & (pagesize - 1));
            load_equal = load_equal && l[i].load_off == l[0].load_off;
            brk_equal = brk_equal && l[i].brk_gap == l[0].brk_gap;
        }
        assert(!stack_equal);
        assert(!sub_page_equal);
        assert(!load_equal);
        assert(!brk_equal);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    // With no_aslr, the layout should be identical in every proc (exact
    // positions are asserted in proc_layout).
    {
        struct LFIEngine *engine = make_lfi(pagesize, true, 2);
        assert(engine);
        struct LFILinuxEngine *linux_ = make_engine(engine);
        assert(linux_);
        struct Layout l1 = proc_layout(linux_, prog, &argv[1], argc - 1, envp,
            true);
        struct Layout l2 = proc_layout(linux_, prog, &argv[1], argc - 1, envp,
            true);
        assert(l1.stack_off == l2.stack_off);
        assert(l1.load_off == l2.load_off);
        assert(l1.brk_gap == l2.brk_gap);
        lfi_linux_free(linux_);
        lfi_free(engine);
    }

    printf("layout aslr OK\n");

    return 0;
}
