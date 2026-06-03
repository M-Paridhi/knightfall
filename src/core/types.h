#pragma once
#include <cstdint>

// ──────────────────────────────────────────
//  Primitive type aliases
// ──────────────────────────────────────────
using Bitboard = uint64_t;
using Square   = int;
using Move     = uint32_t;

// ──────────────────────────────────────────
//  Squares (LERF mapping: a1=0, h1=7, a8=56, h8=63)
// ──────────────────────────────────────────
enum Squares : Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE = 64
};

// ──────────────────────────────────────────
//  Pieces
// ──────────────────────────────────────────
enum PieceType : int {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    NO_PIECE_TYPE
};

enum Color : int {
    WHITE, BLACK, NO_COLOR
};

// piece = color * 6 + pieceType
// WHITE_PAWN=0 ... WHITE_KING=5, BLACK_PAWN=6 ... BLACK_KING=11
enum Piece : int {
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP,
    WHITE_ROOK, WHITE_QUEEN,  WHITE_KING,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP,
    BLACK_ROOK, BLACK_QUEEN,  BLACK_KING,
    NO_PIECE
};

// ──────────────────────────────────────────
//  Castling rights (4 bits)
// ──────────────────────────────────────────
enum CastlingRight : int {
    NO_CASTLING  = 0,
    WHITE_OO     = 1,    // White kingside
    WHITE_OOO    = 2,    // White queenside
    BLACK_OO     = 4,    // Black kingside
    BLACK_OOO    = 8,    // Black queenside
    ALL_CASTLING = 15
};

// ──────────────────────────────────────────
//  Files and Ranks
// ──────────────────────────────────────────
enum File : int { FILE_A, FILE_B, FILE_C, FILE_D,
                  FILE_E, FILE_F, FILE_G, FILE_H };
enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4,
                  RANK_5, RANK_6, RANK_7, RANK_8 };

// ──────────────────────────────────────────
//  Helper functions (constexpr = zero runtime cost)
// ──────────────────────────────────────────
constexpr Square    makeSquare(File f, Rank r)  { return r * 8 + f; }
constexpr File      fileOf(Square s)             { return File(s & 7); }
constexpr Rank      rankOf(Square s)             { return Rank(s >> 3); }
constexpr Color     colorOf(Piece p)             { return Color(p / 6); }
constexpr PieceType typeOf(Piece p)              { return PieceType(p % 6); }
constexpr Color     operator~(Color c)           { return Color(c ^ 1); }
