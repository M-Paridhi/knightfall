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
#include <cstdio>

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
                char c = s[4]; PieceType p = movePromotion(m);
                if ((p==QUEEN&&c=='q')||(p==ROOK&&c=='r')||
                    (p==BISHOP&&c=='b')||(p==KNIGHT&&c=='n')) return m;
            }
        } else return m;
    }
    return NULL_MOVE;
}

static std::string boardToJson(const Board& board) {
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
        oss << "{\"sq\":" << s
            << ",\"file\":" << (s%8)
            << ",\"rank\":" << (s/8)
            << ",\"piece\":\"" << wpc[p] << "\"}";
    }
    oss << "]}";
    return oss.str();
}

static bool sendGameOver(const Board& board, WebSocketServer& server) {
    MoveList lst; generateMoves(board, lst);
    if (lst.count > 0) return false;
    Square ksq = board.kingSquare(board.sideToMove());
    bool inCheck = isAttacked(board, ksq, ~board.sideToMove());
    std::string reason = inCheck ? "checkmate" : "stalemate";
    std::string winner = inCheck
        ? (board.sideToMove()==WHITE ? "black" : "white") : "none";
    std::ostringstream g;
    g << "{\"type\":\"gameover\",\"reason\":\"" << reason
      << "\",\"winner\":\"" << winner << "\"}";
    server.send(g.str());
    return true;
}

// ══════════════════════════════════════════
//  Global state
// ══════════════════════════════════════════
static WebSocketServer     server;
static Board               board;
static Searcher*           searcher     = nullptr;
static bool                humanIsWhite = true;
static int                 engineDepth  = 5;
static std::mutex          boardMtx;
static std::atomic<bool>   engineStop{false};

// Each half-move: the UCI string + whose color played it ('w' or 'b')
struct HalfMove { std::string uci; char color; };  // color: 'w' or 'b'
static std::vector<HalfMove> gameLog;   // full game history, all half-moves in order

// Move stack for undo
struct MovePair { Move human; Move engine; };
static std::vector<MovePair> moveStack;

// Send the full game log as a history message
static void sendHistory() {
    std::ostringstream oss;
    oss << "{\"type\":\"history\",\"humanIsWhite\":"
        << (humanIsWhite ? "true" : "false")
        << ",\"moves\":[";
    for (size_t i = 0; i < gameLog.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"uci\":\"" << gameLog[i].uci
            << "\",\"color\":\"" << gameLog[i].color << "\"}";
    }
    oss << "]}";
    server.send(oss.str());
}

// ══════════════════════════════════════════
//  Info line relay
// ══════════════════════════════════════════
struct InfoRelay : std::streambuf {
    std::string line;
    void flush_line() {
        if (line.empty()) return;
        std::ostringstream oss;
        oss << "{\"type\":\"info\",\"line\":\"" << line << "\"}";
        server.send(oss.str());
        line.clear();
    }
    int overflow(int c) override {
        if (c == '\n') flush_line();
        else           line += (char)c;
        return c;
    }
};

// ══════════════════════════════════════════
//  Engine thread
// ══════════════════════════════════════════
static void runEngine(int depth) {
    engineStop = false;
    server.send("{\"type\":\"thinking\",\"on\":true}");

    InfoRelay relay;
    std::streambuf* old = std::cout.rdbuf(&relay);

    SearchLimits limits;
    limits.maxDepth = depth;
    limits.movetime = 30000;

    SearchResult result;
    {
        std::lock_guard<std::mutex> lk(boardMtx);
        result = searcher->search(board, limits);
    }

    std::cout.rdbuf(old);
    relay.flush_line();
    server.send("{\"type\":\"thinking\",\"on\":false}");

    if (engineStop || result.bestMove == NULL_MOVE) return;

    char engineColor;
    {
        std::lock_guard<std::mutex> lk(boardMtx);
        // Color before making the move
        engineColor = (board.sideToMove() == WHITE) ? 'w' : 'b';
        board.makeMove(result.bestMove);
        if (!moveStack.empty())
            moveStack.back().engine = result.bestMove;
        gameLog.push_back({moveToStr(result.bestMove), engineColor});
    }

    std::ostringstream oss;
    oss << "{\"type\":\"enginemove\","
        << "\"move\":\"" << moveToStr(result.bestMove) << "\","
        << "\"score\":" << result.score << "}";
    server.send(oss.str());

    {
        std::lock_guard<std::mutex> lk(boardMtx);
        server.send(boardToJson(board));
        sendGameOver(board, server);
    }
}

// ══════════════════════════════════════════
//  Message handler
// ══════════════════════════════════════════
static void onMessage(const std::string& msg) {
    auto getStr = [&](const std::string& key) -> std::string {
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
        try { return std::stoi(msg.substr(pos)); } catch(...) { return -1; }
    };

    std::string type = getStr("type");

    if (type == "init") {
        std::lock_guard<std::mutex> lk(boardMtx);
        server.send(boardToJson(board));
        std::ostringstream oss;
        oss << "{\"type\":\"settings\","
            << "\"humanIsWhite\":" << (humanIsWhite?"true":"false") << "}";
        server.send(oss.str());
        // Send full game log so frontend can reconstruct history on reconnect
        sendHistory();

    } else if (type == "move") {
        std::string mv = getStr("move");
        int d = getInt("depth");
        if (d >= 1 && d <= 10) engineDepth = d;

        Move m;
        {
            std::lock_guard<std::mutex> lk(boardMtx);
            m = parseMove(board, mv);
        }
        if (m == NULL_MOVE) {
            server.send("{\"type\":\"error\",\"msg\":\"Illegal move\"}");
            return;
        }
        {
            std::lock_guard<std::mutex> lk(boardMtx);
            char humanColor = (board.sideToMove() == WHITE) ? 'w' : 'b';
            board.makeMove(m);
            moveStack.push_back({m, NULL_MOVE});
            gameLog.push_back({moveToStr(m), humanColor});
            server.send(boardToJson(board));
            if (sendGameOver(board, server)) return;
        }
        std::thread(runEngine, engineDepth).detach();

    } else if (type == "undo") {
        engineStop = true;

        std::lock_guard<std::mutex> lk(boardMtx);
        if (moveStack.empty()) {
            server.send("{\"type\":\"error\",\"msg\":\"Nothing to undo\"}");
            return;
        }

        MovePair pair = moveStack.back();
        moveStack.pop_back();

        // Unmake engine move first (if it was made)
        if (pair.engine != NULL_MOVE) {
            board.unmakeMove(pair.engine);
            if (!gameLog.empty()) gameLog.pop_back();
        }
        // Unmake human move
        board.unmakeMove(pair.human);
        if (!gameLog.empty()) gameLog.pop_back();

        int halfMovesToPop = (pair.engine != NULL_MOVE) ? 2 : 1;
        std::ostringstream oss;
        oss << "{\"type\":\"undo\",\"halfmoves\":" << halfMovesToPop << "}";
        server.send(oss.str());
        server.send(boardToJson(board));

        std::ostringstream st;
        st << "{\"type\":\"settings\","
           << "\"humanIsWhite\":" << (humanIsWhite?"true":"false") << "}";
        server.send(st.str());

    } else if (type == "newgame") {
        engineStop = true;
        int d = getInt("depth");
        if (d >= 1 && d <= 10) engineDepth = d;
        {
            std::lock_guard<std::mutex> lk(boardMtx);
            board.setStartingPosition();
            searcher->clearTT();
            moveStack.clear();
            gameLog.clear();
            humanIsWhite = (getStr("side") != "black");
            server.send(boardToJson(board));
            std::ostringstream oss;
            oss << "{\"type\":\"settings\","
                << "\"humanIsWhite\":" << (humanIsWhite?"true":"false") << "}";
            server.send(oss.str());
            sendHistory();  // send empty history to clear frontend
        }
        if (!humanIsWhite)
            std::thread(runEngine, engineDepth).detach();

    } else if (type == "getmoves") {
        std::string sqStr = getStr("sq");
        if (sqStr.empty()) return;
        int sqIdx = -1;
        try { sqIdx = std::stoi(sqStr); } catch(...) { return; }
        if (sqIdx < 0 || sqIdx >= 64) return;
        std::lock_guard<std::mutex> lk(boardMtx);
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
    if (argc > 1) { try { port = std::stoi(argv[1]); } catch(...) {} }

    std::vector<std::string> candidates = {
        "web/index.html", "../web/index.html", "../../web/index.html",
    };
    if (argc > 2) candidates.insert(candidates.begin(), argv[2]);

    std::string htmlPath;
    for (auto& c : candidates) {
        FILE* f = fopen(c.c_str(), "rb");
        if (f) { fclose(f); htmlPath = c; break; }
    }
    if (htmlPath.empty()) {
        fprintf(stderr,
            "Cannot find web/index.html.\n"
            "Run from the knightfall/ folder:\n"
            "  cd knightfall\n"
            "  .\\build\\visualizer.exe\n");
        delete searcher;
        return 1;
    }

    server.listen(port, htmlPath, onMessage);
    delete searcher;
    return 0;
}