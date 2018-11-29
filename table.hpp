#pragma once
#include "types.hpp"

constexpr ull poly = 0x42F0E1EBA9EA3693;

void make_table(ull *table);

template <typename T>
struct Range {
  T upper;
  T lower;
  Range()
    : upper(64), lower(-64) {}
  Range(const T& upper, const T& lower)
    : upper(upper), lower(lower) {}
};

template <typename T>
struct Entry {
  ull me;
  ull op;
  Range<T> range;
  bool enable;
  Entry()
    : me(0), op(0), range(), enable(false) {}
  Entry(ull me, ull op, Range<T> range)
    : me(me), op(op), range(range), enable(true) {}
};

ull calc_hash(ull me, ull op, ull *table);

template <typename T>
const T& min(const T &lhs, const T &rhs) {
  return (lhs > rhs) ? rhs : lhs;
}

template <typename T>
const T& max(const T &lhs, const T &rhs) {
  return (lhs < rhs) ? rhs : lhs;
}

template <typename T>
class Table {
 public:
  Table(Entry<T> *entries, int size, ull *crc_table)
    : entries(entries), size(size), crc_table(crc_table)
  {
    for (int i = 0; i < size; ++i) {
      entries[i] = Entry<T>();
    }
  }
  Entry<T> get(ull me, ull op) const {
    Entry<T> e = get(calc_hash(me, op, crc_table));
    if (e.me != me || e.op != op) {
      e.enable = false;
    }
    return e;
  }
  void update(ull me, ull op, int upper, int lower, int value) {
    if (lower >= upper) return;
    ull hash = calc_hash(me, op, crc_table);
    Entry<T> e = get(hash);
    if (e.me != me || e.op != op) {
      e.enable = false;
    }
    if (!e.enable) {
      if (value <= lower) {
        entries[hash % size] = Entry<T>(me, op, Range<T>(value, -64));
      } else if (value >= upper) {
        entries[hash % size] = Entry<T>(me, op, Range<T>(64, value));
      } else {
        entries[hash % size] = Entry<T>(me, op, Range<T>(value, value));
      }
    } else {
      int hi, lo;
      if (value <= lower) {
        hi = value;
        lo = -64;
      } else if (value >= upper) {
        hi = 64;
        lo = value;
      } else {
        hi = value;
        lo = value;
      }
      entries[hash % size] = Entry<T>(me, op,
          Range<T>(min(hi, e.range.upper), max(lo, e.range.lower)));
    }
  }
 private:
  Entry<T> *entries;
  int size;
  ull *crc_table;
  Entry<T> get(ull hash) const {
    ull index = hash % size;
    return entries[index];
  }
};
