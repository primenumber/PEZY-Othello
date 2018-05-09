#pragma once
#include "types.hpp"
#include "board.hpp"
//#include "table.cuh"

struct AlphaBetaProblem {
  AlphaBetaProblem(ull me, ull op, int alpha, int beta)
    : me(me), op(op), alpha(alpha), beta(beta) {}
  AlphaBetaProblem(ull me, ull op)
    : me(me), op(op), alpha(-64), beta(64) {}
  ull me;
  ull op;
  int alpha;
  int beta;
};

class MobilityGenerator {
 public:
  MobilityGenerator() {}
  MobilityGenerator(ull player, ull opponent)
    : x(~opponent), y(~player) {}
  MobilityGenerator(const MobilityGenerator &) = default;
  MobilityGenerator& operator=(const MobilityGenerator &) = default;
  ull player_pos() const {
    return x & ~y;
  }
  ull opponent_pos() const {
    return ~x & y;
  }
  ull empty_pos() const {
    return ~(x ^ y);
  }
  ull next_bit() {
    ull p = not_checked_yet();
    ull bit = p & -p;
    reset(bit);
    return bit;
  }
  bool completed() const {
    return not_checked_yet() == 0;
  }
  MobilityGenerator move(ull flip, ull bit) const {
    ull p = player_pos();
    ull o = opponent_pos();
    return MobilityGenerator(o ^ flip, (p ^ flip) | bit);
  }
  MobilityGenerator pass() const {
    ull p = player_pos();
    ull o = opponent_pos();
    return MobilityGenerator(o , p);
  }
  int score() const {
    return final_score(player_pos(), opponent_pos());
  }
 private:
  ull not_checked_yet() const {
    return x & y;
  }
  void reset(ull bit) {
    x ^= bit;
    y ^= bit;
  }
  ull x, y;
};

struct Node {
  MobilityGenerator mg;
  char alpha;
  char beta;
  bool not_pass;
  bool passed_prev;
  Node() {}
  Node(const MobilityGenerator &mg, int alpha, int beta, bool passed_prev = false)
    : mg(mg), alpha(alpha), beta(beta), not_pass(false), passed_prev(passed_prev) {}
  Node(const MobilityGenerator &mg)
    : Node(mg, -64, 64) {}
  Node(const Node &) = default;
  Node& operator=(const Node &) = default;
};

inline void swap(char &a, char &b) {
  char tmp = a;
  a = b;
  b = tmp;
}

inline void sort_by_key(char *first1, char *last1, char *first2) {
  int n = last1 - first1;
  for (int i = 1; i < n; ++i) {
    int j = i;
    while(first1[j] < first1[j-1]) {
      swap(first1[j], first1[j-1]);
      swap(first2[j], first2[j-1]);
      --j;
      if (j <= 0) break;
    }
  }
}

class UpperNode {
 public:
  static constexpr int max_mobility_count = 46;
  UpperNode(ull me, ull op, char alpha, char beta, bool pass = false)
      : alpha(alpha), beta(beta), me(me), op(op), possize(0), index(0), prev_passed(pass) {
    MobilityGenerator mg(me, op);
    while(!mg.completed()) {
      ull next_bit = mg.next_bit();
      int pos = popcnt(next_bit - 1);
      ull flip_bits = flip(me, op, pos);
      if (flip_bits) {
        cntary[possize] = mobility_count(op ^ flip_bits, (me ^ flip_bits) | next_bit);
        posary[possize++] = pos;
      }
    }
    sort_by_key(cntary, cntary + possize, posary);
  }
  UpperNode(const UpperNode &) = default;
  UpperNode(UpperNode &&) = default;
  UpperNode& operator=(const UpperNode &) = default;
  UpperNode& operator=(UpperNode &&) = default;
  bool completed() const {
    return index == possize;
  }
  int pop() {
    return posary[index++];
  }
  int size() const {
    return possize;
  }
  ull me_pos() const {
    return me;
  }
  ull op_pos() const {
    return op;
  }
  bool passed() const {
    return prev_passed;
  }
  int score() const {
    return final_score(me, op);
  }
  UpperNode move(ull bits, ull pos_bit/*, Table table*/) const {
    ull next_player = op ^ bits;
    ull next_opponent = (me ^ bits) | pos_bit;
    //Entry entry = table.find(next_player, next_opponent);
    //if (entry.enable) {
    //  char next_alpha = max(-beta, entry.lower);
    //  char next_beta = min(-alpha, entry.upper);
    //  return UpperNode(next_player, next_opponent, next_alpha, next_beta);
    //} else {
      return UpperNode(next_player, next_opponent, -beta, -alpha);
    //}
  }
  UpperNode pass(/*Table table*/) const {
    //Entry entry = table.find(op, me);
    //if (entry.enable) {
    //  char next_alpha = max(-beta, entry.lower);
    //  char next_beta = min(-alpha, entry.upper);
    //  return UpperNode(op, me, next_alpha, next_beta, true);
    //} else {
      return UpperNode(op, me, -beta, -alpha, true);
    //}
  }
  char alpha;
  char beta;
 private:
  ull me,op;
  char posary[max_mobility_count];
  char cntary[max_mobility_count];
  unsigned char possize;
  unsigned char index;
  bool prev_passed;
};

struct Params {
  size_t count;
  size_t upper_stack_size;
  size_t lower_stack_size;
};

//class UpperNode;

//struct BatchedTask {
//  cudaStream_t *str;
//  AlphaBetaProblem *abp;
//  UpperNode *upper_stacks;
//  Table table;
//  int *result;
//  size_t max_depth;
//  size_t size;
//  size_t grid_size;
//  ull *total;
//};

//void init_batch(BatchedTask &bt, size_t batch_size, size_t max_depth, const Table &table);
//void launch_batch(const BatchedTask &bt);
//bool is_ready_batch(const BatchedTask &bt);
//void destroy_batch(const BatchedTask &bt);
