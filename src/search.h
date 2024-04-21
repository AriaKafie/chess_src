
#ifndef AI_H
#define AI_H

#include "position.h"
#include "evaluation.h"
#include "transpositiontable.h"

#include "bench.h"
#include "movegen.h"
#include "moveordering.h"
#include "ui.h"

#include <cstdint>
#include <iostream>
#include <string>

constexpr int matescore = 100000;

inline constexpr int reduction[90] = {
  0,1,1,1,1,2,2,2,2,2,
  2,2,2,2,2,3,3,3,3,3,
  3,3,3,3,3,3,3,3,3,3,
  3,3,3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,
};

namespace Search {

Move probe_white(uint64_t thinktime);
Move probe_black(uint64_t thinktime);

inline bool is_matescore(int score) {
  return score > 90000 || score < -90000;
}

inline bool is_alpha_matescore(int score) {
  return score >  90000;
}
inline bool is_beta_matescore(int score) {
  return score < -90000;
}

inline constexpr int depth_reduction[90] = {
  0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,
};

template<bool maximizing>
int quiescence_search(int alpha, int beta) {

  Bench::qnodes++;

  if constexpr (maximizing) {
    int eval = static_eval();
    if (eval >= beta)
      return beta;
    alpha = std::max(alpha, eval);
    int best_eval = -INFINITE;
    CaptureList<WHITE> captures;
    if (captures.size() == 0)
      return eval;
    captures.sort();
    for (int i = 0; i < captures.size(); i++) {
      Piece captured = piece_on(to_sq(captures[i]));
      do_capture<WHITE>(captures[i]);
      eval = quiescence_search<false>(alpha, beta);
      undo_capture<WHITE>(captures[i], captured);
      if (eval >= beta)
        return eval;
      best_eval = std::max(eval, best_eval);
      alpha = std::max(alpha, eval);
    }
    return best_eval;
  }
  else {
    int eval = static_eval();
    if (eval <= alpha)
      return alpha;
    beta = std::min(beta, eval);
    int best_eval = INFINITE;
    CaptureList<BLACK> captures;
    if (captures.size() == 0)
      return eval;
    captures.sort();
    for (int i = 0; i < captures.size(); i++) {
      Piece captured = piece_on(to_sq(captures[i]));
      do_capture<BLACK>(captures[i]);
      eval = quiescence_search<true>(alpha, beta);
      undo_capture<BLACK>(captures[i], captured);
      if (eval <= alpha)
        return eval;
      best_eval = std::min(eval, best_eval);
      beta = std::min(beta, eval);
    }
    return best_eval;
  }

}

template<bool maximizing>
int search(int alpha, int beta, int depth, int ply_from_root) {

  Bench::nodes++;

  if (depth <= 0)
    return quiescence_search<maximizing>(alpha, beta);

  int trylookup = TranspositionTable::lookup(depth, alpha, beta, ply_from_root);
  if (trylookup != FAIL)
    return trylookup;

  if constexpr (maximizing) {

    HashFlag flag = UPPER_BOUND;
    int best_eval = -INFINITE;
    Move best_move_yet = NULLMOVE;
    MoveList<WHITE> moves;
    if (moves.size() == 0)
      return moves.incheck() ? (-matescore + ply_from_root) : 0;
    int extension = moves.incheck() ? 1 : 0;
    moves.sort(TranspositionTable::lookup_move(), ply_from_root);

    for (int i = 0; i < moves.size(); i++) {

      do_move<WHITE>(moves[i]);
      int eval = search<false>(alpha, beta, depth - 1 - depth_reduction[i] + extension, ply_from_root + 1);
      if (eval > alpha && depth_reduction[i] && (depth - 1 + extension > 0))
        eval = search<false>(alpha, beta, depth - 1 + extension, ply_from_root + 1);
      undo_move<WHITE>(moves[i]);

      if (eval >= beta) {
        TranspositionTable::record(depth, LOWER_BOUND, eval, moves[i], ply_from_root);
        if (!piece_on(to_sq(moves[i])))
          killer_moves[ply_from_root].add(moves[i] & 0xffff);
        return eval;
      }

      best_eval = std::max(eval, best_eval);
      if (eval > alpha) {
        best_move_yet = moves[i];
        alpha = eval;
        flag = EXACT;
      }

    }
    TranspositionTable::record(depth, flag, best_eval, best_move_yet, ply_from_root);
    return alpha; 

  } else {

    HashFlag flag = LOWER_BOUND;
    int best_eval = INFINITE;
    Move best_move_yet = NULLMOVE;
    MoveList<BLACK> moves;
    if (moves.size() == 0)
      return moves.incheck() ? (matescore - ply_from_root) : 0;
    int extension = moves.incheck() ? 1 : 0;
    moves.sort(TranspositionTable::lookup_move(), ply_from_root);

    for (int i = 0; i < moves.size(); i++) {

      do_move<BLACK>(moves[i]);
      int eval = search<true>(alpha, beta, depth - 1 - depth_reduction[i] + extension, ply_from_root + 1);
      if (eval < beta && depth_reduction[i] && (depth - 1 + extension > 0))
        eval = search<true>(alpha, beta, depth - 1 + extension, ply_from_root + 1);
      undo_move<BLACK>(moves[i]);

      if (eval <= alpha) {
        TranspositionTable::record(depth, UPPER_BOUND, eval, moves[i], ply_from_root);
        if (!piece_on(to_sq(moves[i])))
          killer_moves[ply_from_root].add(moves[i] & 0xffff);
        return eval;
      }

      best_eval = std::min(eval, best_eval);
      if (eval < beta) {
        best_move_yet = moves[i];
        beta = eval;
        flag = EXACT;
      }

    }
    TranspositionTable::record(depth, flag, best_eval, best_move_yet, ply_from_root);
    return beta;
  }
}

} // namespace Search

#endif
