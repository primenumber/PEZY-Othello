#include "table.hpp"

void make_table(ull *table) {
  for (ull i = 0; i < 256; ++i) {
    ull c = i;
    for (int j = 0; j < 8; ++j) {
      c = (c & 1) ? (poly ^ (c >> 1)) : (c >> 1);
    }
    table[i] = c;
  }
}
