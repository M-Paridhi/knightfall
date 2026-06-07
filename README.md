<div align="center">

# ♞ Knightfall

**A classical chess engine built from scratch in C++**

![Language](https://img.shields.io/badge/language-C++14-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Status](https://img.shields.io/badge/status-active%20development-orange.svg)

*Bitboards · Alpha-Beta Pruning · Iterative Deepening · Transposition Tables · UCI Protocol*

</div>

---

## Overview

Knightfall is a fully handcrafted chess engine written in modern C++14 — no external chess libraries, no neural networks, no copied engine code. Every component, from the bitboard representation to the search algorithm, is implemented from first principles.

The project targets **1500–1800 Elo** strength using classical AI techniques and serves as a deep demonstration of algorithms, data structures, systems programming, and software architecture.

---

## Features

| Component | Details |
|---|---|
| **Board Representation** | 64-bit bitboards with incremental Zobrist hashing |
| **Move Generation** | Fully legal move generator — all rules including castling, en passant, promotions |
| **Correctness** | Verified by perft testing to depth 5 across 5 standard positions |
| **Search** | Iterative deepening alpha-beta with quiescence search |
| **Move Ordering** | TT move, MVV-LVA captures, killer heuristic, history heuristic |
| **Evaluation** | Material + piece-square tables (centipawn scale) |
| **Transposition Table** | Zobrist-keyed hash table with exact/upper/lower bounds |
| **Protocol** | Full UCI implementation — works in Arena, CuteChess, any UCI GUI |
| **Terminal Play** | Interactive CLI game with Unicode board display |

---

## Architecture

```
knightfall/
├── src/
│   ├── core/                   # Board representation layer
│   │   ├── types.h             # Enums: Square, Piece, Color, CastlingRight
│   │   ├── bitboard.h/cpp      # 64-bit board, bit manipulation utilities
│   │   ├── move.h              # Move encoding (32-bit packed integer)
│   │   ├── attacks.h/cpp       # Ray-based sliding attacks, precomputed tables
│   │   ├── board.h/cpp         # Board state, FEN, makeMove/unmakeMove, Zobrist
│   │   └── movegen.h/cpp       # Legal move generator
│   │
│   ├── search/                 # AI / decision making layer
│   │   ├── evaluate.h/cpp      # Material + piece-square table evaluation
│   │   ├── search.h/cpp        # Alpha-beta, iterative deepening, quiescence
│   │   └── tt.h/cpp            # Transposition table
│   │
│   ├── protocol/               # Communication layer
│   │   └── uci.h/cpp           # Universal Chess Interface
│   │
│   ├── main.cpp                # UCI engine entry point
│   └── play.cpp                # Interactive terminal game
│
└── tests/
    └── perft.cpp               # Correctness test suite
```

**Dependency flow:** `protocol` → `search` → `core`. No circular dependencies. Each layer has a single responsibility.

---

## Building

### Requirements
- CMake 3.16+
- C++14 compiler (GCC, Clang, or MSVC)
- A CPU with `POPCNT` instruction support (any x86 from ~2008 onward)

### Windows (MinGW)
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Linux / macOS
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

This produces two binaries:
- `knightfall` — the UCI engine
- `play` — the interactive terminal game

---

## Usage

### Play in the terminal
```bash
./play
```
Enter moves in UCI notation: `e2e4`, `g1f3`, `e7e8q` (promotion).

```
+========================================+
|     KNIGHTFALL CHESS ENGINE            |
|     You vs the Engine                  |
+========================================+

Play as [W]hite or [B]lack? w
Engine depth (3=easy, 5=medium, 7=hard) [5]: 5

  +---+---+---+---+---+---+---+---+
 8| r  n  b  q  k  b  n  r |
  +---+---+---+---+---+---+---+---+
 ...
  Your move: e2e4
```

| Command | Action |
|---|---|
| `e2e4` | Make a move |
| `moves` | Show all legal moves |
| `undo` | Take back your last move + engine's reply |
| `flip` | Switch sides mid-game |
| `new` | Start a new game |
| `quit` | Exit |

### Use with a chess GUI (Arena, CuteChess, etc.)

Point any UCI-compatible GUI at `knightfall.exe`. The engine supports:

```
uci          → identify engine
isready      → sync handshake
ucinewgame   → reset state
position     → set board position
go           → begin search
quit         → exit
```

### Run perft correctness tests
```bash
./perft
```
Expected output:
```
=== Knightfall Perft Test Suite ===

Position: Starting Position
  Depth 1: 20        PASS
  Depth 2: 400       PASS
  Depth 3: 8902      PASS
  Depth 4: 197281    PASS  (6 ms)
  Depth 5: 4865609   PASS  (160 ms)

Position: Kiwipete
  Depth 1: 48        PASS
  Depth 2: 2039      PASS
  ...

ALL TESTS PASSED
```

---

## Technical Deep Dive

### Board Representation — Bitboards

Each piece type and color is stored as a 64-bit integer where each bit represents a square. The entire board state fits in 12 `uint64_t` values.

```cpp
Bitboard whitePawns = 0x000000000000FF00ULL; // rank 2, all files
```

Moving all pawns north is a single CPU instruction:
```cpp
Bitboard pushes = whitePawns << 8;  // shift north
```

This enables processing the entire board in parallel rather than square by square.

### Move Encoding

Every move is packed into a 32-bit integer:
```
bits  0– 5 : from square     (64 values)
bits  6–11 : to square       (64 values)
bits 12–13 : move type       (normal / castling / en passant / promotion)
bits 14–15 : promotion piece (N / B / R / Q)
bits 16–31 : move score      (used during ordering)
```

A move list of 256 entries fits in 1KB — well within L1 cache.

### Zobrist Hashing

Each position has a unique 64-bit hash computed incrementally:
```cpp
// Flipping a piece on/off a square XORs its random key
hash ^= zobristTable[color][pieceType][square];
// Side to move
if (sideToMove == BLACK) hash ^= zobristBlackToMove;
```

XOR is its own inverse, so `makeMove` and `unmakeMove` maintain the hash with zero extra work.

**Critical bug found and fixed during development:** The `capturedPiece` must be stored in the history entry *before* saving, not after. If saved after, `unmakeMove` restores the previous position's captured piece instead of the current move's — silently failing to restore captures.

### Alpha-Beta Search

```
alphaBeta(position, depth, α, β):
  if depth == 0: return quiescence(position, α, β)
  for each move in ordered_moves(position):
    score = -alphaBeta(after(move), depth-1, -β, -α)
    if score >= β: return β   ← beta cutoff (prune)
    if score > α: α = score
  return α
```

The transposition table allows reusing results across different move orders, effectively extending search depth by 1–2 plies at no extra cost.

### Sliding Attack Generation — Classical Rays

Sliding piece attacks use precomputed ray bitboards with the positive-ray blocker trick:
```cpp
// Find all squares a rook can reach going north from sq
Bitboard positiveRay(Direction dir, Square sq, Bitboard occupied) {
    Bitboard ray = RayAttacks[dir][sq];
    Bitboard blockers = ray & occupied;
    if (blockers) ray ^= RayAttacks[dir][lsb(blockers)];
    return ray;
}
```

**Why not magic bitboards?** The magic constants we initially used had hash collisions — two different occupancy patterns mapped to the same table index, returning wrong attack sets. Ray attacks are collision-free by construction and only marginally slower at the depths we search.

---

## Correctness Verification

Perft (performance test) counts leaf nodes at a fixed depth. Any deviation from known-correct values indicates a bug in move generation.

| Position | Depth | Nodes | Status |
|---|---|---|---|
| Starting Position | 5 | 4,865,609 | ✅ |
| Kiwipete | 4 | 4,085,603 | ✅ |
| Position 3 | 4 | 43,238 | ✅ |
| Position 4 | 4 | 2,103,487 | ✅ |
| Position 5 | 4 | 3,894,594 | ✅ |

---

## Development Log — Bugs Found

Building a chess engine from scratch involves subtle bugs that only manifest at depth 4+. Notable ones caught during development:

| Bug | Symptom | Root Cause |
|---|---|---|
| `capturedPiece` history | Captures not undone | History saved before `capturedPiece` was set |
| Silent `movePiece` failures | Board corruption at depth 3+ | A failed internal move left board in partial state; subsequent unmake corrupted bitboards |
| Magic bitboard collisions | Wrong rook attacks | Two occupancy patterns mapped to same table index; fixed by switching to classical ray attacks |
| Zobrist hash in `unmakeMove` | Hash wrong after undo | `movePieceSilent` must NOT update hash — hash is already correctly restored from history |

---

## Roadmap

### Phase 4 — Visualization & Polish (upcoming)
- [ ] Web-based board visualization (WebSocket + HTML/JS)
- [ ] Real-time search tree display showing alpha-beta cutoffs
- [ ] Evaluation bar
- [ ] Principal variation display
- [ ] NPS benchmark report

### Phase 5 — Stronger Play
- [ ] Opening book (Italian, Ruy Lopez, Sicilian, Queen's Gambit)
- [ ] Null move pruning (skip a move to detect zugzwang)
- [ ] Late move reductions (search unlikely moves to shallower depth)
- [ ] PGN export
- [ ] Aspiration windows (narrow the search window around expected score)
- [ ] Endgame tablebases

### Future
- [ ] NNUE evaluation (learned from position data)
- [ ] Multi-threading (lazy SMP)

---

## License

This project is licensed under the MIT License. See the LICENSE file for details.

Copyright © 2026 Paridhi Mittal

---

<div align="center">
<sub>Built from scratch. No engine code copied. Every algorithm understood.</sub>
</div>
