#include "core/board.h"
#include <iostream>

int main() {
    Board board;

    std::cout << "=== Knightfall Chess Engine ===\n";
    std::cout << "Starting position:\n";
    board.print();

    // Test FEN round-trip
    std::string fen = board.toFEN();
    std::cout << "FEN export: " << fen << "\n";

    // Load a custom position (Sicilian Defence after 1.e4 c5)
    board.setFromFEN("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
    std::cout << "\nSicilian Defence (1.e4 c5):\n";
    board.print();

    std::cout << "All basic tests passed!\n";
    return 0;
}