#pragma once
#ifndef __PZC_KERNEL__
#include <cstdint>
#else
#define UINT64_C(x) x##ull
#endif
using ull = uint64_t;
class Board {
 public:
  uint64_t me, op;
  Board(uint64_t me, uint64_t op)
    : me(me), op(op) {}
};

