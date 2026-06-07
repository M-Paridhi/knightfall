#include "tt.h"
#include <cstring>
#include <cassert>

TranspositionTable::TranspositionTable(size_t sizeMB) {
    // Round down to nearest power of two entries
    size_t bytes = sizeMB * 1024 * 1024;
    numEntries = 1;
    while (numEntries * sizeof(TTEntry) <= bytes) numEntries <<= 1;
    numEntries >>= 1;  // one step back to fit in size

    table = new TTEntry[numEntries];
    clear();
}

TranspositionTable::~TranspositionTable() {
    delete[] table;
}

void TranspositionTable::clear() {
    memset(table, 0, numEntries * sizeof(TTEntry));
}

void TranspositionTable::store(uint64_t key, Score score, Move move,
                               int depth, Bound bound) {
    TTEntry& e = table[index(key)];

    // Replacement strategy: always replace if:
    // - Different position (new key)
    // - Same position but searched deeper
    // - Exact bound replaces approximate
    if (e.key != key ||
        depth >= e.depth ||
        bound == BOUND_EXACT) {
        e.key   = key;
        e.score = score;
        e.depth = (int8_t)depth;
        e.bound = bound;
        if (move != NULL_MOVE) e.move = move;
    }
}

bool TranspositionTable::probe(uint64_t key, TTEntry& entry) const {
    const TTEntry& e = table[index(key)];
    if (e.key != key) return false;
    entry = e;
    return true;
}

int TranspositionTable::hashfull() const {
    // Sample first 1000 entries
    int filled = 0;
    size_t sample = (numEntries < 1000) ? numEntries : 1000;
    for (size_t i = 0; i < sample; ++i)
        if (table[i].key != 0) ++filled;
    return (int)(filled * 1000 / sample);
}
