#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "lfi_linux.h"

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static const char *library = "tools/lfi-scale/libscale.lfi";

struct Buf {
    void *data;
    size_t size;
};

struct Spawned {
    struct LFILinuxProc *proc;
    struct LFILinuxThread *thread;
};

static struct Buf
readfile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return (struct Buf) { 0 };
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    fclose(f);
    if (p == (void *) -1) {
        return (struct Buf) { 0 };
    }
    return (struct Buf) {
        .data = p,
        .size = sz,
    };
}

static void *
xrealloc(void *p, size_t size)
{
    void *q = realloc(p, size);
    if (!q) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return q;
}

static inline uint64_t
time_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        exit(1);
    }
    return ((uint64_t) ts.tv_sec) * 1000000000LLU + (uint64_t) ts.tv_nsec;
}

// Reads a field like "VmSize:" from /proc/self/status and returns its value
// in bytes, or 0 if unavailable (e.g., not on Linux).
static size_t
status_bytes(const char *field)
{
    FILE *f = fopen("/proc/self/status", "r");
    if (!f)
        return 0;
    char line[256];
    size_t kb = 0;
    size_t fieldlen = strlen(field);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, field, fieldlen) == 0) {
            kb = strtoull(line + fieldlen, NULL, 10);
            break;
        }
    }
    fclose(f);
    return kb * 1024;
}

static size_t
vm_size(void)
{
    return status_bytes("VmSize:");
}

static size_t
vm_rss(void)
{
    return status_bytes("VmRSS:");
}

// Returns the number of memory mappings in this process, or 0 if unavailable.
static size_t
count_maps(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return 0;
    char buf[8192];
    size_t n, lines = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            if (buf[i] == '\n')
                lines++;
        }
    }
    fclose(f);
    return lines;
}

// Returns the system's maximum number of memory mappings per process, or 0 if
// unavailable.
static size_t
max_map_count(void)
{
    FILE *f = fopen("/proc/sys/vm/max_map_count", "r");
    if (!f)
        return 0;
    size_t v = 0;
    if (fscanf(f, "%zu", &v) != 1)
        v = 0;
    fclose(f);
    return v;
}

static char *
humansize(size_t bytes, char *buf, size_t n)
{
    static const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    double v = (double) bytes;
    size_t u = 0;
    while (v >= 1024.0 && u < sizeof(units) / sizeof(units[0]) - 1) {
        v /= 1024.0;
        u++;
    }
    if (u == 0)
        snprintf(buf, n, "%zu %s", bytes, units[u]);
    else
        snprintf(buf, n, "%.1f %s", v, units[u]);
    return buf;
}

// lfi_errno() as of the end of setup. liblfi never clears its error state, so
// a value unchanged since then is stale (e.g., left over from a failed
// reservation probe) rather than the cause of a later failure.
static int base_err;

// Describes the current liblfi error, falling back to a generic message for
// failures that do not set an error code (these log details to stderr).
static const char *
failmsg(void)
{
    if (lfi_errno() == 0 || lfi_errno() == base_err)
        return "failed (see runtime log)";
    return lfi_errmsg();
}

// Attempts to reserve virtual address space for n sandboxes and reports
// whether the reservation succeeded.
static bool
try_reserve(struct LFIOptions opts, size_t n)
{
    struct LFIEngine *engine = lfi_new(opts, n);
    if (!engine)
        return false;
    lfi_free(engine);
    return true;
}

// Returns the largest number of sandboxes that the engine can reserve virtual
// address space for, or 0 if even a single sandbox cannot be reserved.
static size_t
probe_max(struct LFIOptions opts, bool verbose)
{
    // Double until reservation fails, then binary search between the last
    // success and the first failure.
    size_t lo = 0, hi = 1;
    while (hi != 0 && try_reserve(opts, hi)) {
        if (verbose)
            fprintf(stderr, "probe: %zu sandboxes: ok\n", hi);
        lo = hi;
        hi *= 2;
    }
    if (lo == 0)
        return 0;
    if (verbose)
        fprintf(stderr, "probe: %zu sandboxes: failed\n", hi);
    while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;
        if (try_reserve(opts, mid))
            lo = mid;
        else
            hi = mid;
        if (verbose)
            fprintf(stderr, "probe: %zu sandboxes: %s\n", mid,
                lo == mid ? "ok" : "failed");
    }
    return lo;
}

static void
usage(const char *prog_name)
{
    printf("Usage: %s [OPTION...]\n\n", prog_name);
    printf("  -h, --help           show help\n");
    printf("  -V, --verbose        verbose output\n");
    printf("  -n, --nsandboxes=N   reserve space for exactly N sandboxes instead of probing\n");
    printf("  -s, --spawn          after reserving, spawn sandboxes until failure\n");
    printf("  -v, --verify         enable verification of loaded code\n");
    printf("  --run                also run sandbox initialization in each spawned sandbox\n");
    printf("  --no-load            do not load a program into sandboxes (only create)\n");
    printf("  --lib=<path>         sandbox library to load (default: %s)\n", library);
    printf("  --report=N           print progress every N sandboxes, 0 to disable (default: 256)\n");
}

int
main(int argc, char **argv)
{
    bool verbose = false;
    bool do_spawn = false;
    bool do_load = true;
    bool do_run = false;
    bool verify = false;
    size_t nsandboxes = 0;
    size_t report = 256;

    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'V' },
        { "nsandboxes", required_argument, 0, 'n' },
        { "spawn", no_argument, 0, 's' },
        { "verify", no_argument, 0, 'v' },
        { "run", no_argument, 0, 1 },
        { "no-load", no_argument, 0, 2 },
        { "lib", required_argument, 0, 3 },
        { "report", required_argument, 0, 4 },
        { 0, 0, 0, 0 },
    };

    int c;
    while ((c = getopt_long(argc, argv, "hVn:sv", long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'V':
            verbose = true;
            break;
        case 'n':
            nsandboxes = strtoull(optarg, NULL, 0);
            if (nsandboxes == 0) {
                fprintf(stderr, "invalid sandbox count: %s\n", optarg);
                return 1;
            }
            break;
        case 's':
            do_spawn = true;
            break;
        case 'v':
            verify = true;
            break;
        case 1:
            do_run = true;
            break;
        case 2:
            do_load = false;
            break;
        case 3:
            library = optarg;
            break;
        case 4:
            report = strtoull(optarg, NULL, 0);
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    // Running sandbox initialization requires a loaded program.
    if (!do_load)
        do_run = false;

    struct LFIOptions opts = (struct LFIOptions) {
#ifdef LARGE_SANDBOX
        .boxsize = 1UL << (LARGE_SANDBOX_BITS),
#else
        .boxsize = 4UL * 1024 * 1024 * 1024,
#endif
        .pagesize = getpagesize(),
        .no_verify = !verify,
        .verbose = verbose,
    };

    char b1[32], b2[32];
    printf("box size: %s, mode: %s\n", humansize(opts.boxsize, b1, sizeof(b1)),
        !do_spawn     ? "probe only"
            : do_run  ? "spawn (create+load+run)"
            : do_load ? "load (create+load)"
                      : "create only");

    // Phase 1: find the maximum number of sandboxes that address space can be
    // reserved for.
    if (nsandboxes == 0) {
        printf("probing reservation limit...\n");
        fflush(stdout);
        uint64_t probe_start = time_ns();
        nsandboxes = probe_max(opts, verbose);
        uint64_t probe_end = time_ns();
        if (nsandboxes == 0) {
            fprintf(stderr,
                "error: cannot reserve space for even one sandbox: %s\n",
                lfi_errmsg());
            return 1;
        }
        printf("reservation limit: %zu sandboxes (probed in %.0f ms)\n",
            nsandboxes, (double) (probe_end - probe_start) / 1e6);
    }

    size_t vm_before = vm_size();
    struct LFIEngine *engine = lfi_new(opts, nsandboxes);
    if (!engine) {
        fprintf(stderr,
            "error: failed to reserve space for %zu sandboxes: %s\n",
            nsandboxes, lfi_errmsg());
        return 1;
    }
    base_err = lfi_errno();
    size_t reserved = vm_size() - vm_before;
    if (reserved > 0) {
        printf("reserved %s of virtual memory for %zu sandboxes (%s per sandbox)\n",
            humansize(reserved, b1, sizeof(b1)), nsandboxes,
            humansize(reserved / nsandboxes, b2, sizeof(b2)));
    }
    fflush(stdout);

    if (!do_spawn) {
        lfi_free(engine);
        return 0;
    }

    // For small sandboxes, shrink the stack so that it (and the rest of the
    // initial process image) still fits inside the box.
    size_t stacksize = 2UL * 1024 * 1024;
    if (stacksize > opts.boxsize / 4)
        stacksize = opts.boxsize / 4;

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = stacksize,
            .exit_unknown_syscalls = true,
            .verbose = verbose,
        });
    if (!linux_) {
        fprintf(stderr, "error: failed to create Linux engine\n");
        return 1;
    }

    struct Buf prog = (struct Buf) { 0 };
    if (do_load) {
        prog = readfile(library);
        if (!prog.data) {
            fprintf(stderr,
                "error: cannot read %s (use --lib=<path> or --no-load)\n",
                library);
            return 1;
        }
    }

    const char *box_argv[] = {
        library,
        NULL,
    };
    const char *box_envp[] = {
        "LFI=1",
        NULL,
    };

    // Phase 2: spawn sandboxes until failure, keeping all of them alive.
    struct Spawned *spawned = NULL;
    size_t cap = 0;
    size_t count = 0;

    char failbuf[256];
    const char *fail = NULL;
    int fail_errno = 0;
    size_t rss_before = vm_rss();
    uint64_t start = time_ns();

    for (;;) {
        if (count == cap) {
            cap = cap ? cap * 2 : 1024;
            spawned = xrealloc(spawned, cap * sizeof(struct Spawned));
        }

        errno = 0;
        struct LFILinuxProc *proc = lfi_proc_new(linux_);
        if (!proc) {
            fail_errno = errno;
            if (lfi_errno() == LFI_ERR_BOXMAP)
                snprintf(failbuf, sizeof(failbuf),
                    "lfi_proc_new: sandbox address space reservation exhausted");
            else
                snprintf(failbuf, sizeof(failbuf), "lfi_proc_new: %s",
                    lfi_errmsg());
            fail = failbuf;
            break;
        }

        struct LFILinuxThread *t = NULL;
        if (do_load) {
            errno = 0;
            if (!lfi_proc_load(proc, prog.data, prog.size, library)) {
                fail_errno = errno;
                snprintf(failbuf, sizeof(failbuf), "lfi_proc_load: %s",
                    failmsg());
                fail = failbuf;
                lfi_proc_free(proc);
                break;
            }
        }
        if (do_run) {
            lfi_box_init_ret(lfi_proc_box(proc));
            errno = 0;
            t = lfi_thread_new(proc, 1, &box_argv[0], &box_envp[0]);
            if (!t) {
                fail_errno = errno;
                snprintf(failbuf, sizeof(failbuf), "lfi_thread_new: %s",
                    failmsg());
                fail = failbuf;
                lfi_proc_free(proc);
                break;
            }
            int r = lfi_thread_run(t);
            if (r != 0) {
                snprintf(failbuf, sizeof(failbuf),
                    "sandbox initialization exited with code %d", r);
                fail = failbuf;
                lfi_thread_free(t);
                lfi_proc_free(proc);
                break;
            }
            // Check that the sandbox is actually functional by invoking a
            // function inside it.
            lfiptr fn = lfi_proc_sym(proc, "scale_ping");
            if (fn == 0) {
                snprintf(failbuf, sizeof(failbuf),
                    "symbol scale_ping not found in %s", library);
                fail = failbuf;
                lfi_thread_free(t);
                lfi_proc_free(proc);
                break;
            }
            int arg = (int) (count % 1000000);
            int got = LFI_INVOKE(lfi_proc_box(proc), lfi_thread_ctxp(t), fn,
                int, (int), arg);
            if (got != arg + 1) {
                snprintf(failbuf, sizeof(failbuf),
                    "sandbox %zu returned wrong result (%d != %d)", count, got,
                    arg + 1);
                fail = failbuf;
                lfi_thread_free(t);
                lfi_proc_free(proc);
                break;
            }
        }

        spawned[count] = (struct Spawned) { .proc = proc, .thread = t };
        count++;

        if (report != 0 && count % report == 0) {
            uint64_t now = time_ns();
            printf("spawned %zu sandboxes (vm: %s, rss: %s, maps: %zu, %.2f ms/sandbox)\n",
                count, humansize(vm_size(), b1, sizeof(b1)),
                humansize(vm_rss(), b2, sizeof(b2)), count_maps(),
                (double) (now - start) / 1e6 / (double) count);
            fflush(stdout);
        }
    }

    uint64_t end = time_ns();
    size_t vm_end = vm_size();
    size_t rss_end = vm_rss();
    size_t maps_end = count_maps();
    size_t maps_max = max_map_count();

    printf("\n== results ==\n");
    printf("sandboxes spawned: %zu (of %zu reserved)\n", count, nsandboxes);
    if (fail) {
        printf("stopped by:        %s", fail);
        if (fail_errno != 0)
            printf(" (errno: %s)", strerror(fail_errno));
        printf("\n");
    }
    if (vm_end > 0)
        printf("virtual memory:    %s\n", humansize(vm_end, b1, sizeof(b1)));
    if (count > 0 && rss_end > rss_before)
        printf("resident memory:   %s (%s per sandbox)\n",
            humansize(rss_end - rss_before, b1, sizeof(b1)),
            humansize((rss_end - rss_before) / count, b2, sizeof(b2)));
    if (maps_end > 0) {
        printf("memory mappings:   %zu", maps_end);
        if (maps_max > 0)
            printf(" (max_map_count: %zu)", maps_max);
        printf("\n");
    }
    if (count > 0)
        printf("spawn time:        %.2f ms per sandbox (%.2f s total)\n",
            (double) (end - start) / 1e6 / (double) count,
            (double) (end - start) / 1e9);
    fflush(stdout);

    // Teardown is timed as well, since freeing many sandboxes is also part of
    // scalability.
    uint64_t teardown_start = time_ns();
    for (size_t i = 0; i < count; i++) {
        if (spawned[i].thread)
            lfi_thread_free(spawned[i].thread);
        lfi_proc_free(spawned[i].proc);
    }
    lfi_linux_free(linux_);
    lfi_free(engine);
    uint64_t teardown_end = time_ns();
    if (count > 0)
        printf("teardown time:     %.2f s\n",
            (double) (teardown_end - teardown_start) / 1e9);

    free(spawned);
    if (prog.data)
        munmap(prog.data, prog.size);

    return 0;
}
