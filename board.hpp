#pragma once
#include "types.hpp"

int popcnt(ull player);
ull flip(ull player, ull opponent, int pos);
ull mobility(ull player, ull opponent);
int mobility_count(ull player, ull opponent);
inline int final_score(ull player, ull opponent) {
  int pcnt = popcnt(player);
  int ocnt = popcnt(opponent);
  if (pcnt == ocnt) return 0;
  if (pcnt > ocnt) return 64 - 2*ocnt;
  return 2*pcnt - 64;
}
int stones_count(ull player, ull opponent);
