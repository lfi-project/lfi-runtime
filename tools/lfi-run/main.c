#include "argtable3.h"
#include "lfi_linux.h"

#include <assert.h>
#include <errno.h>
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

static const char **
strarray(struct arg_str *vals)
{
    const char **array = malloc(sizeof(char *) * (vals->count + 1));
    if (!array) {
        fprintf(stderr, "failed to allocate\n");
        exit(1);
    }
    for (size_t i = 0; i < vals->count; i++) {
        array[i] = vals->sval[i];
    }
    array[vals->count] = NULL;
    return array;
}

int
main(int argc, char **argv)
{
    struct arg_lit *help = arg_lit0("h", "help", "show help");
    struct arg_lit *verbose = arg_lit0("V", "verbose", "verbose output");
    struct arg_lit *perf = arg_lit0(NULL, "perf", "enable perf support");
    struct arg_lit *no_verify = arg_lit0(NULL, "no-verify",
        "disable verification (unsafe)");
    struct arg_lit *sys_passthrough = arg_lit0(NULL, "sys-passthrough",
        "pass most system calls directly to the host (unsafe)");
    struct arg_int *pagesize = arg_intn(NULL, "pagesize", "<int>", 0, 1,
        "system page size");
    struct arg_str *envs = arg_strn(NULL, "env", "<var=val>", 0, 100,
        "set environment variable");
    struct arg_str *dirs = arg_strn(NULL, "dir", "<box=host>", 0, 100,
        "map sandbox path to host directory");
    struct arg_str *wd = arg_strn(NULL, "wd", "<dir>", 0, 1,
        "working directory within sandbox");
    struct arg_lit *unrestricted = arg_lit0("u", "unrestricted",
        "same as --sys-passthrough --no-verify --dir /=/ --wd $PWD (unsafe)");
    struct arg_str *inputs = arg_strn(NULL, NULL, "<input>", 0, 1000,
        "input command");
    struct arg_end *end = arg_end(20);

    void *argtable[] = { help, verbose, perf, no_verify, sys_passthrough,
        pagesize, envs, dirs, wd, unrestricted, inputs, end };

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("Usage: %s [OPTION...] INPUT...\n\n", argv[0]);
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        return 1;
    }

    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize->count > 0 ? pagesize->ival[0] : getpagesize(),
            .no_verify = no_verify->count > 0 || unrestricted->count > 0,
            .verbose = verbose->count > 0,
        },
        gb(32));
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

    struct LFILinuxEngine *linux_ = lfi_linux_new(engine,
        (struct LFILinuxOptions) {
            .stacksize = mb(2),
            .verbose = verbose->count > 0,
            .exit_unknown_syscalls = true,
            .dir_maps = unrestricted->count > 0 ? all : strarray(dirs),
            .wd = unrestricted->count > 0 ?
                cwd :
                (wd->count > 0 ? wd->sval[0] : NULL),
            .sys_passthrough = unrestricted->count > 0 ||
                sys_passthrough->count > 0,
            .perf = perf->count > 0,
        });
    if (!linux_) {
        fprintf(stderr, "failed to create LFI Linux engine\n");
        exit(1);
    }

    if (inputs->count == 0) {
        fprintf(stderr, "no input file provided\n");
        exit(1);
    }

    struct Buf prog = readfile(inputs->sval[0]);
    if (!prog.data) {
        fprintf(stderr, "failed to open %s: %s\n", inputs->sval[0],
            strerror(errno));
        exit(1);
    }

    struct LFIBox *box = lfi_box_new(engine);
    if (!box) {
        fprintf(stderr, "failed to create sandbox: %s\n", lfi_errmsg());
        exit(1);
    }
    struct LFILinuxProc *proc = lfi_proc_new(linux_, box);
    if (!proc) {
        fprintf(stderr, "failed to create LFI proc\n");
        exit(1);
    }

    bool ok = lfi_proc_load(proc, prog.data, prog.size);
    if (!ok) {
        fprintf(stderr, "failed to load %s\n", inputs->sval[0]);
        exit(1);
    }

    const char **box_argv = strarray(inputs);
    const char **box_envp = strarray(envs);
    struct LFILinuxThread *t = lfi_thread_new(proc, inputs->count, box_argv,
        box_envp);
    if (!t) {
        fprintf(stderr, "failed to create LFI thread\n");
        exit(1);
    }

    int result = lfi_thread_run(t);

    free(box_argv);
    free(box_envp);

    lfi_linux_free(linux_);

    lfi_free(engine);

    return result;
}
