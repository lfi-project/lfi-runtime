#include "addr_space.h"

#include <algorithm>
#include <exception>

namespace mmap {

void AddrSpace::check_in_region(uintptr_t addr, size_t len) const {
  if (!is_valid(to_page(addr), to_page_ceil(len)))
    std::terminate();
}

bool AddrSpace::init(uintptr_t start, size_t len, size_t pagesize) {
  if (pagesize == 0 || (pagesize & (pagesize - 1)) != 0)
    return false;
  p2pagesize_ = 0;
  size_t p = pagesize;
  while (p >>= 1)
    p2pagesize_++;
  base_ = to_page(start);
  len_ = to_page_ceil(len);
  aslr_ = false;
  rand_state_ = 0;
  regions_.clear();
  return true;
}

void AddrSpace::reset() { regions_.clear(); }

void AddrSpace::enable_aslr(uint64_t seed) {
  aslr_ = true;
  rand_state_ = seed;
}

// splitmix64: statistically uniform, but not cryptographically secure, so an
// observer of previous placements can predict future ones.
uint64_t AddrSpace::next_rand() {
  uint64_t z = (rand_state_ += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

// Return a uniform value in [0, n). The modulo bias is negligible because n
// (a page count) is far below 2^64.
uint64_t AddrSpace::rand_below(uint64_t n) {
  return next_rand() % n;
}

// Choose the start page for a new mapping of 'pages' pages from the free
// gaps, or return false if no gap is large enough. With ASLR enabled, the
// position is chosen uniformly at random among all positions that fit;
// otherwise the lowest position is chosen.
bool AddrSpace::find_free(uint64_t pages, uint64_t *start) {
  auto gaps = regions_.get_gaps(base_, base_ + len_);
  if (!aslr_) {
    for (auto &gap : gaps) {
      if (gap.second - gap.first >= pages) {
        *start = gap.first;
        return true;
      }
    }
    return false;
  }
  uint64_t total = 0;
  for (auto &gap : gaps)
    if (gap.second - gap.first >= pages)
      total += gap.second - gap.first - pages + 1;
  if (total == 0)
    return false;
  uint64_t idx = rand_below(total);
  for (auto &gap : gaps) {
    if (gap.second - gap.first < pages)
      continue;
    uint64_t positions = gap.second - gap.first - pages + 1;
    if (idx < positions) {
      *start = gap.first + idx;
      return true;
    }
    idx -= positions;
  }
  return false;
}

uintptr_t AddrSpace::map_any(uintptr_t hint, size_t len, int prot, int flags,
                             int fd, int64_t offset) {
  if (len == 0)
    return (uintptr_t)-1;
  uint64_t pages = to_page_ceil(len);
  if (pages == 0)
    return (uintptr_t)-1;
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (hint != 0 && hint % pagesize == 0) {
    uint64_t start = to_page(hint);
    if (is_valid(start, pages) &&
        !regions_.overlaps(start, start + pages)) {
      regions_.insert(start, start + pages,
                      MapInfo{prot, flags, fd, offset, false});
      check_in_region(to_addr(start), len);
      return to_addr(start);
    }
  }
  uint64_t start;
  if (!find_free(pages, &start))
    return (uintptr_t)-1;
  regions_.insert(start, start + pages,
                  MapInfo{prot, flags, fd, offset, false});
  check_in_region(to_addr(start), len);
  return to_addr(start);
}

uintptr_t AddrSpace::map_at(uintptr_t addr, size_t len, int prot, int flags,
                            int fd, int64_t offset, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0 || len == 0)
    return (uintptr_t)-1;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);
  if (pages == 0)
    return (uintptr_t)-1;

  if (!is_valid(start, pages))
    return (uintptr_t)-1;

  unmap(addr, len, ufn);
  regions_.insert(start, start + pages,
                  MapInfo{prot, flags, fd, offset, false});
  check_in_region(addr, len);
  return addr;
}

Error AddrSpace::unmap(uintptr_t addr, size_t len, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0 || len == 0)
    return Error::kInval;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);
  if (pages == 0)
    return Error::kInval;

  if (!is_valid(start, pages))
    return Error::kInval;

  if (ufn) {
    uint64_t end = start + pages;
    auto overlapping = regions_.get_overlapping(start, end);
    for (auto &e : overlapping) {
      uint64_t cs = std::max(e.start, start);
      uint64_t ce = std::min(e.end, end);
      ufn(to_addr(cs), to_addr(ce) - to_addr(cs), e.val);
    }
  }

  regions_.remove(start, start + pages);
  return Error::kOk;
}

bool AddrSpace::query_page(uintptr_t addr, MapInfo *info) const {
  auto entry = regions_.find(to_page(addr));
  if (!entry)
    return false;
  *info = entry->val;
  return true;
}

Error AddrSpace::protect(uintptr_t addr, size_t len, int prot, UpdateFn ufn) {
  uint64_t pagesize = 1ULL << p2pagesize_;
  if (addr % pagesize != 0 || len == 0)
    return Error::kInval;

  uint64_t start = to_page(addr);
  uint64_t pages = to_page_ceil(len);
  if (pages == 0)
    return Error::kInval;
  uint64_t end = start + pages;

  if (!is_valid(start, pages))
    return Error::kInval;

  auto overlapping = regions_.get_overlapping(start, end);
  for (auto &e : overlapping) {
    uint64_t cs = std::max(e.start, start);
    uint64_t ce = std::min(e.end, end);
    if (ufn)
      ufn(to_addr(cs), to_addr(ce) - to_addr(cs), e.val);
    MapInfo new_info = e.val;
    new_info.prot = prot;
    regions_.remove(cs, ce);
    regions_.insert(cs, ce, new_info);
  }
  return Error::kOk;
}

void AddrSpace::mark_original() {
  regions_.update_all([](MapInfo &info) { info.original = true; });
}

void AddrSpace::unmap_non_original(UpdateFn ufn) {
  auto all = regions_.get_overlapping(base_, base_ + len_);
  for (auto &e : all) {
    if (!e.val.original)
      unmap(to_addr(e.start), to_addr(e.end) - to_addr(e.start), ufn);
  }
}

} // namespace mmap
