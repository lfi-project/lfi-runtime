#ifndef LIBMMAP_ADDR_SPACE_H
#define LIBMMAP_ADDR_SPACE_H

#include "range_map.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace mmap {

struct MapInfo {
  int prot;
  int flags;
  int fd;
  int64_t offset;
  bool original;

  bool operator==(const MapInfo &other) const {
    return prot == other.prot && flags == other.flags && fd == other.fd &&
           offset == other.offset && original == other.original;
  }
};

enum class Error { kOk, kInval, kNoMem };

using UpdateFn = std::function<void(uintptr_t, size_t, MapInfo)>;

struct AddrSpace {
  bool init(uintptr_t start, size_t len, size_t pagesize);
  void reset();

  uintptr_t map_any(uintptr_t hint, size_t len, int prot, int flags, int fd,
                    int64_t offset);
  uintptr_t map_at(uintptr_t addr, size_t len, int prot, int flags, int fd,
                   int64_t offset, UpdateFn ufn = nullptr);

  Error unmap(uintptr_t addr, size_t len, UpdateFn ufn = nullptr);
  bool query_page(uintptr_t addr, MapInfo *info) const;
  Error protect(uintptr_t addr, size_t len, int prot, UpdateFn ufn = nullptr);

  void mark_original();
  void unmap_non_original(UpdateFn ufn = nullptr);

private:
  uint64_t to_page(uint64_t addr) const { return addr >> p2pagesize_; }
  uint64_t to_page_ceil(uint64_t len) const {
    uint64_t pages = len >> p2pagesize_;
    uint64_t mask = (1ULL << p2pagesize_) - 1;
    if (len & mask)
      pages++;
    return pages;
  }
  uintptr_t to_addr(uint64_t page) const { return page << p2pagesize_; }
  void check_in_region(uintptr_t addr, size_t len) const;
  bool is_valid(uint64_t start, uint64_t len) const {
    if (start < base_)
      return false;
    if (len > base_ + len_ - start)
      return false;
    return true;
  }

  uint64_t base_;
  uint64_t len_;
  size_t p2pagesize_;
  RangeMap<uint64_t, MapInfo> regions_;
};

} // namespace mmap

#endif // LIBMMAP_ADDR_SPACE_H
