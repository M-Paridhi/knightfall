#pragma once
#include "types.h"
#include <cassert>
#include <string>

// ──────────────────────────────────────────
//  Core bit operations
// ──────────────────────────────────────────

inline void setBit  (Bitboard& bb, Square s) { bb |=  (1ULL << s); }
inline void clearBit(Bitboard& bb, Square s) { bb &= ~(1ULL << s); }
inline bool testBit (Bitboard  bb, Square s) { return (bb >> s) & 1; }

// Population count — number of set bits (single CPU instruction)
inline int popcount(Bitboard bb) {
    return __builtin_popcountll(bb);
}

// Index of lowest set bit
inline Square lsb(Bitboard bb) {
    assert(bb != 0);
    return Square(__builtin_ctzll(bb));
}

// Extract lowest set bit index AND clear it from bb
// Use this to loop over all squares in a bitboard
inline Square popLsb(Bitboard& bb) {
    Square s = lsb(bb);
    bb &= bb - 1;   // clears the lowest set bit (classic bit trick)
    return s;
}

// ──────────────────────────────────────────
//  Precomputed masks (defined in bitboard.cpp)
// ──────────────────────────────────────────
extern const Bitboard SquareBB[64];
extern const Bitboard FileMask[8];
extern const Bitboard RankMask[8];

// ──────────────────────────────────────────
//  Wrap-around masks for directional shifts
// ──────────────────────────────────────────
constexpr Bitboard NOT_A_FILE = 0xfefefefefefefefeULL;
constexpr Bitboard NOT_H_FILE = 0x7f7f7f7f7f7f7f7fULL;

// ──────────────────────────────────────────
//  Directional shifts
//  N=+8, S=-8, E=+1 (mask H-file), W=-1 (mask A-file)
// ──────────────────────────────────────────
inline Bitboard shiftN (Bitboard b) { return b << 8; }
inline Bitboard shiftS (Bitboard b) { return b >> 8; }
inline Bitboard shiftE (Bitboard b) { return (b & NOT_H_FILE) << 1; }
inline Bitboard shiftW (Bitboard b) { return (b & NOT_A_FILE) >> 1; }
inline Bitboard shiftNE(Bitboard b) { return (b & NOT_H_FILE) << 9; }
inline Bitboard shiftNW(Bitboard b) { return (b & NOT_A_FILE) << 7; }
inline Bitboard shiftSE(Bitboard b) { return (b & NOT_H_FILE) >> 7; }
inline Bitboard shiftSW(Bitboard b) { return (b & NOT_A_FILE) >> 9; }

// ──────────────────────────────────────────
//  Debug: print bitboard as 8x8 grid
// ──────────────────────────────────────────
std::string bitboardToString(Bitboard bb);
