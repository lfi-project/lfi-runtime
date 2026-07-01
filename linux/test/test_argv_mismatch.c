#include "lfi_linux.h"
#include "test.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

char *
xasprintf(const char *fmt, ...);

struct Buf {
    void *data;
    size_t size;
};

int
main(int argc, const char **argv)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .no_verify = false,
            .verbose = true,
        },
        1);
    assert(engine);

    char cwd[FILENAME_MAX];
    char *d = getcwd(cwd, sizeof(cwd));
    assert(d == cwd);

    char *map = xasprintf("/=%s", cwd);
    const char *maps[] = {
        map,
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

    struct LFILinuxProc *proc = lfi_proc_new(linux_);
    assert(proc);

    bool ok = lfi_proc_load_file(proc, argv[1]);
    assert(ok);

    const char *envp[] = {
        "LFI=1",
        "USER=sandbox",
        "HOME=/home",
        NULL,
    };

    // Replicate the repro case: construct argv with a trailing NULL
    // and pass the total array size (including the NULL) as argc.
    int sandbox_nargv = argc - 1;
    const char *sandbox_argv[sandbox_nargv + 1];
    for (int i = 0; i < sandbox_nargv; i++) {
        sandbox_argv[i] = argv[i + 1];
    }
    sandbox_argv[sandbox_nargv] = NULL;

    struct LFILinuxThread *t = lfi_thread_new(proc,
        sizeof(sandbox_argv) / sizeof(sandbox_argv[0]), &sandbox_argv[0],
        &envp[0]);
    assert(t);

    int result = lfi_thread_run(t);
    assert(result == 0);

    lfi_thread_free(t);

    lfi_proc_free(proc);

    lfi_linux_free(linux_);

    lfi_free(engine);

    free(map);

    return 0;
}
