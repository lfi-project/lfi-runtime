#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "elfdefinitions.h"
#include "lfiv.h"
#include "args.h"
#include "argtable3.h"

static struct LFIVOptions opts;

static char *
archname(const char *s)
{
#ifdef ARCH_X64
    if (strcmp(s, "amd64") == 0 || strcmp(s, "x64") == 0 || strcmp(s, "x86_64") == 0)
        return "x64";
#endif
#ifdef ARCH_ARM64
    if (strcmp(s, "arm64") == 0 || strcmp(s, "aarch64") == 0)
        return "arm64";
#endif
    return NULL;
}

static inline
long long unsigned time_ns()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        exit(1);
    }
    return ((long long unsigned)ts.tv_sec) * 1000000000LLU +
        (long long unsigned)ts.tv_nsec;
}

static bool
verify(struct LFIVerifier *v, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "failed to open %s: %s\n", filename, strerror(errno));
        return false;
    }

    Elf64_Ehdr ehdr;
    if (fread(&ehdr, 1, sizeof(ehdr), file) != sizeof(ehdr)) {
        fprintf(stderr, "failed to read ELF header: %s\n", strerror(errno));
        goto err;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 || ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "not a valid 64-bit ELF file\n");
        goto err;
    }

    switch (ehdr.e_machine) {
    case EM_X86_64:
#ifdef ARCH_X64
        if (!args.arch || strcmp(args.arch, "x64") == 0) {
            v->verify = lfiv_verify_x64;
            break;
        }
#endif
        fprintf(stderr, "error: ELF file is x64, not %s\n", args.arch);
        goto err;
    case EM_AARCH64:
#ifdef ARCH_ARM64
        if (!args.arch || strcmp(args.arch, "arm64") == 0) {
            v->verify = lfiv_verify_arm64;
            break;
        }
#endif
        fprintf(stderr, "error: ELF file is arm64, not %s\n", args.arch);
        goto err;
    default:
        fprintf(stderr, "ELF architecture is not x64 or arm64\n");
        goto err;
    }

    assert(v->verify != NULL);

    size_t total = 0;

    long long unsigned t1 = time_ns();
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        if (fseek(file, ehdr.e_phoff + i * sizeof(Elf64_Phdr), SEEK_SET) != 0) {
            fprintf(stderr, "seek failed: %s\n", strerror(errno));
            goto err;
        }

        Elf64_Phdr phdr;
        if (fread(&phdr, 1, sizeof(phdr), file) != sizeof(phdr)) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            goto err;
        }

        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
            void *segment = malloc(phdr.p_filesz);
            if (!segment) {
                fprintf(stderr, "error: out of memory\n");
                goto err;
            }

            if (fseek(file, phdr.p_offset, SEEK_SET) != 0) {
                fprintf(stderr, "seek failed: %s\n", strerror(errno));
                free(segment);
                goto err;
            }

            if (fread(segment, 1, phdr.p_filesz, file) != phdr.p_filesz) {
                fprintf(stderr, "read failed: %s\n", strerror(errno));
                free(segment);
                goto err;
            }

            if (!lfiv_verify(v, segment, phdr.p_filesz, phdr.p_vaddr)) {
                fprintf(stderr, "verification failed\n");
                return false;
            }

            total += phdr.p_filesz;

            free(segment);
        }
    }
    long long unsigned elapsed = time_ns() - t1;

    fclose(file);
    printf("verification passed (%.1f MiB/s)\n", ((float) total / ((float) elapsed / 1000 / 1000 / 1000)) / 1024 / 1024);
    return true;
err:
    fclose(file);
    return false;
}

struct Args args;

static void
showerr(char *msg, size_t sz)
{
    (void) sz;
    fprintf(stderr, "%s\n", msg);
}

int
main(int argc, char **argv)
{
    struct arg_lit *help = arg_lit0("h", "help", "show help");
    struct arg_str *arch = arg_strn("a", "arch", "ARCH", 0, 1, "run on architecture (x64,arm64)");
    struct arg_int *n = arg_intn("n", "n", "NUM", 0, 1, "run the verifier n times (for benchmarking)");
    struct arg_str *sandbox = arg_strn("s", "sandbox", "TYPE", 0, 1, "select sandbox type (full,stores)");
    struct arg_str *inputs = arg_strn(NULL, NULL, "<input>", 0, 1000, "input files");
    struct arg_end *end = arg_end(20);

    void *argtable[] = {
        help,
        arch,
        n,
        sandbox,
        inputs,
        end,
    };

    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "memory allocation error\n");
        return 1;
    }

    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        return 1;
    }

    if (help->count > 0 || inputs->count == 0) {
        printf("Usage: %s [OPTION...] INPUT...\n\n", argv[0]);
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        return 0;
    }

    args.n = n->count > 0 ? n->ival[0] : 1;
    if (arch->count > 0) {
        args.arch = archname(arch->sval[0]);
        if (!args.arch) {
            fprintf(stderr, "unknown architecture: %s\n", arch->sval[0]);
            return 1;
        }
    }
    if (sandbox->count > 0) {
        if (strcmp(sandbox->sval[0], "full") == 0)
            opts.box = LFI_BOX_FULL;
        else if (strcmp(sandbox->sval[0], "stores") == 0)
            opts.box = LFI_BOX_STORES;
        else {
            fprintf(stderr, "unsupported sandbox type: %s\n", sandbox->sval[0]);
            return 1;
        }
    }

    opts.err = showerr;
    struct LFIVerifier v = (struct LFIVerifier) {
        .opts = opts,
    };

    bool failed = false;
    for (size_t i = 0; i < inputs->count; i++) {
        if (!verify(&v, inputs->sval[i]))
            failed = true;
    }
    if (failed)
        return 1;

    return 0;
}
