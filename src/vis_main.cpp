#include "core/board.h"
#include "core/attacks.h"
#include "core/movegen.h"
#include "search/search.h"
#include "protocol/server.h"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

// ══════════════════════════════════════════
//  Helpers
// ══════════════════════════════════════════
static std::string moveToStr(Move m) {
    Square f = moveFrom(m), t = moveTo(m);
    std::string s;
    s += char('a' + fileOf(f)); s += char('1' + rankOf(f));
    s += char('a' + fileOf(t)); s += char('1' + rankOf(t));
    if (moveType(m) == PROMOTION) {
        const char* pp = "nbrq";
        s += pp[movePromotion(m) - KNIGHT];
    }
    return s;
}

static Move parseMove(const Board& board, const std::string& s) {
    if (s.size() < 4) return NULL_MOVE;
    File ff = File(s[0]-'a'), tf = File(s[2]-'a');
    Rank fr = Rank(s[1]-'1'), tr = Rank(s[3]-'1');
    if (ff<0||ff>7||fr<0||fr>7||tf<0||tf>7||tr<0||tr>7) return NULL_MOVE;
    Square from = makeSquare(ff,fr), to = makeSquare(tf,tr);
    MoveList list; generateMoves(board, list);
    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        if (moveFrom(m)!=from || moveTo(m)!=to) continue;
        if (moveType(m)==PROMOTION) {
            if (s.size()<5) { if (movePromotion(m)==QUEEN) return m; }
            else {
                char c = s[4];
                PieceType p = movePromotion(m);
                if ((p==QUEEN&&c=='q')||(p==ROOK&&c=='r')||
                    (p==BISHOP&&c=='b')||(p==KNIGHT&&c=='n')) return m;
            }
        } else return m;
    }
    return NULL_MOVE;
}

// Build a JSON description of the current board state
static std::string boardToJson(const Board& board) {
    // Piece characters: White=uppercase, Black=lowercase
    const char wpc[] = "PNBRQKpnbrqk.";
    std::ostringstream oss;
    oss << "{\"type\":\"position\","
        << "\"fen\":\"" << board.toFEN() << "\","
        << "\"turn\":\"" << (board.sideToMove()==WHITE?"white":"black") << "\","
        << "\"pieces\":[";
    bool first = true;
    for (int s = 0; s < 64; ++s) {
        Piece p = board.pieceOn(Square(s));
        if (p == NO_PIECE) continue;
        if (!first) oss << ",";
        first = false;
        int r = s / 8, f = s % 8;
        oss << "{\"sq\":" << s
            << ",\"file\":" << f
            << ",\"rank\":" << r
            << ",\"piece\":\"" << wpc[p] << "\"}";
    }
    oss << "]}";
    return oss.str();
}

// ══════════════════════════════════════════
//  Game state (shared between threads)
// ══════════════════════════════════════════
static WebSocketServer  server;
static Board            board;
static Searcher*        searcher = nullptr;
static std::mutex       boardMtx;
static std::atomic<bool> engineRunning{false};
static std::atomic<bool> engineStop{false};
static bool             humanIsWhite = true;

// ══════════════════════════════════════════
//  Engine search (runs in its own thread)
//  Sends "info" lines to browser while thinking
// ══════════════════════════════════════════
static void runEngine(int depth) {
    engineRunning = true;
    engineStop    = false;

    // We override cout to capture info lines and relay to browser
    // Use a custom streambuf
    struct CaptureBuf : std::streambuf {
        std::string line;
        WebSocketServer& srv;
        explicit CaptureBuf(WebSocketServer& s) : srv(s) {}
        int overflow(int c) override {
            if (c == '\n') {
                // Forward info line to browser as JSON
                std::ostringstream oss;
                oss << "{\"type\":\"info\",\"line\":\"" << line << "\"}";
                srv.send(oss.str());
                line.clear();
            } else {
                line += (char)c;
            }
            return c;
        }
    } cap(server);

    std::streambuf* old = std::cout.rdbuf(&cap);

    SearchLimits limits;
    limits.maxDepth = depth;
    limits.movetime = 30000;

    SearchResult result;
    {
        std::lock_guard<std::mutex> lk(boardMtx);
        result = searcher->search(board, limits);
    }

    std::cout.rdbuf(old);

    if (!engineStop && result.bestMove != NULL_MOVE) {
        {
            std::lock_guard<std::mutex> lk(boardMtx);
            board.makeMove(result.bestMove);
        }

        // Tell browser the engine's move
        std::ostringstream oss;
        oss << "{\"type\":\"enginemove\","
            << "\"move\":\"" << moveToStr(result.bestMove) << "\","
            << "\"score\":" << result.score
            << "}";
        server.send(oss.str());

        // Send updated board
        std::lock_guard<std::mutex> lk(boardMtx);
        server.send(boardToJson(board));

        // Check game over
        MoveList lst; generateMoves(board, lst);
        if (lst.count == 0) {
            Square ksq = board.kingSquare(board.sideToMove());
            bool inCheck = isAttacked(board, ksq, ~board.sideToMove());
            std::string reason = inCheck ? "checkmate" : "stalemate";
            std::string winner = inCheck
                ? (board.sideToMove()==WHITE ? "black" : "white")
                : "none";
            std::ostringstream g;
            g << "{\"type\":\"gameover\","
              << "\"reason\":\"" << reason << "\","
              << "\"winner\":\"" << winner << "\"}";
            server.send(g.str());
        }
    }

    engineRunning = false;
}

// ══════════════════════════════════════════
//  Handle messages from the browser
// ══════════════════════════════════════════
static void onMessage(const std::string& msg) {
    // Simple JSON field extraction — no full parser needed
    auto getField = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = msg.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = msg.find('"', pos);
        return msg.substr(pos, end - pos);
    };
    auto getInt = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        size_t pos = msg.find(search);
        if (pos == std::string::npos) return -1;
        pos += search.size();
        return std::stoi(msg.substr(pos));
    };

    std::string type = getField("type");

    if (type == "init") {
        // Browser connected — send current position
        std::lock_guard<std::mutex> lk(boardMtx);
        server.send(boardToJson(board));
        std::ostringstream oss;
        oss << "{\"type\":\"settings\","
            << "\"humanIsWhite\":" << (humanIsWhite?"true":"false") << "}";
        server.send(oss.str());

    } else if (type == "move") {
        // Human made a move
        if (engineRunning) return;
        std::string mv = getField("move");
        Move m = parseMove(board, mv);
        if (m == NULL_MOVE) {
            server.send("{\"type\":\"error\",\"msg\":\"Illegal move\"}");
            return;
        }
        {
            std::lock_guard<std::mutex> lk(boardMtx);
            board.makeMove(m);
            server.send(boardToJson(board));
        }

        // Check game over after human move
        MoveList lst; generateMoves(board, lst);
        if (lst.count == 0) {
            Square ksq = board.kingSquare(board.sideToMove());
            bool inCheck = isAttacked(board, ksq, ~board.sideToMove());
            std::string reason = inCheck ? "checkmate" : "stalemate";
            std::string winner = inCheck
                ? (board.sideToMove()==WHITE ? "black" : "white")
                : "none";
            std::ostringstream g;
            g << "{\"type\":\"gameover\","
              << "\"reason\":\"" << reason << "\","
              << "\"winner\":\"" << winner << "\"}";
            server.send(g.str());
            return;
        }

        // Now engine plays
        int depth = getInt("depth");
        if (depth < 1 || depth > 10) depth = 5;
        std::thread(runEngine, depth).detach();

    } else if (type == "newgame") {
        engineStop = true;
        std::lock_guard<std::mutex> lk(boardMtx);
        board.setStartingPosition();
        searcher->clearTT();
        humanIsWhite = getField("side") != "black";
        server.send(boardToJson(board));
        std::ostringstream oss;
        oss << "{\"type\":\"settings\","
            << "\"humanIsWhite\":" << (humanIsWhite?"true":"false") << "}";
        server.send(oss.str());

        // If human is Black, engine goes first
        if (!humanIsWhite) {
            int depth = getInt("depth");
            if (depth < 1) depth = 5;
            std::thread(runEngine, depth).detach();
        }

    } else if (type == "getmoves") {
        // Browser asks for legal moves from a square
        std::string sqStr = getField("sq");
        if (sqStr.empty()) return;
        int sqIdx = std::stoi(sqStr);
        MoveList list; generateMoves(board, list);
        std::ostringstream oss;
        oss << "{\"type\":\"legalmoves\",\"from\":" << sqIdx << ",\"moves\":[";
        bool first = true;
        for (int i = 0; i < list.count; ++i) {
            if (moveFrom(list.moves[i]) == Square(sqIdx)) {
                if (!first) oss << ",";
                oss << moveTo(list.moves[i]);
                first = false;
            }
        }
        oss << "]}";
        server.send(oss.str());
    }
}

// ══════════════════════════════════════════
//  Entry point
// ══════════════════════════════════════════
int main(int argc, char* argv[]) {
    initAttacks();
    board.setStartingPosition();
    searcher = new Searcher(32);

    int port = 8080;
    if (argc > 1) port = std::stoi(argv[1]);

    // Determine path to web/index.html
    // Try a few common locations
    std::vector<std::string> candidates = {
        "web/index.html",
        "../web/index.html",
        "../../web/index.html",
        "./index.html"
    };
    if (argc > 2) candidates.insert(candidates.begin(), argv[2]);

    std::string htmlPath;
    for (auto& c : candidates) {
        FILE* f = fopen(c.c_str(), "rb");
        if (f) { fclose(f); htmlPath = c; break; }
    }
    if (htmlPath.empty()) {
        fprintf(stderr, "Cannot find web/index.html. Run from the knightfall/ directory.\n");
        return 1;
    }

    server.listen(port, htmlPath, onMessage);
    delete searcher;
    return 0;
}
