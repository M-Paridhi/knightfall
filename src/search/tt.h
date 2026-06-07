#pragma once
#include "core/types.h"
#include "core/move.h"
#include "evaluate.h"

// ──────────────────────────────────────────
//  Transposition Table Entry
//
//  When we finish searching a position to depth D,
//  we store the result. If we encounter the same
//  position again (via a different move order),
//  we can reuse the result instead of re-searching.
//
//  The bound type tells us what the score means:
//  EXACT  — the score is the true minimax value
//  LOWER  — we found a move at least this good (failed high / beta cutoff)
//  UPPER  — all moves were worse than alpha (failed low)
// ──────────────────────────────────────────
enum Bound : uint8_t { BOUND_NONE, BOUND_EXACT, BOUND_LOWER, BOUND_UPPER };

struct TTEntry {
    uint64_t key   = 0;        // full Zobrist hash for collision detection
    Score    score = 0;        // score at this node
    Move     move  = NULL_MOVE;// best move found
    int8_t   depth = 0;        // depth this was searched to
    Bound    bound = BOUND_NONE;
};

// ──────────────────────────────────────────
//  Transposition Table
//
//  A fixed-size hash table. Size is always a
//  power of two so we can use & instead of %.
// ──────────────────────────────────────────
class TranspositionTable {
public:
    explicit TranspositionTable(size_t sizeMB = 16);
    ~TranspositionTable();

    void  clear();
    void  store(uint64_t key, Score score, Move move, int depth, Bound bound);
    bool  probe(uint64_t key, TTEntry& entry) const;

    // How full is the table? (per mille, 0-1000)
    int hashfull() const;

private:
    TTEntry*  table   = nullptr;
    size_t    numEntries = 0;

    size_t index(uint64_t key) const { return key & (numEntries - 1); }
};
