#pragma once
#include "core/board.h"

// Score is always from White's perspective.
// Positive = White is winning. Negative = Black is winning.
// Units: centipawns (100 = 1 pawn).
using Score = int;

constexpr Score SCORE_INFINITE  =  1000000;
constexpr Score SCORE_MATE      =   900000;   // base mate score
constexpr Score SCORE_DRAW      =        0;
constexpr Score SCORE_NONE      = -SCORE_INFINITE - 1;

// Material values in centipawns
constexpr Score PAWN_VALUE   = 100;
constexpr Score KNIGHT_VALUE = 320;
constexpr Score BISHOP_VALUE = 330;
constexpr Score ROOK_VALUE   = 500;
constexpr Score QUEEN_VALUE  = 900;

// Main evaluation function.
// Returns score from White's perspective regardless of side to move.
Score evaluate(const Board& board);
