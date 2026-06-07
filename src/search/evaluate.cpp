#include "evaluate.h"

// ══════════════════════════════════════════
//  Piece-Square Tables
//
//  Each table gives a bonus/penalty for having
//  a piece on each square. Encourages good
//  piece placement without explicit rules.
//
//  Tables are from White's perspective (a1=0).
//  For Black we mirror vertically (rank flip).
// ══════════════════════════════════════════

// clang-format off

// Pawns: reward center advancement, penalise edge
static const int PawnTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

// Knights: reward center, penalise rim ("a knight on the rim is dim")
static const int KnightTable[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

// Bishops: reward long diagonals, avoid corners
static const int BishopTable[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

// Rooks: reward 7th rank, open files
static const int RookTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

// Queens: avoid early development to exposed squares
static const int QueenTable[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

// Kings: middlegame — hide behind pawns, stay castled
static const int KingMidTable[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

// clang-format on

// Mirror a square for Black (flip rank)
static inline Square mirror(Square s) {
    return Square(s ^ 56);  // XOR with 56 flips the rank (0<->7, 1<->6, etc.)
}

// Look up piece-square bonus for a piece on a square
static int pstBonus(PieceType pt, Color c, Square s) {
    Square idx = (c == WHITE) ? mirror(s) : s;
    // Tables are indexed from rank 8 down to rank 1 (visual board layout)
    // mirror(s) for White makes a1 map to the bottom-left of the table
    switch (pt) {
        case PAWN:   return PawnTable[idx];
        case KNIGHT: return KnightTable[idx];
        case BISHOP: return BishopTable[idx];
        case ROOK:   return RookTable[idx];
        case QUEEN:  return QueenTable[idx];
        case KING:   return KingMidTable[idx];
        default:     return 0;
    }
}

// Material value per piece type
static const Score materialValue[7] = {
    PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE,
    ROOK_VALUE, QUEEN_VALUE, 0, 0  // king has no material value
};

// ══════════════════════════════════════════
//  evaluate — main evaluation function
//
//  Returns score from White's perspective.
//  Positive = White winning, Negative = Black winning.
// ══════════════════════════════════════════
Score evaluate(const Board& board) {
    Score score = 0;

    for (int c = WHITE; c <= BLACK; ++c) {
        int sign = (c == WHITE) ? 1 : -1;

        for (int pt = PAWN; pt <= KING; ++pt) {
            Bitboard bb = board.pieces(Color(c), PieceType(pt));

            while (bb) {
                Square sq = popLsb(bb);
                score += sign * materialValue[pt];
                score += sign * pstBonus(PieceType(pt), Color(c), sq);
            }
        }
    }

    return score;
}
