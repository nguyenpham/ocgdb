/*
 This file is part of Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 Distributed under the MIT License (MIT) (See accompanying file LICENSE
 or copy at http://opensource.org/licenses/MIT)
 */

/// What the chess board needs for game databases and PQL, and Banksia's core (which plays
/// games, and does not store them) does not have: the encodings of the binary move fields
/// (Moves2, Moves1), the keys for finding duplicates, the bitboards PQL evaluates and the
/// shifting of PQL patterns. Taken from Banksia GUI's core (banksiagui banksia/chess/chess.cpp),
/// where these functions live in chess.cpp; they are kept in a file of their own
/// here so that chess.cpp stays as close to Banksia's as possible.

#include <cassert>
#include <iostream>
#include <unordered_map>

#include "chess.h"

using namespace banksia;

extern u64 polyglotRandom64[800];

namespace {
    /// square names in this board's order (a8 = 0) and in Stockfish's (a1 = 0), for the 2-byte
    /// encoding, which writes squares as Stockfish does (banksiagui banksia/base/types.h)
    enum ChessPos {
        pos_a8, pos_b8, pos_c8, pos_d8, pos_e8, pos_f8, pos_g8, pos_h8,
        pos_a7, pos_b7, pos_c7, pos_d7, pos_e7, pos_f7, pos_g7, pos_h7,
        pos_a6, pos_b6, pos_c6, pos_d6, pos_e6, pos_f6, pos_g6, pos_h6,
        pos_a5, pos_b5, pos_c5, pos_d5, pos_e5, pos_f5, pos_g5, pos_h5,
        pos_a4, pos_b4, pos_c4, pos_d4, pos_e4, pos_f4, pos_g4, pos_h4,
        pos_a3, pos_b3, pos_c3, pos_d3, pos_e3, pos_f3, pos_g3, pos_h3,
        pos_a2, pos_b2, pos_c2, pos_d2, pos_e2, pos_f2, pos_g2, pos_h2,
        pos_a1, pos_b1, pos_c1, pos_d1, pos_e1, pos_f1, pos_g1, pos_h1,
    };

    enum StockFishSquare {
        SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
        SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
        SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
        SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
        SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
        SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
        SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
        SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    };
} // namespace

// embede game length and result
uint64_t ChessBoard::getHashKeyForCheckingDuplicates(int h) const
{
    /// on the main line: the history also keeps the moves of variations
    auto line = _mainLineHists();
    uint64_t hk;
    size_t n = line.size(), t;
    if (h < 0 || static_cast<size_t>(h) >= n) {
        hk = hashKey;
        t = n;
    } else {
        hk = line.at(h)->hashKey;
        t = h;
    }

    return hk ^ polyglotRandom64[t];
}

uint64_t ChessBoard::_posToBitboard[64];
uint64_t ChessBoard::bb_edge_left, ChessBoard::bb_edge_right, ChessBoard::bb_edge_top,
        ChessBoard::bb_edge_bottom;

static const int toSFPos[] {
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8, SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7,
    SQ_F7, SQ_G7, SQ_H7, SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6, SQ_A5, SQ_B5,
    SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5, SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4,
    SQ_H4, SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3, SQ_A2, SQ_B2, SQ_C2, SQ_D2,
    SQ_E2, SQ_F2, SQ_G2, SQ_H2, SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
};

static const int fromSFPos[] { pos_a1, pos_b1, pos_c1, pos_d1, pos_e1, pos_f1, pos_g1, pos_h1, pos_a2, pos_b2,
                               pos_c2, pos_d2, pos_e2, pos_f2, pos_g2, pos_h2, pos_a3, pos_b3, pos_c3, pos_d3,
                               pos_e3, pos_f3, pos_g3, pos_h3, pos_a4, pos_b4, pos_c4, pos_d4, pos_e4, pos_f4,
                               pos_g4, pos_h4, pos_a5, pos_b5, pos_c5, pos_d5, pos_e5, pos_f5, pos_g5, pos_h5,
                               pos_a6, pos_b6, pos_c6, pos_d6, pos_e6, pos_f6, pos_g6, pos_h6, pos_a7, pos_b7,
                               pos_c7, pos_d7, pos_e7, pos_f7, pos_g7, pos_h7, pos_a8, pos_b8, pos_c8, pos_d8,
                               pos_e8, pos_f8, pos_g8, pos_h8 };

// for encoding/decoding a bishop move into one byte
static std::vector<int> bishopStartPointsVec;

void ChessBoard::staticInit()
{
    bb_edge_left = bb_edge_right = 0;
    bb_edge_bottom = 0xffULL;
    bb_edge_top = 0xffULL << 56;

    for (auto i = 0; i < 64; ++i) {
        _posToBitboard[i] = 1ULL << toSFPos[i];
        bishopStartPointsVec.push_back(0);

        auto c = i % 8;
        if (c == 0) {
            bb_edge_left |= 1ULL << i;
        } else if (c == 7) {
            bb_edge_right |= 1ULL << i;
        }
    }

    // for encoding/decoding a bishop move into one byte
    for (auto pos = 0; pos < 8; ++pos) {
        // down right from the top row (row 8)
        for (auto i = 0; i < 8 - pos; i++) {
            auto k = pos + i * 9;
            assert(k >= 0 && k < 64);
            bishopStartPointsVec[k] |= pos;
        }

        // up right from the bottm row (1)
        for (auto i = 0;; i++) {
            auto k = 56 + pos - i * 7; // last row
            bishopStartPointsVec[k] |= (56 + pos) << 8;
            if (k % 8 >= 7) break;
        }
    }

    // from column a
    for (auto pos = 8; pos < 64; pos += 8) {
        // down right from column a
        for (auto i = 0;; i++) {
            auto k = pos + i * 9;
            if (k >= 64) break;
            bishopStartPointsVec[k] |= pos;
        }

        // up right from column a
        for (auto i = 0;; i++) {
            auto k = pos - i * 7;
            if (k < 0) break;
            bishopStartPointsVec[k] |= pos << 8;
        }
    }
}

// "e2", "a7"
uint64_t ChessBoard::posToBitboard(const char* s)
{
    auto c = s[0], d = s[1];
    if (c >= 'a' && c <= 'h' && d >= '1' && d <= '8') {
        auto pos = static_cast<int>('8' - d) * 8 + static_cast<int>(c - 'a');
        assert(pos >= 0 && pos < 64);
        return _posToBitboard[pos];
    }

    return 0;
}

BitboardBase* ChessBoard::posToBitboards() const
{
    auto bb = new ChessBitboard;
    bb->bitboardVec = std::vector<uint64_t> { hashKey, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
                                              0ULL,    0ULL, 0ULL, 0ULL, 0ULL, 0ULL

    };
    assert(bb->bitboardVec.size() == static_cast<int>(BBIdx::max));


    for (auto i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            continue;
        }
        auto k = _posToBitboard[i];
        auto sd = static_cast<int>(piece.side);
        assert(sd == 0 || sd == 1);

        bb->bitboardVec[static_cast<int>(BBIdx::kings) + piece.type - 1] |= k;
        bb->bitboardVec[static_cast<int>(BBIdx::black) + sd] |= k;

        if (piece.type == static_cast<int>(PieceTypeStd::king)) {
            bb->bitboardVec[static_cast<int>(BBIdx::blackkingsquare) + sd] = k;
        }
    }

    int64_t rights0 = castleRights[0], rights1 = castleRights[1];
    bb->bitboardVec[static_cast<int>(BBIdx::prop)] = (enpassant & 0xff) | rights0 << 8 | rights1 << 10;
    return bb;
}


std::string ChessBoard::bitboard2string(uint64_t bb)
{
    std::string s;
    uint64_t k = 1;
    for (auto i = 0; i < 64; i++, k <<= 1) {
        s += (bb & k) != 0 ? "1 " : ". ";
        if (i && (i & 7) == 7) {
            s += "\n";
        }
    }

    s += "bb: " + std::to_string(bb) + "\n";
    return s;
}


// Our: empty, king, queen, rook, bishop, knight, pawn
// SF : NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
uint16_t ChessBoard::encode2Bytes(const Hist& hist) const
{
    return toSFPos[hist.move.from] | toSFPos[hist.move.dest] << 6
           | (hist.move.promotion > 0 ? ((PAWNSTD + 1 - hist.move.promotion) << 12) : 0);
}

Move ChessBoard::decode2Bytes(uint16_t d) const
{
    Move m;
    m.from = fromSFPos[d & 0x3f];
    m.dest = fromSFPos[(d >> 6) & 0x3f];
    m.promotion = d >> 12;
    if (m.promotion > 0) {
        m.promotion = PAWNSTD + 1 - m.promotion;
    }

    /// not checked for validity, because of variation levels
    assert(m.promotion == EMPTY || (m.promotion > KING && m.promotion < PAWNSTD));
    return m;
}

static const std::unordered_map<int, int> knightMoveIndexMap {
    { -17, 0 }, { -15, 1 }, { -10, 2 }, { -6, 3 }, { 6, 4 }, { 10, 5 }, { 15, 6 }, { 17, 7 },
};

static const std::vector<int> knightMoveIndexVec { -17, -15, -10, -6, 6, 10, 15, 17 };

static const std::unordered_map<int, int> kingMoveIndexMap {
    { -9, 0 }, { -8, 1 }, { -7, 2 }, { -2, 3 }, { -1, 4 }, // +- 2 is castling
    { 1, 5 },  { 2, 6 },  { 7, 7 },  { 8, 8 },  { 9, 9 },
};
static const std::vector<int> kingMoveIndexVec { -9, -8, -7, -2, -1, 1, 2, 7, 8, 9 };


static const std::unordered_map<int, int> pawnMoveIndexMap {
    { 7, 0 },
    { 8, 1 },
    { 9, 2 },
    { 16, 3 },
};

static const std::vector<int> pawnMoveIndexVec { 7, 8, 9, 16 };

std::pair<uint16_t, int> ChessBoard::encode1Byte(MoveFull move)
{
    uint16_t t = move.piece.idx;
    assert(t >= 0 && t < 16);

    std::pair<uint16_t, int> pair;

    auto type = static_cast<PieceTypeStd>(move.piece.type);
    // Queen move takes 2 byte
    if (type == PieceTypeStd::queen) {
        t |= static_cast<int16_t>(move.dest << 4);
        pair.second = 2;

        assert((t >> 4) < 64);
    } else {
        switch (type) {
            case PieceTypeStd::king: {
                auto d = move.dest - move.from;
                auto it = kingMoveIndexMap.find(d);
                if (it != kingMoveIndexMap.end()) {
                    t |= it->second << 4;
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::knight: {
                auto d = move.dest - move.from;
                auto it = knightMoveIndexMap.find(d);
                if (it != knightMoveIndexMap.end()) {
                    t |= it->second << 4;
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::pawn: {
                auto d = std::abs(move.dest - move.from);
                auto it = pawnMoveIndexMap.find(d);
                if (it != pawnMoveIndexMap.end()) {
                    t |= it->second << 4;
                    if (move.promotion >= static_cast<int>(PieceTypeStd::queen)) {
                        auto promotion = move.promotion - static_cast<int>(PieceTypeStd::queen);
                        t |= promotion << 6;
                    }
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::rook: {
                if (move.from / 8 == move.dest / 8) { // same row
                    t |= (move.dest % 8) << 5;
                } else {
                    t |= 1 << 4 | (move.dest / 8) << 5;
                }
                break;
            }
            case PieceTypeStd::bishop: {
                auto sf = bishopStartPointsVec.at(move.from);
                auto sd = bishopStartPointsVec.at(move.dest);

                if ((sf & 0xff) == (sd & 0xff)) {
                    assert((sf >> 8) != (sd >> 8));
                    auto s = sd & 0xff;
                    assert(s < 64);
                    assert(move.dest >= s && (move.dest - s) % 9 == 0);
                    auto k = (move.dest - s) / 9;
                    assert(k >= 0 && k < 8);
                    t |= k << 5;
                } else {
                    assert((sf >> 8) == (sd >> 8));
                    auto s = sd >> 8;
                    assert(s < 64);
                    assert(move.dest <= s && (move.dest - s) % 7 == 0);
                    auto k = (s - move.dest) / 7;
                    assert(k >= 0 && k < 8);
                    t |= 1 << 4 | k << 5;
                }
                break;
            }

            default:
                break;
        }
        pair.second = 1;
    }

    pair.first = t;
    assert(pair.second == 1 || pair.second == 2);
    return pair;
}

std::pair<Move, int> ChessBoard::decode1Byte(const int8_t* d)
{
    // first 4 bits is the index of the piece
    uint8_t dd = *d;
    auto idx = static_cast<int>(dd & 0xf);

    std::pair<Move, int> pair;
    int from;

    for (from = 0; from < 64; ++from) {
        if (pieces[from].idx == idx && pieces[from].side == side) {
            break;
        }
    }

    if (from >= 64) {
        std::cout << "Error on decode1Byte. Stop!" << std::endl;
        return pair; // something wrong
    }

    auto n = 1;
    Move m;
    m.promotion = 0;

    auto piece = pieces[from];
    m.from = from;

    auto type = static_cast<PieceTypeStd>(piece.type);
    switch (type) {
        case PieceTypeStd::king: {
            auto k = (dd >> 4) & 0xf;
            m.dest = from + kingMoveIndexVec.at(k);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::knight: {
            auto k = (dd >> 4) & 0xf;
            m.dest = from + knightMoveIndexVec.at(k);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::pawn: {
            auto k = (dd >> 4) & 0x3;
            auto x = pawnMoveIndexVec.at(k);
            m.dest = from + (piece.side == Side::white ? -x : x);
            assert(BoardCore::isValid(m));

            if (m.dest < 8 || m.dest >= 56) {
                m.promotion = ((dd >> 6) & 0x3) + static_cast<int>(PieceTypeStd::queen);
            }
            break;
        }
        case PieceTypeStd::queen: {
            auto q = reinterpret_cast<const uint16_t*>(d);
            auto qq = *q;
            assert((qq & 0xff) == dd);
            m.dest = qq >> 4;
            n = 2;
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::rook: {
            auto k = dd >> 5;
            assert(k < 8);
            if (dd & (1 << 4)) { // same column
                m.dest = k * 8 + (m.from % 8);
                assert(m.from % 8 == m.dest % 8);
            } else {
                int r = m.from / 8;
                m.dest = r * 8 + k;
                assert(m.from / 8 == m.dest / 8);
            }
            assert(m.from / 8 == m.dest / 8 || m.from % 8 == m.dest % 8);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::bishop: {
            auto sf = bishopStartPointsVec.at(m.from);
            auto k = dd >> 5;
            if (dd & (1 << 4)) {
                auto s = sf >> 8;
                assert(s < 64);
                m.dest = s - k * 7;
                assert(BoardCore::isValid(m));
            } else {
                auto s = sf & 0xff;
                assert(s < 64);
                m.dest = s + k * 9;
                assert(BoardCore::isValid(m));
            }
            break;
        }

        default:
            break;
    }

    pair.first = m;
    pair.second = n;
    assert(BoardCore::isValid(m));
    return pair;
}


/// All the places a pattern can stand on the board (PQL's "#"). The first entry is the pattern itself;
/// every other entry is a COPY moved by some rows and columns.
void ChessBoard::pattern_shift(std::vector<BitboardBase*>& patternBitBoards)
{
    assert(patternBitBoards.size() == 1);

    /// a copy of `from` moved one step, or null when it cannot (the step may fail after it has
    /// changed the board it was given, so it works on the copy)
    auto stepped = [](const ChessBitboard* from, bool (*step)(ChessBitboard*)) -> ChessBitboard* {
        auto v = new ChessBitboard(*from);
        if (step(v)) {
            return v;
        }
        delete v;
        return nullptr;
    };

    auto origin = static_cast<ChessBitboard*>(patternBitBoards.at(0));

    /// the rows: the pattern's own, then each one up as far as it goes, then down
    std::vector<ChessBitboard*> rows { origin };
    for (auto v = origin; (v = stepped(v, pattern_shift_up)) != nullptr;) {
        rows.push_back(v);
    }
    for (auto v = origin; (v = stepped(v, pattern_shift_down)) != nullptr;) {
        rows.push_back(v);
    }

    /// and in every row, left and right
    patternBitBoards.clear();
    for (auto row : rows) {
        patternBitBoards.push_back(row);
        for (auto v = row; (v = stepped(v, pattern_shift_left)) != nullptr;) {
            patternBitBoards.push_back(v);
        }
        for (auto v = row; (v = stepped(v, pattern_shift_right)) != nullptr;) {
            patternBitBoards.push_back(v);
        }
    }
}

bool ChessBoard::pattern_shift_up(ChessBitboard* v)
{
    assert(v && !v->isEmpty());
    auto bb = v->bitboardVec[static_cast<int>(banksia::BBIdx::black)]
              | v->bitboardVec[static_cast<int>(banksia::BBIdx::white)];
    assert(bb);

    if (bb & banksia::ChessBoard::bb_edge_top) return false;

    for (int i = static_cast<int>(banksia::BBIdx::black); i < static_cast<int>(banksia::BBIdx::max); i++) {
        v->bitboardVec[i] <<= 8;

        // Pawn can't go to the top line
        if (i == static_cast<int>(banksia::BBIdx::pawns)
            && (v->bitboardVec[i] & banksia::ChessBoard::bb_edge_top)) {
            return false;
        }
    }

    v->bitboardVec[static_cast<int>(banksia::BBIdx::blackkingsquare)] += 8;
    v->bitboardVec[static_cast<int>(banksia::BBIdx::whitekingsquare)] += 8;

    return true;
}

bool ChessBoard::pattern_shift_down(ChessBitboard* v)
{
    assert(v && !v->isEmpty());
    auto bb = v->bitboardVec[static_cast<int>(banksia::BBIdx::black)]
              | v->bitboardVec[static_cast<int>(banksia::BBIdx::white)];
    assert(bb);

    if (bb & banksia::ChessBoard::bb_edge_bottom) return false;

    for (int i = static_cast<int>(banksia::BBIdx::black); i < static_cast<int>(banksia::BBIdx::max); i++) {
        v->bitboardVec[i] >>= 8;

        // Pawn can't go to the bottom line
        if (i == static_cast<int>(banksia::BBIdx::pawns)
            && (v->bitboardVec[i] & banksia::ChessBoard::bb_edge_bottom)) {
            return false;
        }
    }

    v->bitboardVec[static_cast<int>(banksia::BBIdx::blackkingsquare)] -= 8;
    v->bitboardVec[static_cast<int>(banksia::BBIdx::whitekingsquare)] -= 8;

    return true;
}

bool ChessBoard::pattern_shift_left(ChessBitboard* v)
{
    assert(v && !v->isEmpty());
    auto bb = v->bitboardVec[static_cast<int>(banksia::BBIdx::black)]
              | v->bitboardVec[static_cast<int>(banksia::BBIdx::white)];
    assert(bb);

    if (bb & banksia::ChessBoard::bb_edge_left) return false;

    for (int i = static_cast<int>(banksia::BBIdx::black); i < static_cast<int>(banksia::BBIdx::max); i++) {
        v->bitboardVec[i] >>= 1;
    }

    v->bitboardVec[static_cast<int>(banksia::BBIdx::blackkingsquare)]--;
    v->bitboardVec[static_cast<int>(banksia::BBIdx::whitekingsquare)]--;
    return true;
}

bool ChessBoard::pattern_shift_right(ChessBitboard* v)
{
    assert(v && !v->isEmpty());
    auto bb = v->bitboardVec[static_cast<int>(banksia::BBIdx::black)]
              | v->bitboardVec[static_cast<int>(banksia::BBIdx::white)];
    assert(bb);

    if (bb & banksia::ChessBoard::bb_edge_right) return false;

    for (int i = static_cast<int>(banksia::BBIdx::black); i < static_cast<int>(banksia::BBIdx::max); i++) {
        v->bitboardVec[i] <<= 1;
    }

    v->bitboardVec[static_cast<int>(banksia::BBIdx::blackkingsquare)]++;
    v->bitboardVec[static_cast<int>(banksia::BBIdx::whitekingsquare)]++;
    return true;
}
