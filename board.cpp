#include "board.hpp"

int stones_count(ull player, ull opponent) {
  return __builtin_popcountll(player | opponent);
}
