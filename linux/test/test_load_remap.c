// Tests lfi_proc_load_remap: the buffer stays intact, so it can be loaded more
// than once, and the loaded code is backed by the buffer's file. Runs against
// the lib.lfi target built in meson.build.

// For mremap.
#define _GNU_SOURCE

#include "lfi_arch.h"
#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NPROCS 3

#ifndef MREMAP_DONTUNMAP
#define MREMAP_DONTUNMAP 4
#endif

// Whether the kernel supports MREMAP_DONTUNMAP for file mappings (Linux 5.13+),
// so the buffer must keep its file pages.
static bool kernel_dontunmap;

static bool
probe_dontunmap(FILE *f)
{
    size_t pagesize = getpagesize();
    void *src = mmap(NULL, pagesize, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    void *dst = mmap(NULL, pagesize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0);
    assert(src != MAP_FAILED && dst != MAP_FAILED);
    void *p = mremap(src, pagesize, pagesize,
        MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_DONTUNMAP, dst);
    munmap(src, pagesize);
    munmap(dst, pagesize);
    return p != MAP_FAILED;
}

struct Loaded {
    struct LFILinuxProc *proc;
    struct LFILinuxThread *thread;
};

static struct Loaded
load(struct LFILinuxEngine *linux_, const uint8_t *data, size_t size,
    const char *path)
{
    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);
    bool ok = lfi_proc_load_remap(proc, data, size, path);
    assert(ok);

    lfiptr lfi_ret = lfi_proc_sym(proc, "_lfi_ret");
    assert(lfi_ret);
    lfi_box_register_ret(lfi_proc_box(proc), lfi_ret);

    const char *argv[] = { "lib", NULL };
    const char *envp[] = { NULL };
    struct LFILinuxThread *t = lfi_thread_new(proc, 1, argv, envp);
    assert(t);
    int result = lfi_thread_run(t);
    assert(result == 0);
    return (struct Loaded) { .proc = proc, .thread = t };
}

static void
call_add(struct Loaded *l)
{
    int x = LFI_INVOKE(lfi_proc_box(l->proc), lfi_thread_ctxp(l->thread),
        lfi_proc_sym(l->proc, "add"), int, (int, int), 10, 32);
    assert(x == 42);
}

static void
unload(struct Loaded *l)
{
    lfi_thread_free(l->thread);
    lfi_proc_free(l->proc);
}

// Returns the path of the file that the mapping containing addr maps, or ""
// for an anonymous mapping.
static const char *
mapped_file(uintptr_t addr)
{
    static char path[512];
    path[0] = '\0';
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return path;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo, hi;
        int n = 0;
        if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %n", &lo, &hi, &n) < 2)
            continue;
        if (addr >= lo && addr < hi) {
            snprintf(path, sizeof(path), "%s", line + n);
            path[strcspn(path, "\n")] = '\0';
            break;
        }
    }
    fclose(f);
    return path;
}

// Loads 'data' NPROCS times, all live at once, and checks that each copy of
// the library works and that 'data' still holds 'expect' afterwards. If
// 'path' is set, the buffer is a mapping of that file.
static void
load_many(struct LFILinuxEngine *linux_, const char *what, const uint8_t *data,
    const uint8_t *expect, size_t size, const char *path)
{
    struct Loaded loaded[NPROCS];
    for (int i = 0; i < NPROCS; i++) {
        loaded[i] = load(linux_, data, size, path);
        assert(memcmp(data, expect, size) == 0);
    }
    for (int i = 0; i < NPROCS; i++)
        call_add(&loaded[i]);

    if (path) {
        // The first load moves the buffer's own file pages, so its code is
        // backed by the file even without MREMAP_DONTUNMAP. With it, the
        // buffer keeps its file pages too, and so the later loads are also
        // backed by the file.
        uintptr_t code = (uintptr_t) lfi_proc_sym(loaded[0].proc, "add");
        const char *first = mapped_file(code);
        printf("%s: code of load 1 maps '%s'\n", what, first);
        assert(strcmp(first, path) == 0);

        bool dontunmap = true;
        for (size_t off = 0; off < size; off += getpagesize())
            if (strcmp(mapped_file((uintptr_t) data + off), path) != 0)
                dontunmap = false;
        for (int i = 1; i < NPROCS; i++) {
            code = (uintptr_t) lfi_proc_sym(loaded[i].proc, "add");
            const char *later = mapped_file(code);
            printf("%s: code of load %d maps '%s'\n", what, i + 1, later);
            if (dontunmap)
                assert(strcmp(later, path) == 0);
        }
        printf("%s: buffer %s its file pages\n", what,
            dontunmap ? "kept" : "did not keep (no MREMAP_DONTUNMAP)");
        if (kernel_dontunmap)
            assert(dontunmap);
    }

    for (int i = 0; i < NPROCS; i++)
        unload(&loaded[i]);
    printf("%s: %d loads OK, buffer intact\n", what, NPROCS);
}

int
main(int argc, const char **argv)
{
    if (argc <= 1) {
        fprintf(stderr, "no input program provided\n");
        return 1;
    }
    const char *path = argv[1];
    char *abspath = realpath(path, NULL);
    assert(abspath);

    FILE *f = fopen(path, "r");
    assert(f);
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    uint8_t *contents = malloc(size);
    assert(contents);
    fseek(f, 0, SEEK_SET);
    assert(fread(contents, 1, size, f) == size);

    kernel_dontunmap = probe_dontunmap(f);
    printf("kernel supports MREMAP_DONTUNMAP for files: %s\n",
        kernel_dontunmap ? "yes" : "no");

    size_t pagesize = getpagesize();
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) { .boxsize = gb(4), .pagesize = pagesize },
        NPROCS);
    assert(engine);
    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) { .stacksize = mb(2) });
    assert(linux_);

    // A buffer backed by the library file.
    uint8_t *filebuf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    assert(filebuf != MAP_FAILED);
    load_many(linux_, "file-backed", filebuf, contents, size, abspath);
    munmap(filebuf, size);

    // An anonymous buffer.
    size_t rounded = (size + pagesize - 1) / pagesize * pagesize;
    uint8_t *anonbuf = mmap(NULL, rounded, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(anonbuf != MAP_FAILED);
    memcpy(anonbuf, contents, size);
    load_many(linux_, "anonymous", anonbuf, contents, size, NULL);
    munmap(anonbuf, rounded);

    fclose(f);
    free(contents);
    free(abspath);
    lfi_linux_free(linux_);
    lfi_free(engine);
    printf("OK\n");
    return 0;
}
