#pragma once
#include "types.h"
#include <cassert>
#include <string>

// ──────────────────────────────────────────
//  Core bit operations
// ──────────────────────────────────────────

// Set/clear/test individual bits
inline void   setBit(Bitboard& bb, Square s)   { bb |=  (1ULL << s); }
inline void   clearBit(Bitboard& bb, Square s) { bb &= ~(1ULL << s); }
inline bool   testBit(Bitboard  bb, Square s)  { return (bb >> s) & 1; }

// Number of set bits (population count)
// __builtin_popcountll is a GCC/Clang intrinsic → single CPU instruction
inline int popcount(Bitboard bb) {
    return __builtin_popcountll(bb);
}

// Index of the least significant set bit
// Use this to iterate over all set squares in a bitboard
inline Square lsb(Bitboard bb) {
    assert(bb != 0);
    return Square(__builtin_ctzll(bb));  // count trailing zeros
}

// Extract (and clear) the least significant bit
// The core of our "iterate over pieces" loop
inline Square popLsb(Bitboard& bb) {
    Square s = lsb(bb);
    bb &= bb - 1;   // clears the lowest set bit — classic bit trick
    return s;
}

// ──────────────────────────────────────────
//  Precomputed masks (defined in bitboard.cpp)
// ──────────────────────────────────────────
extern const Bitboard FileMask[8];   // all squares on a given file
extern const Bitboard RankMask[8];   // all squares on a given rank
extern const Bitboard SquareBB[64];  // single-square bitboards

// ──────────────────────────────────────────
//  Shift operations (moving pieces on the board)
// ──────────────────────────────────────────
//
// Shifting a bitboard = moving all pieces in a direction.
// We must mask out wrap-arounds (h-file pieces wrapping to a-file).
//
//  Direction encoding:
//  N  = +8  (north, toward rank 8)
//  S  = -8  (south, toward rank 1)
//  E  = +1  (east, toward h-file) — mask out a-file before shifting
//  W  = -1  (west, toward a-file) — mask out h-file before shifting

constexpr Bitboard NOT_A_FILE = 0xfefefefefefefefeULL;
constexpr Bitboard NOT_H_FILE = 0x7f7f7f7f7f7f7f7fULL;

inline Bitboard shiftN (Bitboard b) { return b << 8; }
inline Bitboard shiftS (Bitboard b) { return b >> 8; }
inline Bitboard shiftE (Bitboard b) { return (b & NOT_H_FILE) << 1; }
inline Bitboard shiftW (Bitboard b) { return (b & NOT_A_FILE) >> 1; }
inline Bitboard shiftNE(Bitboard b) { return (b & NOT_H_FILE) << 9; }
inline Bitboard shiftNW(Bitboard b) { return (b & NOT_A_FILE) << 7; }
inline Bitboard shiftSE(Bitboard b) { return (b & NOT_H_FILE) >> 7; }
inline Bitboard shiftSW(Bitboard b) { return (b & NOT_A_FILE) >> 9; }

// ──────────────────────────────────────────
//  Debug utility
// ──────────────────────────────────────────
std::string bitboardToString(Bitboard bb);