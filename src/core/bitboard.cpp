#include "bitboard.h"
#include <sstream>

const Bitboard SquareBB[64] = {
    1ULL <<  0, 1ULL <<  1, 1ULL <<  2, 1ULL <<  3,
    1ULL <<  4, 1ULL <<  5, 1ULL <<  6, 1ULL <<  7,
    1ULL <<  8, 1ULL <<  9, 1ULL << 10, 1ULL << 11,
    1ULL << 12, 1ULL << 13, 1ULL << 14, 1ULL << 15,
    1ULL << 16, 1ULL << 17, 1ULL << 18, 1ULL << 19,
    1ULL << 20, 1ULL << 21, 1ULL << 22, 1ULL << 23,
    1ULL << 24, 1ULL << 25, 1ULL << 26, 1ULL << 27,
    1ULL << 28, 1ULL << 29, 1ULL << 30, 1ULL << 31,
    1ULL << 32, 1ULL << 33, 1ULL << 34, 1ULL << 35,
    1ULL << 36, 1ULL << 37, 1ULL << 38, 1ULL << 39,
    1ULL << 40, 1ULL << 41, 1ULL << 42, 1ULL << 43,
    1ULL << 44, 1ULL << 45, 1ULL << 46, 1ULL << 47,
    1ULL << 48, 1ULL << 49, 1ULL << 50, 1ULL << 51,
    1ULL << 52, 1ULL << 53, 1ULL << 54, 1ULL << 55,
    1ULL << 56, 1ULL << 57, 1ULL << 58, 1ULL << 59,
    1ULL << 60, 1ULL << 61, 1ULL << 62, 1ULL << 63
};

const Bitboard FileMask[8] = {
    0x0101010101010101ULL,
    0x0202020202020202ULL,
    0x0404040404040404ULL,
    0x0808080808080808ULL,
    0x1010101010101010ULL,
    0x2020202020202020ULL,
    0x4040404040404040ULL,
    0x8080808080808080ULL
};

const Bitboard RankMask[8] = {
    0x00000000000000FFULL,
    0x000000000000FF00ULL,
    0x0000000000FF0000ULL,
    0x00000000FF000000ULL,
    0x000000FF00000000ULL,
    0x0000FF0000000000ULL,
    0x00FF000000000000ULL,
    0xFF00000000000000ULL
};

std::string bitboardToString(Bitboard bb) {
    std::ostringstream oss;
    oss << "\n";
    for (int rank = 7; rank >= 0; --rank) {
        oss << (rank + 1) << " | ";
        for (int file = 0; file < 8; ++file) {
            Square s = makeSquare(File(file), Rank(rank));
            oss << (testBit(bb, s) ? "1 " : ". ");
        }
        oss << "\n";
    }
    oss << "    a b c d e f g h\n";
    return oss.str();
}