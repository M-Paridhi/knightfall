#include "uci.h"
#include "core/attacks.h"
#include <iostream>
#include <sstream>
#include <string>

UCIProtocol::UCIProtocol() : searcher(16) {
    initAttacks();
    board.setStartingPosition();
}

// ──────────────────────────────────────────
//  Parse a move in UCI notation
//  "e2e4" → normal move
//  "e7e8q" → promotion to queen
// ──────────────────────────────────────────
Move UCIProtocol::parseMove(const std::string& s) {
    if (s.size() < 4) return NULL_MOVE;

    File fromFile = File(s[0] - 'a');
    Rank fromRank = Rank(s[1] - '1');
    File toFile   = File(s[2] - 'a');
    Rank toRank   = Rank(s[3] - '1');

    if (fromFile < 0 || fromFile > 7) return NULL_MOVE;
    if (fromRank < 0 || fromRank > 7) return NULL_MOVE;
    if (toFile   < 0 || toFile   > 7) return NULL_MOVE;
    if (toRank   < 0 || toRank   > 7) return NULL_MOVE;

    Square from = makeSquare(fromFile, fromRank);
    Square to   = makeSquare(toFile,   toRank);

    // Generate legal moves to find the exact move type
    MoveList list;
    generateMoves(board, list);

    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        if (moveFrom(m) != from || moveTo(m) != to) continue;

        // For promotions, match the promotion piece
        if (moveType(m) == PROMOTION) {
            if (s.size() < 5) continue;
            PieceType promo = movePromotion(m);
            char c = s[4];
            if ((promo == QUEEN  && c == 'q') ||
                (promo == ROOK   && c == 'r') ||
                (promo == BISHOP && c == 'b') ||
                (promo == KNIGHT && c == 'n'))
                return m;
        } else {
            return m;
        }
    }

    return NULL_MOVE;
}

// ──────────────────────────────────────────
//  UCI command handlers
// ──────────────────────────────────────────
void UCIProtocol::handleUCI() {
    std::cout << "id name Knightfall\n";
    std::cout << "id author Paridhi Mittal\n";
    std::cout << "option name Hash type spin default 16 min 1 max 256\n";
    std::cout << "uciok\n";
    std::cout.flush();
}

void UCIProtocol::handleIsReady() {
    std::cout << "readyok\n";
    std::cout.flush();
}

void UCIProtocol::handleNewGame() {
    board.setStartingPosition();
    searcher.clearTT();
}

void UCIProtocol::handlePosition(const std::string& line) {
    std::istringstream ss(line);
    std::string token;
    ss >> token;  // "position"

    ss >> token;
    if (token == "startpos") {
        board.setStartingPosition();
        ss >> token;  // consume "moves" if present
    } else if (token == "fen") {
        std::string fen;
        while (ss >> token && token != "moves")
            fen += token + " ";
        board.setFromFEN(fen);
    }

    // Apply move list
    if (token == "moves") {
        while (ss >> token) {
            Move m = parseMove(token);
            if (m != NULL_MOVE) board.makeMove(m);
        }
    }
}

void UCIProtocol::handleGo(const std::string& line) {
    std::istringstream ss(line);
    std::string token;
    ss >> token;  // "go"

    SearchLimits limits;
    limits.maxDepth = 64;
    limits.movetime = 1000;  // default 1 second

    bool isWhite = (board.sideToMove() == WHITE);

    while (ss >> token) {
        if (token == "depth") {
            ss >> limits.maxDepth;
            limits.movetime = 60000;  // no time limit when depth is specified
        } else if (token == "movetime") {
            ss >> limits.movetime;
        } else if (token == "wtime" && isWhite) {
            int wtime; ss >> wtime;
            limits.movetime = wtime / 20;  // use ~5% of remaining time
        } else if (token == "btime" && !isWhite) {
            int btime; ss >> btime;
            limits.movetime = btime / 20;
        } else if (token == "infinite") {
            limits.maxDepth = 64;
            limits.movetime = 60000;
        }
    }

    SearchResult result = searcher.search(board, limits);

    // Output best move
    if (result.bestMove != NULL_MOVE) {
        Square f = moveFrom(result.bestMove);
        Square t = moveTo(result.bestMove);
        std::cout << "bestmove "
                  << char('a' + fileOf(f)) << char('1' + rankOf(f))
                  << char('a' + fileOf(t)) << char('1' + rankOf(t));
        if (moveType(result.bestMove) == PROMOTION) {
            const char* pp = "nbrq";
            std::cout << pp[movePromotion(result.bestMove) - KNIGHT];
        }
        std::cout << "\n";
    } else {
        std::cout << "bestmove 0000\n";
    }
    std::cout.flush();
}

// ──────────────────────────────────────────
//  Main loop
// ──────────────────────────────────────────
void UCIProtocol::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        if      (cmd == "uci")         handleUCI();
        else if (cmd == "isready")     handleIsReady();
        else if (cmd == "ucinewgame")  handleNewGame();
        else if (cmd == "position")    handlePosition(line);
        else if (cmd == "go")          handleGo(line);
        else if (cmd == "quit")        break;
        // "stop" is handled implicitly by time management
    }
}
