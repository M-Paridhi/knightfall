#include "core/board.h"
#include "core/attacks.h"
#include "core/movegen.h"
#include "search/search.h"
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>

// ── Helpers ───────────────────────────────

std::string moveToStr(Move m) {
    Square f = moveFrom(m), t = moveTo(m);
    std::string s;
    s += char('a' + fileOf(f));
    s += char('1' + rankOf(f));
    s += char('a' + fileOf(t));
    s += char('1' + rankOf(t));
    if (moveType(m) == PROMOTION) {
        const char* pp = "nbrq";
        s += pp[movePromotion(m) - KNIGHT];
    }
    return s;
}

Move parseMove(const Board& board, const std::string& s) {
    if (s.size() < 4) return NULL_MOVE;
    File fromFile = File(s[0] - 'a');
    Rank fromRank = Rank(s[1] - '1');
    File toFile   = File(s[2] - 'a');
    Rank toRank   = Rank(s[3] - '1');
    if (fromFile<0||fromFile>7||fromRank<0||fromRank>7) return NULL_MOVE;
    if (toFile  <0||toFile  >7||toRank  <0||toRank  >7) return NULL_MOVE;

    Square from = makeSquare(fromFile, fromRank);
    Square to   = makeSquare(toFile,   toRank);

    MoveList list; generateMoves(board, list);
    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        if (moveFrom(m) != from || moveTo(m) != to) continue;
        if (moveType(m) == PROMOTION) {
            if (s.size() < 5) {
                // default to queen
                if (movePromotion(m) == QUEEN) return m;
            } else {
                char c = s[4];
                PieceType promo = movePromotion(m);
                if ((promo==QUEEN &&c=='q')||(promo==ROOK  &&c=='r')||
                    (promo==BISHOP&&c=='b')||(promo==KNIGHT&&c=='n'))
                    return m;
            }
        } else {
            return m;
        }
    }
    return NULL_MOVE;
}

void printBoardFancy(const Board& board) {
    // Unicode pieces
    const char* wPieces[] = {"♙","♘","♗","♖","♕","♔"};
    const char* bPieces[] = {"♟","♞","♝","♜","♛","♚"};
    const char* empty     = "·";

    std::cout << "\n";
    for (int r = 7; r >= 0; --r) {
        std::cout << " " << (r + 1) << " │";
        for (int f = 0; f < 8; ++f) {
            Square s = makeSquare(File(f), Rank(r));
            Piece  p = board.pieceOn(s);
            std::cout << " ";
            if (p == NO_PIECE) std::cout << empty;
            else if (colorOf(p) == WHITE) std::cout << wPieces[typeOf(p)];
            else                          std::cout << bPieces[typeOf(p)];
        }
        std::cout << "\n";
    }
    std::cout << "   └─────────────────\n";
    std::cout << "     a b c d e f g h\n\n";
}

void printMoveList(const Board& board) {
    MoveList list; generateMoves(board, list);
    std::cout << "Legal moves (" << list.count << "): ";
    for (int i = 0; i < list.count; ++i) {
        std::cout << moveToStr(list.moves[i]);
        if (i < list.count - 1) std::cout << "  ";
    }
    std::cout << "\n";
}

bool isGameOver(const Board& board, std::string& reason) {
    MoveList list; generateMoves(board, list);
    if (list.count == 0) {
        Square ksq = board.kingSquare(board.sideToMove());
        if (isAttacked(board, ksq, ~board.sideToMove()))
            reason = "Checkmate!";
        else
            reason = "Stalemate!";
        return true;
    }
    if (board.halfmoveClock() >= 100) {
        reason = "Draw by 50-move rule.";
        return true;
    }
    return false;
}

int main() {
    initAttacks();

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║     KNIGHTFALL CHESS ENGINE          ║\n";
    std::cout << "║     You vs the Engine                ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    std::cout << "Commands:\n";
    std::cout << "  <move>   e.g. e2e4, g1f3, e7e8q\n";
    std::cout << "  moves    show all legal moves\n";
    std::cout << "  undo     take back your last move\n";
    std::cout << "  flip     switch sides (you play Black)\n";
    std::cout << "  new      start a new game\n";
    std::cout << "  quit     exit\n\n";

    // Choose side
    std::cout << "Play as [W]hite or [B]lack? ";
    std::string choice;
    std::getline(std::cin, choice);
    bool humanIsWhite = (choice.empty() || choice[0]=='W' || choice[0]=='w');

    // Choose engine strength
    std::cout << "Engine depth (3=easy, 5=medium, 7=hard) [5]: ";
    std::string depthStr;
    std::getline(std::cin, depthStr);
    int engineDepth = depthStr.empty() ? 5 : std::stoi(depthStr);
    engineDepth = std::max(1, std::min(10, engineDepth));

    Board board;
    board.setStartingPosition();

    Searcher searcher(16);
    SearchLimits limits;
    limits.maxDepth = engineDepth;
    limits.movetime = 30000; // plenty of time

    // Move history for undo
    std::vector<Move> moveHistory;

    while (true) {
        printBoardFancy(board);

        std::string reason;
        if (isGameOver(board, reason)) {
            std::cout << "  *** " << reason << " ***\n\n";
            // Show who won
            if (reason == "Checkmate!") {
                Color loser = board.sideToMove();
                std::cout << (loser == WHITE ? "  Black wins!\n" : "  White wins!\n");
            }
            break;
        }

        bool humanTurn = (board.sideToMove() == WHITE) == humanIsWhite;

        if (humanTurn) {
            // Check if in check
            Square ksq = board.kingSquare(board.sideToMove());
            if (isAttacked(board, ksq, ~board.sideToMove()))
                std::cout << "  *** CHECK! ***\n\n";

            std::cout << "  Your move: ";
            std::string input;
            if (!std::getline(std::cin, input)) break;

            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t"));
            input.erase(input.find_last_not_of(" \t") + 1);
            std::transform(input.begin(), input.end(), input.begin(), ::tolower);

            if (input == "quit" || input == "exit") break;

            if (input == "new") {
                board.setStartingPosition();
                searcher.clearTT();
                moveHistory.clear();
                std::cout << "\n  New game started!\n\n";
                continue;
            }

            if (input == "moves") {
                printMoveList(board);
                continue;
            }

            if (input == "undo") {
                // Undo two moves (yours + engine's)
                if (moveHistory.size() >= 2) {
                    board.unmakeMove(moveHistory.back()); moveHistory.pop_back();
                    board.unmakeMove(moveHistory.back()); moveHistory.pop_back();
                    std::cout << "  Undone!\n\n";
                } else if (moveHistory.size() == 1) {
                    board.unmakeMove(moveHistory.back()); moveHistory.pop_back();
                    std::cout << "  Undone!\n\n";
                } else {
                    std::cout << "  Nothing to undo.\n\n";
                }
                continue;
            }

            if (input == "flip") {
                humanIsWhite = !humanIsWhite;
                std::cout << "  You are now playing as "
                          << (humanIsWhite ? "White" : "Black") << ".\n\n";
                continue;
            }

            Move m = parseMove(board, input);
            if (m == NULL_MOVE) {
                std::cout << "  Invalid move. Try 'moves' to see legal moves.\n\n";
                continue;
            }

            board.makeMove(m);
            moveHistory.push_back(m);
            std::cout << "  You played: " << moveToStr(m) << "\n\n";

        } else {
            // Engine's turn
            std::cout << "  Engine is thinking";
            std::cout.flush();

            // Suppress the info lines from the searcher
            // (redirect them into a string we can parse for the PV)
            std::streambuf* oldBuf = std::cout.rdbuf();
            std::ostringstream infoCapture;
            std::cout.rdbuf(infoCapture.rdbuf());

            SearchResult result = searcher.search(board, limits);

            std::cout.rdbuf(oldBuf);

            // Show final depth info
            std::string info = infoCapture.str();
            // Extract last depth line
            std::istringstream infoStream(info);
            std::string line, lastLine;
            while (std::getline(infoStream, line))
                if (!line.empty()) lastLine = line;

            std::cout << "\r";  // clear "thinking..."
            if (!lastLine.empty()) std::cout << "  " << lastLine << "\n";

            if (result.bestMove == NULL_MOVE) {
                std::cout << "  Engine has no move!\n";
                break;
            }

            board.makeMove(result.bestMove);
            moveHistory.push_back(result.bestMove);
            std::cout << "  Engine played: " << moveToStr(result.bestMove) << "\n\n";
        }
    }

    std::cout << "\nThanks for playing!\n";
    return 0;
}
