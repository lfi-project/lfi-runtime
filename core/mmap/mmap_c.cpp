#include "mmap_c.h"
#include "addr_space.h"

struct MMapAddrSpace {
  mmap::AddrSpace impl;
};

static struct MMapInfo to_c(mmap::MapInfo info) {
  return {info.prot, info.flags, info.fd, info.offset, info.original};
}

static mmap::UpdateFn wrap_cb(MMapUpdateFn ufn, void *udata) {
  if (!ufn)
    return nullptr;
  return [ufn, udata](uintptr_t start, size_t len, mmap::MapInfo info) {
    ufn(start, len, to_c(info), udata);
  };
}

static enum MMapError to_c_error(mmap::Error err) {
  switch (err) {
  case mmap::Error::kOk:
    return MMAP_OK;
  case mmap::Error::kInval:
    return MMAP_INVAL;
  case mmap::Error::kNoMem:
    return MMAP_NOMEM;
  }
  return MMAP_INVAL;
}

struct MMapAddrSpace *mmap_create(uintptr_t start, size_t len,
                                  size_t pagesize) {
  auto *mm = new (std::nothrow) MMapAddrSpace;
  if (!mm)
    return nullptr;
  if (!mm->impl.init(start, len, pagesize)) {
    delete mm;
    return nullptr;
  }
  return mm;
}

void mmap_destroy(struct MMapAddrSpace *mm) { delete mm; }

void mmap_reset(struct MMapAddrSpace *mm) { mm->impl.reset(); }

uintptr_t mmap_map_any(struct MMapAddrSpace *mm, uintptr_t hint, size_t len,
                       int prot, int flags, int fd, int64_t offset) {
  return mm->impl.map_any(hint, len, prot, flags, fd, offset);
}

uintptr_t mmap_map_at(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                      int prot, int flags, int fd, int64_t offset,
                      MMapUpdateFn ufn, void *udata) {
  return mm->impl.map_at(addr, len, prot, flags, fd, offset,
                         wrap_cb(ufn, udata));
}

enum MMapError mmap_unmap(struct MMapAddrSpace *mm, uintptr_t addr, size_t len,
                          MMapUpdateFn ufn, void *udata) {
  return to_c_error(mm->impl.unmap(addr, len, wrap_cb(ufn, udata)));
}

bool mmap_query_page(const struct MMapAddrSpace *mm, uintptr_t addr,
                     struct MMapInfo *info) {
  mmap::MapInfo cpp_info;
  if (!mm->impl.query_page(addr, &cpp_info))
    return false;
  *info = to_c(cpp_info);
  return true;
}

enum MMapError mmap_protect(struct MMapAddrSpace *mm, uintptr_t addr,
                            size_t len, int prot, MMapUpdateFn ufn,
                            void *udata) {
  return to_c_error(mm->impl.protect(addr, len, prot, wrap_cb(ufn, udata)));
}

void mmap_mark_original(struct MMapAddrSpace *mm) { mm->impl.mark_original(); }

void mmap_unmap_non_original(struct MMapAddrSpace *mm, MMapUpdateFn ufn,
                             void *udata) {
  mm->impl.unmap_non_original(wrap_cb(ufn, udata));
}
