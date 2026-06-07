#pragma once
#include "board.h"
#include "move.h"
#include "attacks.h"

// ──────────────────────────────────────────
//  MoveList — a fixed-size array of moves
//  Max legal moves in any chess position = 218
//  We use 256 for safety margin
// ──────────────────────────────────────────
struct MoveList {
    Move  moves[256];
    int   count = 0;

    void add(Move m)            { moves[count++] = m; }
    Move* begin()               { return moves; }
    Move* end()                 { return moves + count; }
    const Move* begin() const   { return moves; }
    const Move* end()   const   { return moves + count; }
};

// ──────────────────────────────────────────
//  Move generation
//
//  generateMoves() fills a MoveList with all
//  LEGAL moves in the given position.
//
//  Legal = does not leave own king in check.
// ──────────────────────────────────────────
void generateMoves(const Board& board, MoveList& list);

// Generate only captures (used in quiescence search, Phase 2)
void generateCaptures(const Board& board, MoveList& list);
