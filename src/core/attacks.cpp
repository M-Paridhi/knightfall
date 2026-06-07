#include "attacks.h"
#include "board.h"

// ══════════════════════════════════════════
//  Attack tables storage
// ══════════════════════════════════════════
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard PawnAttacks[2][64];

// Classical ray-based sliding attacks
// These are precomputed for each direction from each square
static Bitboard RayAttacks[8][64];  // 8 directions x 64 squares

enum Direction { NORTH=0, EAST, SOUTH, WEST, NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST };

// ══════════════════════════════════════════
//  Classical sliding attack generation
//  Uses o^(o-2r) trick for horizontal/vertical
//  and reverse for negative rays
// ══════════════════════════════════════════

// Positive ray (N, NE, E, NW): o-2r trick
static inline Bitboard positiveRay(Direction dir, Square sq, Bitboard occ) {
    Bitboard ray = RayAttacks[dir][sq];
    Bitboard blockers = ray & occ;
    if (blockers) {
        Square blocker = lsb(blockers);  // nearest blocker
        ray ^= RayAttacks[dir][blocker]; // exclude squares beyond blocker
    }
    return ray;
}

// Negative ray (S, SW, W, SE): uses reverse bit trick
static inline Bitboard negativeRay(Direction dir, Square sq, Bitboard occ) {
    Bitboard ray = RayAttacks[dir][sq];
    Bitboard blockers = ray & occ;
    if (blockers) {
        // msb = most significant bit = nearest blocker in negative direction
        int blocker = 63 - __builtin_clzll(blockers);  // MSB
        ray ^= RayAttacks[dir][blocker];
    }
    return ray;
}

Bitboard rookAttacks(Square sq, Bitboard occ) {
    return positiveRay(NORTH, sq, occ)
         | positiveRay(EAST,  sq, occ)
         | negativeRay(SOUTH, sq, occ)
         | negativeRay(WEST,  sq, occ);
}

Bitboard bishopAttacks(Square sq, Bitboard occ) {
    return positiveRay(NORTH_EAST, sq, occ)
         | positiveRay(NORTH_WEST, sq, occ)
         | negativeRay(SOUTH_EAST, sq, occ)
         | negativeRay(SOUTH_WEST, sq, occ);
}

Bitboard queenAttacks(Square sq, Bitboard occ) {
    return rookAttacks(sq, occ) | bishopAttacks(sq, occ);
}

// ══════════════════════════════════════════
//  initAttacks
// ══════════════════════════════════════════
void initAttacks() {
    // ── Pawn attacks ──────────────────────
    for (Square s = 0; s < 64; ++s) {
        PawnAttacks[WHITE][s] = shiftNE(SquareBB[s]) | shiftNW(SquareBB[s]);
        PawnAttacks[BLACK][s] = shiftSE(SquareBB[s]) | shiftSW(SquareBB[s]);
    }

    // ── Knight attacks ────────────────────
    const int knightMoves[8][2] = {
        {2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}
    };
    for (Square s = 0; s < 64; ++s) {
        KnightAttacks[s] = 0;
        int r = rankOf(s), f = fileOf(s);
        for (auto& km : knightMoves) {
            int nr = r + km[0], nf = f + km[1];
            if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7)
                KnightAttacks[s] |= SquareBB[makeSquare(File(nf), Rank(nr))];
        }
    }

    // ── King attacks ──────────────────────
    for (Square s = 0; s < 64; ++s) {
        Bitboard b = SquareBB[s];
        KingAttacks[s] = shiftN(b)|shiftS(b)|shiftE(b)|shiftW(b)
                        |shiftNE(b)|shiftNW(b)|shiftSE(b)|shiftSW(b);
    }

    // ── Precompute rays ───────────────────
    // For each square and direction, compute all squares along that ray
    for (Square s = 0; s < 64; ++s) {
        int r = rankOf(s), f = fileOf(s);
        RayAttacks[NORTH][s]      = 0;
        RayAttacks[SOUTH][s]      = 0;
        RayAttacks[EAST][s]       = 0;
        RayAttacks[WEST][s]       = 0;
        RayAttacks[NORTH_EAST][s] = 0;
        RayAttacks[NORTH_WEST][s] = 0;
        RayAttacks[SOUTH_EAST][s] = 0;
        RayAttacks[SOUTH_WEST][s] = 0;

        for (int i = r+1; i <= 7; ++i)
            RayAttacks[NORTH][s] |= SquareBB[makeSquare(File(f), Rank(i))];
        for (int i = r-1; i >= 0; --i)
            RayAttacks[SOUTH][s] |= SquareBB[makeSquare(File(f), Rank(i))];
        for (int i = f+1; i <= 7; ++i)
            RayAttacks[EAST][s]  |= SquareBB[makeSquare(File(i), Rank(r))];
        for (int i = f-1; i >= 0; --i)
            RayAttacks[WEST][s]  |= SquareBB[makeSquare(File(i), Rank(r))];
        for (int dr = 1; r+dr <= 7 && f+dr <= 7; ++dr)
            RayAttacks[NORTH_EAST][s] |= SquareBB[makeSquare(File(f+dr), Rank(r+dr))];
        for (int dr = 1; r+dr <= 7 && f-dr >= 0; ++dr)
            RayAttacks[NORTH_WEST][s] |= SquareBB[makeSquare(File(f-dr), Rank(r+dr))];
        for (int dr = 1; r-dr >= 0 && f+dr <= 7; ++dr)
            RayAttacks[SOUTH_EAST][s] |= SquareBB[makeSquare(File(f+dr), Rank(r-dr))];
        for (int dr = 1; r-dr >= 0 && f-dr >= 0; ++dr)
            RayAttacks[SOUTH_WEST][s] |= SquareBB[makeSquare(File(f-dr), Rank(r-dr))];
    }
}

// ══════════════════════════════════════════
//  isAttacked
// ══════════════════════════════════════════
bool isAttacked(const Board& board, Square s, Color by) {
    if (s == NO_SQUARE) return false;
    Bitboard occ = board.occupied();
    if (knightAttacks(s)      & board.pieces(by, KNIGHT)) return true;
    if (kingAttacks(s)        & board.pieces(by, KING))   return true;
    if (pawnAttacks(~by, s)   & board.pieces(by, PAWN))   return true;
    if (bishopAttacks(s, occ) & (board.pieces(by,BISHOP)|board.pieces(by,QUEEN))) return true;
    if (rookAttacks(s, occ)   & (board.pieces(by,ROOK)  |board.pieces(by,QUEEN))) return true;
    return false;
}
