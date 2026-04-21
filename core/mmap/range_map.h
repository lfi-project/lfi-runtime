#ifndef LIBMMAP_RANGE_MAP_H
#define LIBMMAP_RANGE_MAP_H

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace mmap {

template <class K, class V> struct Entry {
  K start;
  K end;
  V val;

  bool empty() const { return start >= end; }
};

template <class K, class V> class RangeMap {
public:
  bool empty() const { return Map_.empty(); }
  size_t size() const { return Map_.size(); }
  void clear() { Map_.clear(); }

  // Find the entry containing the point 'key', or std::nullopt.
  std::optional<Entry<K, V>> find(K key) const {
    auto it = Map_.upper_bound(key);
    if (it == Map_.begin())
      return std::nullopt;
    --it;
    if (key < it->second.first)
      return Entry<K, V>{it->first, it->second.first, it->second.second};
    return std::nullopt;
  }

  // Insert range [start, end) with the given value. Overlapping ranges are
  // split or removed. Adjacent ranges with equal values are coalesced.
  void insert(K start, K end, V val) {
    if (start >= end)
      return;

    auto it = overlap_begin(start);

    std::optional<std::pair<K, std::pair<K, V>>> left_stub;
    std::optional<std::pair<K, std::pair<K, V>>> right_stub;

    while (it != Map_.end() && it->first < end) {
      K e_start = it->first;
      K e_end = it->second.first;
      V e_val = it->second.second;

      if (e_start < start)
        left_stub = {e_start, {start, e_val}};
      if (e_end > end)
        right_stub = {end, {e_end, e_val}};

      it = Map_.erase(it);
    }

    if (left_stub)
      Map_.insert(*left_stub);
    if (right_stub)
      Map_.insert(*right_stub);

    auto res = Map_.insert({start, {end, val}});
    coalesce(res.first);
  }

  // Remove all mappings within [start, end). Partially overlapping ranges
  // are trimmed/split.
  void remove(K start, K end) {
    if (start >= end)
      return;

    auto it = overlap_begin(start);

    std::optional<std::pair<K, std::pair<K, V>>> left_stub;
    std::optional<std::pair<K, std::pair<K, V>>> right_stub;

    while (it != Map_.end() && it->first < end) {
      K e_start = it->first;
      K e_end = it->second.first;
      V e_val = it->second.second;

      if (e_start < start)
        left_stub = {e_start, {start, e_val}};
      if (e_end > end)
        right_stub = {end, {e_end, e_val}};

      it = Map_.erase(it);
    }

    if (left_stub)
      Map_.insert(*left_stub);
    if (right_stub)
      Map_.insert(*right_stub);
  }

  // Return true if any stored range overlaps [start, end).
  bool overlaps(K start, K end) const {
    if (start >= end)
      return false;
    auto it = overlap_begin(start);
    return it != Map_.end() && it->first < end;
  }

  // Return all entries overlapping [start, end).
  std::vector<Entry<K, V>> get_overlapping(K start, K end) const {
    std::vector<Entry<K, V>> result;
    if (start >= end)
      return result;
    for (auto it = overlap_begin(start); it != Map_.end() && it->first < end;
         ++it) {
      result.push_back({it->first, it->second.first, it->second.second});
    }
    return result;
  }

  // Return the gaps (unmapped sub-ranges) within [start, end).
  std::vector<std::pair<K, K>> get_gaps(K start, K end) const {
    std::vector<std::pair<K, K>> result;
    if (start >= end)
      return result;
    K cursor = start;
    for (auto it = overlap_begin(start); it != Map_.end() && it->first < end;
         ++it) {
      if (it->first > cursor)
        result.push_back({cursor, it->first});
      if (it->second.first > cursor)
        cursor = it->second.first;
    }
    if (cursor < end)
      result.push_back({cursor, end});
    return result;
  }

  // Apply a function to every value in the map.
  void update_all(std::function<void(V &)> fn) {
    for (auto &entry : Map_)
      fn(entry.second.second);
  }

private:
  // Each entry (Start, (End, Value)) represents range [Start, End).
  std::map<K, std::pair<K, V>> Map_;

  // Return an iterator to the first entry that could overlap a range
  // starting at 'start'.
  auto overlap_begin(K start) {
    auto it = Map_.lower_bound(start);
    if (it != Map_.begin()) {
      --it;
      if (it->second.first <= start)
        ++it;
    }
    return it;
  }

  auto overlap_begin(K start) const {
    auto it = Map_.lower_bound(start);
    if (it != Map_.begin()) {
      --it;
      if (it->second.first <= start)
        ++it;
    }
    return it;
  }

  // Try to merge the entry at 'it' with its left and right neighbors.
  void coalesce(typename std::map<K, std::pair<K, V>>::iterator it) {
    // Merge with right neighbor.
    auto right = std::next(it);
    if (right != Map_.end() && it->second.first == right->first &&
        it->second.second == right->second.second) {
      it->second.first = right->second.first;
      Map_.erase(right);
    }
    // Merge with left neighbor.
    if (it != Map_.begin()) {
      auto left = std::prev(it);
      if (left->second.first == it->first &&
          left->second.second == it->second.second) {
        left->second.first = it->second.first;
        Map_.erase(it);
      }
    }
  }
};

} // namespace mmap

#endif // LIBMMAP_RANGE_MAP_H
