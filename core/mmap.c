#include "mmap.h"

#include <assert.h>
#include <stdlib.h>

#define LINUX_EINVAL 22

static bool
ispow2(uint64_t x)
{
    return (x & (x - 1)) == 0;
}

static size_t
getpow(uint64_t x)
{
    size_t r = 0;
    while (x >>= 1) {
        r++;
    }
    return r;
}

static bool
mmvalid(struct MMAddrSpace *mm, uint64_t start, size_t len)
{
    return start >= mm->base && (start + len) <= (mm->base + mm->len);
}

static uint64_t
mmtrunc(struct MMAddrSpace *mm, uint64_t addr)
{
    return addr >> mm->p2pagesize;
}

static uint64_t
mmceil(struct MMAddrSpace *mm, uint64_t addr)
{
    uint64_t align = 1 << mm->p2pagesize;
    uint64_t align_mask = align - 1;
    return ((addr + align_mask) & ~align_mask) >> mm->p2pagesize;
}

static bool
contained(uint64_t key1, uint64_t size1, uint64_t key2, uint64_t size2)
{
    return key1 >= key2 && key1 + size1 <= key2 + size2;
}

static void
node_insert_before(struct MMAddrSpace *mm, struct MMNode *n, struct MMNode *new)
{
    assert(n);
    new->next = n;
    new->prev = n->prev;
    if (n->prev) {
        n->prev->next = new;
    } else {
        mm->nodes = new;
    }
    n->prev = new;
}

static void
node_insert_after(struct MMAddrSpace *mm, struct MMNode *n, struct MMNode *new)
{
    (void) mm;
    assert(n);
    new->next = n->next;
    new->prev = n;
    if (n->next) {
        n->next->prev = new;
    }
    n->next = new;
}

static void
node_remove(struct MMAddrSpace *mm, struct MMNode *n)
{
    if (n->next) {
        n->next->prev = n->prev;
    }
    if (n->prev) {
        n->prev->next = n->next;
    } else {
        mm->nodes = n->next;
    }
    n->next = NULL;
    n->prev = NULL;
}

bool
mm_init(struct MMAddrSpace *mm, uintptr_t start, size_t len, size_t pagesize)
{
    assert(ispow2(pagesize));
    size_t p2pagesize = getpow(pagesize);
    *mm = (struct MMAddrSpace) {
        .p2pagesize = p2pagesize,
        .base = start >> p2pagesize,
        .len = len >> p2pagesize,
        .nodes = NULL,
        .cursor = NULL,
    };
    return true;
}

void
mm_free(struct MMAddrSpace *mm)
{
    size_t p2pagesize = mm->p2pagesize;
    mm_unmap(mm, mm->base << p2pagesize, mm->len << p2pagesize);
    free(mm->nodes);
    mm->nodes = NULL;
}

static uintptr_t
mm_mapany_cursor(struct MMAddrSpace *mm, size_t len, int prot, int flags,
    int fd, off_t offset, struct MMNode *cursor)
{
    len = mmceil(mm, len);

    struct MMNode *node = cursor;
    struct MMNode *end = node;
    uint64_t start = node ? node->base : mm->base;
    size_t gap = 0;
    while (start < mm->base + mm->len) {
        gap = node ? node->base - start : mm->base + mm->len - start;
        if (!node || gap >= len)
            break;
        start = node->base + node->len;
        if (!node->next)
            end = node;
        node = node->next;
    }

    if (gap >= len) {
        struct MMNode *new = malloc(sizeof(struct MMNode));
        if (!new)
            return (uintptr_t) -1;
        *new = (struct MMNode) {
            .base = start,
            .len = len,
            .info =
                (struct MMInfo) {
                    .prot = prot,
                    .flags = flags,
                    .fd = fd,
                    .offset = offset,
                },
        };
        if (node) {
            node_insert_before(mm, node, new);
        } else if (end) {
            node_insert_after(mm, end, new);
        } else {
            assert(!mm->nodes);
            mm->nodes = new;
        }

        mm->cursor = new->prev;

        return start << mm->p2pagesize;
    }

    return (uintptr_t) -1;
}

uintptr_t
mm_mapany(struct MMAddrSpace *mm, size_t len, int prot, int flags, int fd,
    off_t offset)
{
    uintptr_t p = -1;
    if (mm->cursor)
        p = mm_mapany_cursor(mm, len, prot, flags, fd, offset, mm->cursor);
    if (p != (uintptr_t) -1)
        return p;
    return mm_mapany_cursor(mm, len, prot, flags, fd, offset, mm->nodes);
}

uintptr_t
mm_mapat(struct MMAddrSpace *mm, uintptr_t addr, size_t length, int prot,
    int flags, int fd, off_t offset)
{
    return mm_mapat_cb(mm, addr, length, prot, flags, fd, offset, NULL, NULL);
}

uintptr_t
mm_mapat_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t length, int prot,
    int flags, int fd, off_t offset, UpdateFn ufn, void *udata)
{
    if (addr % (1 << mm->p2pagesize) != 0)
        return (uintptr_t) -1;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -LINUX_EINVAL;

    struct MMNode *new = malloc(sizeof(struct MMNode));
    if (!new)
        return (uintptr_t) -1;

    int r = mm_unmap_cb(mm, addr << mm->p2pagesize, length << mm->p2pagesize,
        ufn, udata);
    assert(r == 0);

    *new = (struct MMNode) {
        .base = addr,
        .len = length,
        .info =
            (struct MMInfo) {
                .prot = prot,
                .flags = flags,
                .fd = fd,
                .offset = offset,
            },
    };

    struct MMNode *node = mm->nodes;
    if (mm->cursor && mm->cursor->base <= addr)
        node = mm->cursor;
    if (!node) {
        mm->nodes = new;
    } else if (addr + length <= node->base) {
        node_insert_before(mm, node, new);
    } else {
        while (node) {
            size_t next = node->next ? node->next->base : mm->base + mm->len;
            if (addr >= node->base + node->len && addr + length <= next)
                break;
            node = node->next;
        }
        assert(node);
        node_insert_after(mm, node, new);
    }

    return addr << mm->p2pagesize;
}

struct UnmapData {
    UpdateFn fn;
    size_t p2pagesize;
};

int
mm_unmap(struct MMAddrSpace *mm, uintptr_t addr, size_t length)
{
    return mm_unmap_cb(mm, addr, length, NULL, NULL);
}

int
mm_unmap_cb(struct MMAddrSpace *mm, uintptr_t addr, size_t length, UpdateFn ufn,
    void *udata)
{
    if (addr % (1 << mm->p2pagesize) != 0 || length == 0)
        return -LINUX_EINVAL;
    addr = mmtrunc(mm, addr);
    length = mmceil(mm, length);

    if (!mmvalid(mm, addr, length))
        return -LINUX_EINVAL;

    bool used_new = false;
    struct MMNode *new = malloc(sizeof(struct MMNode));
    if (!new)
        return -1;

    struct MMNode *node = mm->nodes;

    if (mm->cursor) {
        struct MMNode *start = mm->cursor;
        while (start && start->base > addr) {
            start = start->prev;
        }
        if (start) {
            assert(start->base <= addr);
            node = start;
        }
    }

    struct MMNode *cursor = mm->cursor;
    while (node) {
        if (contained(node->base, node->len, addr, length)) {
            // Node fully contained in range.
            if (ufn)
                ufn(node->base << mm->p2pagesize, node->len << mm->p2pagesize,
                    node->info, udata);
            struct MMNode *old_node = node;
            if (mm->cursor == cursor)
                mm->cursor = node->prev;
            node = node->next;
            node_remove(mm, old_node);
            free(old_node);
        } else if (contained(addr, length, node->base, node->len)) {
            // Node fully contains the range.
            if (ufn)
                ufn(addr << mm->p2pagesize, length << mm->p2pagesize,
                    node->info, udata);
            *new = (struct MMNode) {
                .base = addr + length,
                .len = node->base + node->len - (addr + length),
                .info = node->info,
            };
            if (new->len != 0) {
                node_insert_after(mm, node, new);
                assert(!used_new);
                used_new = true;
            }
            node->len = addr - node->base;
            if (mm->cursor == cursor)
                mm->cursor = node->prev;
            struct MMNode *old_node = node;
            node = node->next;
            if (old_node->len == 0) {
                node_remove(mm, old_node);
                free(old_node);
            }
            break;
        } else if (node->base < addr && node->base + node->len > addr) {
            // Node starts before the range and ends inside it.
            size_t node_end = node->base + node->len;
            if (ufn)
                ufn(addr << mm->p2pagesize, (node_end - addr) << mm->p2pagesize,
                    node->info, udata);
            if (mm->cursor == cursor)
                mm->cursor = node;
            node->len = addr - node->base;
            node = node->next;
        } else if (node->base < addr + length &&
            node->base + node->len > addr) {
            // Node starts within the range and ends outside it.
            if (ufn)
                ufn(node->base << mm->p2pagesize,
                    (addr + length - node->base) << mm->p2pagesize, node->info,
                    udata);
            size_t node_end = node->base + node->len;
            node->base = addr + length;
            node->len = node_end - node->base;
            node = node->next;
            break;
        } else if (node->base >= addr + length) {
            // Node fully beyond the range.
            break;
        } else {
            node = node->next;
        }
    }

    if (!used_new)
        free(new);

    return 0;
}
