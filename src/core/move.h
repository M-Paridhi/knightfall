#pragma once
#include "types.h"

// ──────────────────────────────────────────
//  Move encoding (packed into a single uint32_t)
//
//  bits  0– 5 : from square     (6 bits, values 0–63)
//  bits  6–11 : to square       (6 bits, values 0–63)
//  bits 12–13 : move type       (2 bits, 4 types)
//  bits 14–15 : promotion piece (2 bits: N/B/R/Q)
//  bits 16–31 : move score      (16 bits, used in move ordering)
// ──────────────────────────────────────────

enum MoveType : int {
    NORMAL     = 0,
    CASTLING   = 1,
    EN_PASSANT = 2,
    PROMOTION  = 3
};

inline Move encodeMove(Square from, Square to,
                       MoveType type  = NORMAL,
                       PieceType promo = KNIGHT)
{
    return (from) | (to << 6) | (type << 12) | ((promo - KNIGHT) << 14);
}

inline Square    moveFrom(Move m)       { return Square(m & 0x3F); }
inline Square    moveTo(Move m)         { return Square((m >> 6) & 0x3F); }
inline MoveType  moveType(Move m)       { return MoveType((m >> 12) & 0x3); }
inline PieceType movePromotion(Move m)  { return PieceType(((m >> 14) & 0x3) + KNIGHT); }

inline void setMoveScore(Move& m, int score) { m = (m & 0xFFFF) | (uint32_t(score) << 16); }
inline int  getMoveScore(Move m)             { return int(m >> 16); }

constexpr Move NULL_MOVE = 0;
