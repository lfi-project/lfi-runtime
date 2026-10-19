#include "lfi_linux.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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

static inline size_t
gb(size_t x)
{
    return x * 1024 * 1024 * 1024;
}

static inline size_t
mb(size_t x)
{
    return x * 1024 * 1024;
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
        return (struct Buf) { 0 };
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fileno(f), 0);
    fclose(f);
    if (!p) {
        return (struct Buf) { 0 };
    }
    return (struct Buf) {
        .data = p,
        .size = sz,
    };
}

// Passthrough is the default wherever the runtime supports it: in the full
// configuration on a Linux host.
#if defined(LINUX_FULL) && defined(__linux__)
#define PASSTHROUGH_DEFAULT true
#else
#define PASSTHROUGH_DEFAULT false
#endif

static void
usage(const char *prog_name)
{
    printf("Usage: %s [OPTION...] INPUT...\n\n", prog_name);
    printf("  -h, --help             show help\n");
    printf("  -V, --verbose          verbose output\n");
    printf("  --perf                 enable perf support\n");
    printf("  -v, --verify           enable verification\n");
    printf("  --no-debug             disable debugging support\n");
    printf("  -p, --sys-passthrough  pass file, time, and resource syscalls to the host (UNSAFE, requires linux_full)%s\n",
        PASSTHROUGH_DEFAULT ? " (default)" : "");
    printf("  --no-sys-passthrough   disable --sys-passthrough\n");
    printf("  --no-aslr              disable address space layout randomization\n");
    printf("  --pagesize=<int>       system page size\n");
    printf("  --env=<var=val>        set environment variable\n");
    printf("  <input>                input command\n");
}

int
main(int argc, char **argv)
{
    bool help = false;
    bool verbose = false;
    bool perf = false;
    bool verify = false;
    bool no_debug = false;
    bool sys_passthrough = PASSTHROUGH_DEFAULT;
    int pagesize = 0;
    bool no_aslr = false;
    const char **envs = NULL;
    int envs_count = 0;

    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'V' },
        { "perf", no_argument, 0, 1 },
        { "verify", no_argument, 0, 'v' },
        { "no-debug", no_argument, 0, 2 },
        { "sys-passthrough", no_argument, 0, 'p' },
        { "no-sys-passthrough", no_argument, 0, 8 },
        { "pagesize", required_argument, 0, 3 },
        { "env", required_argument, 0, 4 },
        { "no-aslr", no_argument, 0, 7 },
        { 0, 0, 0, 0 }
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hVvp", long_options,
                &option_index)) != -1) {
        switch (opt) {
        case 'h':
            help = true;
            break;
        case 'V':
            verbose = true;
            break;
        case 1:
            perf = true;
            break;
        case 'v':
            verify = true;
            break;
        case 2:
            no_debug = true;
            break;
        case 'p':
            sys_passthrough = true;
            break;
        case 8:
            sys_passthrough = false;
            break;
        case 3:
            pagesize = atoi(optarg);
            break;
        case 4:
            envs_count++;
            envs = xrealloc(envs, sizeof(char *) * envs_count);
            envs[envs_count - 1] = optarg;
            break;
        case 7:
            no_aslr = true;
            break;
        case '?':
            return 1;
        default:
            abort();
        }
    }

    if (help || argc == 1) {
        usage(argv[0]);
        return 0;
    }

    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize > 0 ? pagesize : getpagesize(),
            .no_verify = !verify,
            .no_aslr = no_aslr,
            .verbose = verbose,
        },
        4);
    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .verbose = verbose,
            .exit_unknown_syscalls = true,
            .sys_passthrough = sys_passthrough,
            .perf = perf,
            .debug = !no_debug,
        });
    if (!linux_) {
        fprintf(stderr, "failed to create LFI Linux engine\n");
        exit(1);
    }

    if (optind >= argc) {
        fprintf(stderr, "no input file provided\n");
        exit(1);
    }

    struct Buf prog = readfile(argv[optind]);
    if (!prog.data) {
        fprintf(stderr, "failed to open %s: %s\n", argv[optind],
            strerror(errno));
        exit(1);
    }

    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    if (!proc) {
        fprintf(stderr, "failed to create LFI proc\n");
        exit(1);
    }

    bool ok = lfi_proc_load_remap(proc, prog.data, prog.size, argv[optind]);
    if (!ok) {
        fprintf(stderr, "failed to load %s\n", argv[optind]);
        exit(1);
    }

    envs_count++;
    envs = xrealloc(envs, sizeof(char *) * envs_count);
    envs[envs_count - 1] = NULL;
    struct LFILinuxThread *t = lfi_thread_new(proc, argc - optind,
        (const char **)&argv[optind], envs);
    if (!t) {
        fprintf(stderr, "failed to create LFI thread\n");
        exit(1);
    }

    int result = lfi_thread_run(t);

    free(envs);

    lfi_thread_free(t);

    lfi_proc_free(proc);

    lfi_linux_free(linux_);

    lfi_free(engine);

    return result;
}
