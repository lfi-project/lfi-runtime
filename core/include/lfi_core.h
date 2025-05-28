#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef uintptr_t lfiptr;

struct LFIOptions {
    bool noverify;
};

struct LFIEngine *
lfi_new(struct LFIOptions opts);

bool
lfi_reserve(struct LFIEngine *lfi, size_t size);

void
lfi_remove(struct LFIEngine *lfi, struct LFIBox *box);

void
lfi_free(struct LFIEngine *lfi);

struct LFIBox *
lfi_box_new(struct LFIEngine *lfi);

struct LFIBoxInfo
lfi_box_info(struct LFIBox *box);

lfiptr
lfi_box_mapat(struct LFIBox *box, lfiptr addr, size_t size, int prot, int flags, int fd, off_t off);

lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd, off_t off);

int
lfi_box_munmap(struct LFIBox *box, lfiptr addr, size_t size);

int
lfi_box_mprotect(struct LFIBox *box, lfiptr addr, size_t size, int prot);

bool
lfi_box_ptrvalid(struct LFIBox *box, lfiptr addr);

bool
lfi_box_bufvalid(struct LFIBox *box, lfiptr addr, size_t size);

void *
lfi_box_copyfm(struct LFIBox *box, void *dst, lfiptr src, size_t size);

lfiptr
lfi_box_copyto(struct LFIBox *box, lfiptr dst, void *src, size_t size);

void *
lfi_box_free(struct LFIBox *box);

struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp);

int
lfi_ctx_run(struct LFIContext *ctx);

void *
lfi_ctx_free(struct LFIContext *ctx);

struct LFIRegs *
lfi_ctx_regs(struct LFIContext *ctx);

void
lfi_ctx_exit(struct LFIContext *ctx, int code);

struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx);
