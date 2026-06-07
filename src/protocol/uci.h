#pragma once
#include "core/board.h"
#include "search/search.h"

// ──────────────────────────────────────────
//  UCI (Universal Chess Interface)
//
//  The standard protocol for chess engines.
//  The GUI sends text commands on stdin,
//  the engine replies on stdout.
//
//  Supported commands:
//    uci           — identify the engine
//    isready       — sync (engine replies "readyok")
//    ucinewgame    — reset for a new game
//    position      — set the board position
//    go            — start searching
//    stop          — stop searching
//    quit          — exit
// ──────────────────────────────────────────
class UCIProtocol {
public:
    UCIProtocol();
    void run();   // main loop

private:
    Board    board;
    Searcher searcher;

    void handleUCI();
    void handleIsReady();
    void handleNewGame();
    void handlePosition(const std::string& line);
    void handleGo(const std::string& line);

    // Parse a move in UCI notation ("e2e4", "e7e8q")
    Move parseMove(const std::string& str);
};
