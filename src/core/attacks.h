#pragma once
#include "types.h"
#include "bitboard.h"

// ── Attack tables ──────────────────────────────────────────────────────────
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard PawnAttacks[2][64];

// ── Init (call once at startup) ────────────────────────────────────────────
void initAttacks();

// ── Inline accessors ───────────────────────────────────────────────────────
inline Bitboard knightAttacks(Square s)            { return KnightAttacks[s]; }
inline Bitboard kingAttacks(Square s)              { return KingAttacks[s]; }
inline Bitboard pawnAttacks(Color c, Square s)     { return PawnAttacks[c][s]; }

// Sliding attacks (classical ray-based, defined in attacks.cpp)
Bitboard rookAttacks  (Square sq, Bitboard occ);
Bitboard bishopAttacks(Square sq, Bitboard occ);
Bitboard queenAttacks (Square sq, Bitboard occ);

// Square attack detection
class Board;
bool isAttacked(const Board& board, Square s, Color byColor);
