#pragma once
#include "types.h"
#include "bitboard.h"
#include "move.h"
#include <string>

// ──────────────────────────────────────────
//  Irreversible state — saved before each move
//  so we can perfectly undo (unmake) it
// ──────────────────────────────────────────
struct StateInfo {
    int       castlingRights;
    Square    enPassantSquare;
    int       halfmoveClock;
    Piece     capturedPiece;
    uint64_t  zobristKey;
};

// ──────────────────────────────────────────
//  Board — the central data structure
// ──────────────────────────────────────────
class Board {
public:
    // ── Construction ──────────────────────
    Board();
    void setStartingPosition();
    void setFromFEN(const std::string& fen);
    std::string toFEN() const;

    // ── Piece bitboard accessors ───────────
    Bitboard pieces(Color c, PieceType pt) const { return pieceBB[c][pt]; }
    Bitboard pieces(PieceType pt)          const { return byType[pt]; }
    Bitboard pieces(Color c)               const { return byColor[c]; }
    Bitboard occupied()                    const { return byColor[WHITE] | byColor[BLACK]; }
    Bitboard empty()                       const { return ~occupied(); }

    // ── Square queries ─────────────────────
    Piece  pieceOn(Square s)    const { return pieceOnSquare[s]; }
    Square kingSquare(Color c)  const { return lsb(pieceBB[c][KING]); }

    // ── Game state accessors ───────────────
    Color  sideToMove()     const { return stm; }
    int    castling()       const { return state.castlingRights; }
    bool   canCastle(CastlingRight cr) const { return state.castlingRights & cr; }
    Square enPassant()      const { return state.enPassantSquare; }
    int    halfmoveClock()  const { return state.halfmoveClock; }
    int    fullmoveNumber() const { return fullmoveNum; }
    uint64_t hash()         const { return state.zobristKey; }

    // ── Move execution ─────────────────────
    void makeMove(Move m);
    void unmakeMove(Move m);

    // ── Debug ──────────────────────────────
    void print() const;

private:
    // ── Board representation ───────────────
    Bitboard pieceBB[2][6];      // [color][pieceType]
    Bitboard byColor[2];         // all pieces per color
    Bitboard byType[6];          // all pieces per type (both colors)
    Piece    pieceOnSquare[64];  // square → piece (O(1) lookup)

    // ── Game state ─────────────────────────
    Color stm;
    int   fullmoveNum;

    StateInfo state;

    // ── History stack for unmake ───────────
    static constexpr int MAX_HISTORY = 1024;
    StateInfo history[MAX_HISTORY];
    int       historyPly;

    // ── Zobrist tables ─────────────────────
    uint64_t zobristTable[2][6][64];
    uint64_t zobristBlackToMove;
    uint64_t zobristCastling[16];
    uint64_t zobristEnPassant[8];

    // ── Internal helpers ───────────────────
    void initZobrist();
    void putPiece(Piece p, Square s);
    void removePiece(Square s);
    void movePiece(Square from, Square to);
};