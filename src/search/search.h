#pragma once
#include "core/board.h"
#include "core/movegen.h"
#include "evaluate.h"
#include "tt.h"
#include <cstdint>
#include <chrono>

// ── Search result ─────────────────────────────────────────────────────────
struct SearchResult {
    Move     bestMove = NULL_MOVE;
    Score    score    = 0;
    int      depth    = 0;
    uint64_t nodes    = 0;
};

// ── Search limits ─────────────────────────────────────────────────────────
struct SearchLimits {
    int maxDepth = 64;   // hard depth cap
    int movetime = 1000; // milliseconds per move
};

// ── Searcher ──────────────────────────────────────────────────────────────
class Searcher {
public:
    explicit Searcher(size_t ttSizeMB = 16);

    SearchResult search(Board& board, const SearchLimits& limits);
    void clearTT()        { tt.clear(); }

    uint64_t nodesSearched = 0;

private:
    TranspositionTable tt;

    // Killer moves: quiet moves causing beta cutoffs, per depth
    Move killers[128][2];

    // History heuristic: bonus for quiet moves causing cutoffs
    int  history[2][64][64]; // [color][from][to]

    // Time management
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    int timeLimitMs = 1000;
    bool timeUp()  const;

    // Core search functions
    Score alphaBeta(Board& board, int depth, Score alpha, Score beta,
                    int ply, bool isPV);
    Score quiescence(Board& board, Score alpha, Score beta, int ply);

    // Move ordering
    void orderMoves(MoveList& list, const Board& board, Move ttMove, int ply);

    // Score adjustments for mate distance
    Score scoreToTT(Score s, int ply)   const;
    Score scoreFromTT(Score s, int ply) const;
};
