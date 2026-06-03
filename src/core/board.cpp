#include "board.h"
#include "move.h"
#include <sstream>
#include <iostream>
#include <cassert>
#include <random>
#include <cmath>

// ══════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════
Board::Board() {
    initZobrist();
    setStartingPosition();
}

// ══════════════════════════════════════════
//  Zobrist initialisation
//  Must be called before any board operations.
//  Fixed seed = same keys every run = opening book / TT stay valid.
// ══════════════════════════════════════════
void Board::initZobrist() {
    std::mt19937_64 rng(0xDEADBEEFCAFEBABEULL);

    for (int c = 0; c < 2; ++c)
        for (int pt = 0; pt < 6; ++pt)
            for (int s = 0; s < 64; ++s)
                zobristTable[c][pt][s] = rng();

    zobristBlackToMove = rng();

    for (int cr = 0; cr < 16; ++cr)
        zobristCastling[cr] = rng();

    for (int f = 0; f < 8; ++f)
        zobristEnPassant[f] = rng();
}

// ══════════════════════════════════════════
//  Three primitive operations
//  Everything else is built from these three.
// ══════════════════════════════════════════

void Board::putPiece(Piece p, Square s) {
    assert(pieceOnSquare[s] == NO_PIECE);
    Color     c  = colorOf(p);
    PieceType pt = typeOf(p);

    setBit(pieceBB[c][pt], s);
    setBit(byColor[c], s);
    setBit(byType[pt], s);
    pieceOnSquare[s] = p;
    state.zobristKey ^= zobristTable[c][pt][s];
}

void Board::removePiece(Square s) {
    Piece p = pieceOnSquare[s];
    assert(p != NO_PIECE);
    Color     c  = colorOf(p);
    PieceType pt = typeOf(p);

    clearBit(pieceBB[c][pt], s);
    clearBit(byColor[c], s);
    clearBit(byType[pt], s);
    pieceOnSquare[s] = NO_PIECE;
    state.zobristKey ^= zobristTable[c][pt][s];
}

void Board::movePiece(Square from, Square to) {
    Piece p = pieceOnSquare[from];
    assert(p != NO_PIECE);
    assert(pieceOnSquare[to] == NO_PIECE);
    Color     c  = colorOf(p);
    PieceType pt = typeOf(p);

    // XOR trick: flip both squares in one operation per bitboard
    Bitboard mask = SquareBB[from] | SquareBB[to];
    pieceBB[c][pt] ^= mask;
    byColor[c]     ^= mask;
    byType[pt]     ^= mask;

    pieceOnSquare[from] = NO_PIECE;
    pieceOnSquare[to]   = p;
    state.zobristKey ^= zobristTable[c][pt][from]
                      ^ zobristTable[c][pt][to];
}

// ══════════════════════════════════════════
//  FEN parsing
//  FEN format: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
// ══════════════════════════════════════════
void Board::setFromFEN(const std::string& fen) {
    // Clear all state
    for (int c = 0; c < 2; ++c)
        for (int pt = 0; pt < 6; ++pt)
            pieceBB[c][pt] = 0ULL;
    for (int c  = 0; c  < 2;  ++c)  byColor[c]  = 0ULL;
    for (int pt = 0; pt < 6; ++pt)  byType[pt]  = 0ULL;
    for (int s  = 0; s  < 64; ++s)  pieceOnSquare[s] = NO_PIECE;

    state = StateInfo{};
    state.enPassantSquare = NO_SQUARE;
    state.castlingRights  = NO_CASTLING;
    state.halfmoveClock   = 0;
    state.capturedPiece   = NO_PIECE;
    state.zobristKey      = 0ULL;
    historyPly = 0;

    std::istringstream ss(fen);
    std::string token;

    // ── Field 1: piece placement ───────────
    // FEN starts at rank 8 (top of board), file a (left)
    ss >> token;
    int rank = 7, file = 0;
    for (char ch : token) {
        if (ch == '/') {
            --rank; file = 0;
        } else if (ch >= '1' && ch <= '8') {
            file += (ch - '0');
        } else {
            // Map character to piece
            // Uppercase = White, Lowercase = Black
            // P=pawn N=knight B=bishop R=rook Q=queen K=king
            const std::string chars = "PNBRQKpnbrqk";
            int idx = (int)chars.find(ch);
            if (idx != (int)std::string::npos) {
                Color     c  = (idx < 6) ? WHITE : BLACK;
                PieceType pt = PieceType(idx % 6);
                Square    sq = makeSquare(File(file), Rank(rank));
                putPiece(Piece(c * 6 + pt), sq);
                ++file;
            }
        }
    }

    // ── Field 2: side to move ──────────────
    ss >> token;
    stm = (token == "w") ? WHITE : BLACK;
    if (stm == BLACK)
        state.zobristKey ^= zobristBlackToMove;

    // ── Field 3: castling rights ───────────
    ss >> token;
    for (char ch : token) {
        switch (ch) {
            case 'K': state.castlingRights |= WHITE_OO;  break;
            case 'Q': state.castlingRights |= WHITE_OOO; break;
            case 'k': state.castlingRights |= BLACK_OO;  break;
            case 'q': state.castlingRights |= BLACK_OOO; break;
        }
    }
    state.zobristKey ^= zobristCastling[state.castlingRights];

    // ── Field 4: en passant square ─────────
    ss >> token;
    if (token != "-") {
        File f = File(token[0] - 'a');
        Rank r = Rank(token[1] - '1');
        state.enPassantSquare = makeSquare(f, r);
        state.zobristKey ^= zobristEnPassant[f];
    }

    // ── Fields 5 & 6: clocks ──────────────
    if (ss >> token) state.halfmoveClock = std::stoi(token);
    if (ss >> token) fullmoveNum            = std::stoi(token);
}

void Board::setStartingPosition() {
    setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

// ══════════════════════════════════════════
//  FEN export
// ══════════════════════════════════════════
std::string Board::toFEN() const {
    std::ostringstream oss;
    const std::string pieceChars = "PNBRQKpnbrqk";

    // Field 1: piece placement
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Square s = makeSquare(File(file), Rank(rank));
            Piece  p = pieceOnSquare[s];
            if (p == NO_PIECE) {
                ++empty;
            } else {
                if (empty) { oss << empty; empty = 0; }
                oss << pieceChars[p];
            }
        }
        if (empty) oss << empty;
        if (rank > 0) oss << '/';
    }

    // Field 2: side to move
    oss << ' ' << (stm == WHITE ? 'w' : 'b');

    // Field 3: castling
    oss << ' ';
    bool anyCastle = false;
    if (state.castlingRights & WHITE_OO)  { oss << 'K'; anyCastle = true; }
    if (state.castlingRights & WHITE_OOO) { oss << 'Q'; anyCastle = true; }
    if (state.castlingRights & BLACK_OO)  { oss << 'k'; anyCastle = true; }
    if (state.castlingRights & BLACK_OOO) { oss << 'q'; anyCastle = true; }
    if (!anyCastle) oss << '-';

    // Field 4: en passant
    oss << ' ';
    if (state.enPassantSquare != NO_SQUARE) {
        oss << char('a' + fileOf(state.enPassantSquare));
        oss << char('1' + rankOf(state.enPassantSquare));
    } else {
        oss << '-';
    }

    // Fields 5 & 6: clocks
    oss << ' ' << state.halfmoveClock << ' ' << fullmoveNum;

    return oss.str();
}

// ══════════════════════════════════════════
//  makeMove — apply a move to the board
//  Called millions of times during search.
// ══════════════════════════════════════════

// Castling rights mask — indexed by square.
// Moving from/to a corner or king square clears the relevant right.
static const int castlingMask[64] = {
    ~WHITE_OOO, 15, 15, 15, ~(WHITE_OO|WHITE_OOO), 15, 15, ~WHITE_OO,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    ~BLACK_OOO, 15, 15, 15, ~(BLACK_OO|BLACK_OOO), 15, 15, ~BLACK_OO
};

void Board::makeMove(Move m) {
    // Save irreversible state so we can restore it in unmakeMove
    history[historyPly++] = state;

    Square    from     = moveFrom(m);
    Square    to       = moveTo(m);
    MoveType  type     = moveType(m);
    PieceType promoted = movePromotion(m);
    Piece     moved    = pieceOnSquare[from];
    Piece     captured = pieceOnSquare[to];

    // Remove old castling contribution from hash
    state.zobristKey ^= zobristCastling[state.castlingRights];

    // ── Handle capture ─────────────────────
    state.capturedPiece = NO_PIECE;
    if (captured != NO_PIECE) {
        state.capturedPiece = captured;
        removePiece(to);
        state.halfmoveClock = 0;
    }

    // ── Execute the move ───────────────────
    if (type == NORMAL) {
        movePiece(from, to);

    } else if (type == CASTLING) {
        movePiece(from, to);  // move king
        // Move the rook
        Square rookFrom, rookTo;
        if      (to == G1) { rookFrom = H1; rookTo = F1; }
        else if (to == C1) { rookFrom = A1; rookTo = D1; }
        else if (to == G8) { rookFrom = H8; rookTo = F8; }
        else               { rookFrom = A8; rookTo = D8; }
        movePiece(rookFrom, rookTo);

    } else if (type == EN_PASSANT) {
        movePiece(from, to);
        // Captured pawn is one rank behind the destination
        Square capturedPawn = (stm == WHITE)
            ? Square(to - 8) : Square(to + 8);
        state.capturedPiece = pieceOnSquare[capturedPawn];
        removePiece(capturedPawn);

    } else if (type == PROMOTION) {
        removePiece(from);
        if (captured != NO_PIECE) {
            // Already removed above for normal captures,
            // but promotion captures need the square clear first
        }
        Piece promotedPiece = Piece(stm * 6 + promoted);
        putPiece(promotedPiece, to);
    }

    // ── Update en passant ──────────────────
    // Remove old EP contribution from hash
    if (state.enPassantSquare != NO_SQUARE)
        state.zobristKey ^= zobristEnPassant[fileOf(state.enPassantSquare)];
    state.enPassantSquare = NO_SQUARE;

    // Set new EP square if double pawn push
    if (typeOf(moved) == PAWN && std::abs(to - from) == 16) {
        state.enPassantSquare = Square((from + to) / 2);
        state.zobristKey ^= zobristEnPassant[fileOf(state.enPassantSquare)];
    }

    // ── Update castling rights ─────────────
    state.castlingRights &= castlingMask[from] & castlingMask[to];
    state.zobristKey ^= zobristCastling[state.castlingRights];

    // ── Update halfmove clock ──────────────
    if (typeOf(moved) == PAWN && captured == NO_PIECE)
        state.halfmoveClock = 0;
    else if (captured == NO_PIECE)
        ++state.halfmoveClock;

    // ── Flip side to move ──────────────────
    stm = ~stm;
    state.zobristKey ^= zobristBlackToMove;

    if (stm == WHITE) ++fullmoveNum;
}

// ══════════════════════════════════════════
//  unmakeMove — perfectly restore board state
// ══════════════════════════════════════════
void Board::unmakeMove(Move m) {
    // Flip side to move back first
    stm = ~stm;
    if (stm == BLACK) --fullmoveNum;

    // Restore irreversible state
    state = history[--historyPly];

    Square   from = moveFrom(m);
    Square   to   = moveTo(m);
    MoveType type = moveType(m);

    if (type == NORMAL) {
        movePiece(to, from);
        if (state.capturedPiece != NO_PIECE)
            putPiece(state.capturedPiece, to);

    } else if (type == CASTLING) {
        movePiece(to, from);  // move king back
        Square rookFrom, rookTo;
        if      (to == G1) { rookFrom = H1; rookTo = F1; }
        else if (to == C1) { rookFrom = A1; rookTo = D1; }
        else if (to == G8) { rookFrom = H8; rookTo = F8; }
        else               { rookFrom = A8; rookTo = D8; }
        movePiece(rookTo, rookFrom);  // move rook back

    } else if (type == EN_PASSANT) {
        movePiece(to, from);
        Square capturedPawn = (stm == WHITE)
            ? Square(to - 8) : Square(to + 8);
        putPiece(state.capturedPiece, capturedPawn);

    } else if (type == PROMOTION) {
        removePiece(to);
        putPiece(Piece(stm * 6 + PAWN), from);
        if (state.capturedPiece != NO_PIECE)
            putPiece(state.capturedPiece, to);
    }
}

// ══════════════════════════════════════════
//  Debug print
// ══════════════════════════════════════════
void Board::print() const {
    const std::string pieceChars = "PNBRQKpnbrqk.";
    std::cout << "\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            Square s = makeSquare(File(file), Rank(rank));
            std::cout << pieceChars[pieceOnSquare[s]] << ' ';
        }
        std::cout << "\n";
    }
    std::cout << "    a b c d e f g h\n";
    std::cout << "FEN: " << toFEN() << "\n";
    std::cout << "Hash: " << std::hex << state.zobristKey << std::dec << "\n\n";
}