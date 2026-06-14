#include "search.h"
#include "core/attacks.h"
#include <algorithm>
#include <cstring>
#include <iostream>

// ══════════════════════════════════════════
//  Constructor
// ══════════════════════════════════════════
Searcher::Searcher(size_t ttSizeMB) : tt(ttSizeMB) {
    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));
}

// ══════════════════════════════════════════
//  Time management
// ══════════════════════════════════════════
bool Searcher::timeUp() const {
    auto now = std::chrono::high_resolution_clock::now();
    int elapsed = (int)std::chrono::duration<double, std::milli>(
                      now - startTime).count();
    return elapsed >= timeLimitMs;
}

// ══════════════════════════════════════════
//  Mate score adjustment
//
//  Mate scores in the TT must be adjusted for
//  the ply at which they were found. A mate in 3
//  from the root is a mate in 2 from ply 1.
//  Without adjustment, the TT would confuse
//  mate distances across different plies.
// ══════════════════════════════════════════
Score Searcher::scoreToTT(Score s, int ply) const {
    if (s >  SCORE_MATE - 200) return s + ply;
    if (s < -SCORE_MATE + 200) return s - ply;
    return s;
}

Score Searcher::scoreFromTT(Score s, int ply) const {
    if (s >  SCORE_MATE - 200) return s - ply;
    if (s < -SCORE_MATE + 200) return s + ply;
    return s;
}

// ══════════════════════════════════════════
//  Move ordering
//
//  Priority (highest first):
//  1. TT move (best move from previous search)
//  2. Captures sorted by MVV-LVA
//  3. Promotions
//  4. Killer moves (quiet moves causing cutoffs)
//  5. History heuristic (quiet moves that were historically good)
// ══════════════════════════════════════════
static const int PIECE_VALUE[7] = {100, 320, 330, 500, 900, 20000, 0};

void Searcher::orderMoves(MoveList& list, const Board& board,
                          Move ttMove, int ply) {
    Color us = board.sideToMove();

    for (int i = 0; i < list.count; ++i) {
        Move  m   = list.moves[i];
        int   sc  = 0;

        if (m == ttMove) {
            sc = 2'000'000;
        } else {
            Square   from     = moveFrom(m);
            Square   to       = moveTo(m);
            Piece    captured = (moveType(m) == EN_PASSANT)
                                ? Piece(~us * 6 + PAWN)
                                : board.pieceOn(to);

            if (captured != NO_PIECE) {
                // MVV-LVA: victim value * 10 - attacker value
                PieceType victim   = typeOf(captured);
                PieceType attacker = typeOf(board.pieceOn(from));
                sc = 1'000'000 + PIECE_VALUE[victim] * 10 - PIECE_VALUE[attacker];
            } else if (moveType(m) == PROMOTION) {
                sc = 900'000 + PIECE_VALUE[movePromotion(m)];
            } else if (m == killers[ply][0]) {
                sc = 800'000;
            } else if (m == killers[ply][1]) {
                sc = 799'000;
            } else {
                // History heuristic: reward moves that previously caused cutoffs
                sc = history[us][from][to];
            }
        }

        setMoveScore(list.moves[i], sc);
    }

    // Partial selection sort — pick best each iteration
    for (int i = 0; i < list.count; ++i) {
        int best = i;
        for (int j = i + 1; j < list.count; ++j)
            if (getMoveScore(list.moves[j]) > getMoveScore(list.moves[best]))
                best = j;
        std::swap(list.moves[i], list.moves[best]);
    }
}

// ══════════════════════════════════════════
//  Quiescence Search
// ══════════════════════════════════════════
Score Searcher::quiescence(Board& board, Score alpha, Score beta, int ply) {
    ++nodesSearched;

    Score standPat = evaluate(board);
    if (board.sideToMove() == BLACK) standPat = -standPat;

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    MoveList captures;
    generateCaptures(board, captures);
    orderMoves(captures, board, NULL_MOVE, ply);

    for (int i = 0; i < captures.count; ++i) {
        board.makeMove(captures.moves[i]);
        Score score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(captures.moves[i]);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// ══════════════════════════════════════════
//  Alpha-Beta with TT and History Heuristic
// ══════════════════════════════════════════
Score Searcher::alphaBeta(Board& board, int depth, Score alpha, Score beta,
                          int ply, bool isPV) {
    if (depth == 0) return quiescence(board, alpha, beta, ply);

    ++nodesSearched;

    // Time check every 4096 nodes
    if ((nodesSearched & 4095) == 0 && timeUp()) return 0;

    // ── Transposition Table probe ──────────
    TTEntry entry;
    Move    ttMove = NULL_MOVE;

    if (tt.probe(board.hash(), entry)) {
        ttMove = entry.move;

        if (entry.depth >= depth) {
            Score ttScore = scoreFromTT(entry.score, ply);

            if (entry.bound == BOUND_EXACT) return ttScore;
            if (entry.bound == BOUND_LOWER && ttScore >= beta)  return ttScore;
            if (entry.bound == BOUND_UPPER && ttScore <= alpha) return ttScore;
        }
    }

    // ── Generate and order moves ───────────
    MoveList list;
    generateMoves(board, list);

    if (list.count == 0) {
        Square ksq = board.kingSquare(board.sideToMove());
        if (isAttacked(board, ksq, ~board.sideToMove()))
            return -(SCORE_MATE - ply);   // mate: penalise longer mates
        return SCORE_DRAW;               // stalemate
    }

    if (board.halfmoveClock() >= 100) return SCORE_DRAW;

    orderMoves(list, board, ttMove, ply);

    // ── Search each move ───────────────────
    Score  bestScore = -SCORE_INFINITE;
    Move   bestMove  = NULL_MOVE;
    Bound  bound     = BOUND_UPPER;   // assume we'll fail low

    for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];

        board.makeMove(m);
        Score score = -alphaBeta(board, depth - 1, -beta, -alpha,
                                 ply + 1, isPV && i == 0);
        board.unmakeMove(m);

        if (timeUp()) return 0;

        if (score > bestScore) {
            bestScore = score;
            bestMove  = m;

            if (score > alpha) {
                alpha = score;
                bound = BOUND_EXACT;

                if (alpha >= beta) {
                    // Beta cutoff — update killers and history
                    bool isQuiet = (board.pieceOn(moveTo(m)) == NO_PIECE &&
                                    moveType(m) != EN_PASSANT);
                    if (isQuiet) {
                        // Killer: store the two most recent quiet cutoff moves
                        if (m != killers[ply][0]) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = m;
                        }
                        // History: bonus proportional to depth squared
                        Color us = board.sideToMove();
                        history[us][moveFrom(m)][moveTo(m)] += depth * depth;

                        // Decay history for other moves to avoid overflow
                        if (history[us][moveFrom(m)][moveTo(m)] > 1'000'000)
                            for (auto& row : history)
                                for (auto& col : row)
                                    for (auto& val : col)
                                        val /= 2;
                    }
                    bound = BOUND_LOWER;
                    break;
                }
            }
        }
    }

    // ── Store in TT ───────────────────────
    tt.store(board.hash(), scoreToTT(bestScore, ply), bestMove, depth, bound);

    return bestScore;
}

// ══════════════════════════════════════════
//  Iterative Deepening — public entry point
// ══════════════════════════════════════════
SearchResult Searcher::search(Board& board, const SearchLimits& limits) {
    // Reset state
    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));
    nodesSearched = 0;
    startTime     = std::chrono::high_resolution_clock::now();
    timeLimitMs   = limits.movetime;

    SearchResult result;

    for (int depth = 1; depth <= limits.maxDepth; ++depth) {
        // Search at this depth
        Score score = alphaBeta(board, depth, -SCORE_INFINITE, SCORE_INFINITE,
                                0, true);

        if (timeUp() && depth > 1) break;  // don't use partial result

        // Retrieve best move from TT (root position)
        TTEntry entry;
        if (tt.probe(board.hash(), entry) && entry.move != NULL_MOVE) {
            result.bestMove = entry.move;
            result.score    = score;
        }

        result.depth = depth;
        result.nodes = nodesSearched;

        // Timing info
        auto now = std::chrono::high_resolution_clock::now();
        int  ms  = (int)std::chrono::duration<double, std::milli>(
                       now - startTime).count();
        int  nps = (ms > 0) ? (int)(nodesSearched * 1000 / ms) : 0;

        // UCI info line
        std::cout << "info depth "  << depth
                  << " score cp "   << score
                  << " nodes "      << nodesSearched
                  << " nps "        << nps
                  << " hashfull "   << tt.hashfull()
                  << " time "       << ms;

        if (result.bestMove != NULL_MOVE) {
            Square f = moveFrom(result.bestMove), t = moveTo(result.bestMove);
            std::cout << " pv "
                      << char('a' + fileOf(f)) << char('1' + rankOf(f))
                      << char('a' + fileOf(t)) << char('1' + rankOf(t));
            // Promotion piece must be appended — required by UCI spec
            if (moveType(result.bestMove) == PROMOTION) {
                const char* pp = "nbrq";
                std::cout << pp[movePromotion(result.bestMove) - KNIGHT];
            }
        }
        std::cout << "\n";
        std::cout.flush();

        // Stop if we found a forced mate
        if (score > SCORE_MATE - 200 || score < -SCORE_MATE + 200) break;
    }

    return result;
}
