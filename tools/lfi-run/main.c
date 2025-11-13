#include "lfi_linux.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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

static void
usage(const char *prog_name)
{
    printf("Usage: %s [OPTION...] INPUT...\n\n", prog_name);
    printf("  -h, --help             show help\n");
    printf("  -V, --verbose          verbose output\n");
    printf("  --perf                 enable perf support\n");
    printf("  -v, --verify           enable verification\n");
    printf("  --no-debug             disable debugging support\n");
    printf("  -p, --sys-passthrough  pass most system calls directly to the host\n");
    printf("  --pagesize=<int>       system page size\n");
    printf("  --env=<var=val>        set environment variable\n");
    printf("  --dir=<box=host>       map sandbox path to host directory\n");
    printf("  --wd=<dir>             working directory within sandbox\n");
    printf("  -r, --restricted       apply --dir and --wd flags (default is --dir /=/ --wd $PWD for testing)\n");
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
    bool sys_passthrough = false;
    int pagesize = 0;
    const char **envs = NULL;
    int envs_count = 0;
    const char **dirs = NULL;
    int dirs_count = 0;
    const char *wd = NULL;
    bool restricted = false;

    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'V' },
        { "perf", no_argument, 0, 1 },
        { "verify", no_argument, 0, 'v' },
        { "no-debug", no_argument, 0, 2 },
        { "sys-passthrough", no_argument, 0, 'p' },
        { "pagesize", required_argument, 0, 3 },
        { "env", required_argument, 0, 4 },
        { "dir", required_argument, 0, 5 },
        { "wd", required_argument, 0, 6 },
        { "restricted", no_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hVvp:r", long_options,
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
        case 3:
            pagesize = atoi(optarg);
            break;
        case 4:
            envs_count++;
            envs = realloc(envs, sizeof(char *) * envs_count);
            envs[envs_count - 1] = optarg;
            break;
        case 5:
            dirs_count++;
            dirs = realloc(dirs, sizeof(char *) * dirs_count);
            dirs[dirs_count - 1] = optarg;
            break;
        case 6:
            wd = optarg;
            break;
        case 'r':
            restricted = true;
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
            .verbose = verbose,
        },
        4);
    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    char cwd[FILENAME_MAX];
    char *p = getcwd(cwd, sizeof(cwd));
    assert(p == cwd);

    const char *all[] = {
        "/=/",
        NULL,
    };

    dirs_count++;
    dirs = realloc(dirs, sizeof(char *) * dirs_count);
    dirs[dirs_count - 1] = NULL;

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .verbose = verbose,
            .exit_unknown_syscalls = true,
            .dir_maps = restricted ? dirs : all,
            .wd = restricted ? wd : cwd,
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

    bool ok = lfi_proc_load(proc, prog.data, prog.size, argv[optind]);
    if (!ok) {
        fprintf(stderr, "failed to load %s\n", argv[optind]);
        exit(1);
    }

    envs_count++;
    envs = realloc(envs, sizeof(char *) * envs_count);
    envs[envs_count - 1] = NULL;
    struct LFILinuxThread *t = lfi_thread_new(proc, argc - optind,
        (const char **)&argv[optind], envs);
    if (!t) {
        fprintf(stderr, "failed to create LFI thread\n");
        exit(1);
    }

    int result = lfi_thread_run(t);

    free(envs);
    free(dirs);

    lfi_linux_free(linux_);

    lfi_free(engine);

    return result;
}
