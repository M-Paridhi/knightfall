#include "core/board.h"
#include "core/attacks.h"
#include "core/movegen.h"
#include <iostream>
#include <chrono>
#include <string>

uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    MoveList list;
    generateMoves(board, list);
    if (depth == 1) return list.count;
    uint64_t nodes = 0;
    for (int i = 0; i < list.count; ++i) {
        board.makeMove(list.moves[i]);
        nodes += perft(board, depth - 1);
        board.unmakeMove(list.moves[i]);
    }
    return nodes;
}

struct TestCase {
    const char* name;
    const char* fen;
    int         maxDepth;
    uint64_t    expected[5];
};

static const TestCase TESTS[] = {
    { "Starting Position",
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      5, {20, 400, 8902, 197281, 4865609} },

    { "Kiwipete",
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      4, {48, 2039, 97862, 4085603, 0} },

    { "Position 3",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      4, {14, 191, 2812, 43238, 0} },

    { "Position 4",
      "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      4, {44, 1486, 62379, 2103487, 0} },

    { "Position 5",
      "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
      4, {46, 2079, 89890, 3894594, 0} },
};

int main() {
    initAttacks();

    std::cout << "=== Knightfall Perft Test Suite ===\n\n";

    bool allPassed = true;

    for (const auto& test : TESTS) {
        std::cout << "Position: " << test.name << "\n";
        Board board;
        board.setFromFEN(test.fen);

        for (int d = 1; d <= test.maxDepth; ++d) {
            if (test.expected[d-1] == 0) continue;
            auto start = std::chrono::high_resolution_clock::now();
            uint64_t result = perft(board, d);
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();

            bool pass = (result == test.expected[d-1]);
            allPassed &= pass;

            std::cout << "  Depth " << d << ": " << result;
            if (pass) std::cout << "  PASS";
            else      std::cout << "  FAIL (expected " << test.expected[d-1] << ")";
            if (ms > 1.0) std::cout << "  (" << (int)ms << " ms)";
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    std::cout << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    return allPassed ? 0 : 1;
}
