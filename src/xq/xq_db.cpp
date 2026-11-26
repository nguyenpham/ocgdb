/*
 This file is part of Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 Distributed under the MIT License (MIT) (See accompanying file LICENSE
 or copy at http://opensource.org/licenses/MIT)
 */

/// What the xiangqi board needs for game databases and PQL, and Banksia's core does not have:
/// the bitboards PQL evaluates, the encodings of the binary move fields (Moves2, Moves1) and the
/// key for finding duplicates. Taken from Banksia GUI's core (banksiagui banksia/xq/xq.cpp and
/// xq_hash.cpp); kept in a file of their own so that xq.cpp stays as close to Banksia's as possible.

#include <cassert>
#include <iostream>
#include <unordered_map>

#include "xq.h"

using namespace banksia;

BitboardBase* XqBoard::posToBitboards() const
{
    auto bb = new XqBitboard;

    for (auto i = 0; i < static_cast<int>(XQBBIdx::max); i++) {
        bb->bitboardVec.push_back(XqBB());
    }
    bb->bitboardVec[static_cast<int>(XQBBIdx::hash)].bits[0] = hashKey;

    assert(bb->bitboardVec.size() == static_cast<int>(XQBBIdx::max));


    for (auto i = 0; i < 90; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            continue;
        }

        auto sd = static_cast<int>(piece.side);
        assert(sd == 0 || sd == 1);

        bb->bitboardVec[static_cast<int>(XQBBIdx::kings) + piece.type - 1].setBit(i);
        bb->bitboardVec[static_cast<int>(XQBBIdx::black) + sd].setBit(i);

        if (piece.type == static_cast<int>(PieceTypeStd::king)) {
            bb->bitboardVec[static_cast<int>(XQBBIdx::blackkingsquare) + sd].bits[0] = i;
        }
    }

    return bb;
}


uint16_t XqBoard::encode2Bytes(const Hist& hist) const
{
    return hist.move.from | hist.move.dest << 7;
}

Move XqBoard::decode2Bytes(uint16_t d) const
{
    return Move(d & 0x7f, (d >> 7) & 0x7f);
}

static const std::vector<int> kingPawnMoveIndexVec { -1, -9, 1, 9 };
static const std::vector<int> advisorMoveIndexVec { -10, -8, 10, 8 };
static const std::vector<int> elephantMoveIndexVec { -20, -16, 20, 16 };
static const std::vector<int> horseMoveIndexVec { -11, -19, -17, -7, 11, 19, 17, 7 };

/// Move offset -> index of the move in Moves1. Built complete inside the static's initializer,
/// which C++ runs once and thread-safely: the encoder runs on every thread of a build, and a map
/// filled on first use was read by one thread while another was still inserting.
static const std::unordered_map<int, int>& moveIndexMap()
{
    static const std::unordered_map<int, int> m = [] {
        std::unordered_map<int, int> v;
        for (auto i = 0; i < 4; i++) {
            v[kingPawnMoveIndexVec[i]] = i;
            v[advisorMoveIndexVec[i]] = i;
            v[elephantMoveIndexVec[i]] = i;
            v[horseMoveIndexVec[i]] = i;
        }
        for (auto i = 4; i < 8; i++) {
            v[horseMoveIndexVec[i]] = i;
        }
        return v;
    }();
    return m;
}

std::pair<uint16_t, int> XqBoard::encode1Byte(MoveFull move)
{

    uint16_t t = move.piece.idx;
    assert(t >= 0 && t < 16);

    std::pair<uint16_t, int> pair;

    auto type = static_cast<PieceTypeXq>(move.piece.type);
    if (type == PieceTypeXq::rook || type == PieceTypeXq::cannon) {
        t |= static_cast<int16_t>(move.dest << 4);
        pair.second = 2;

        assert((t >> 4) < 90);
    } else {
        auto d = move.dest - move.from;
        auto& indexMap = moveIndexMap();
        auto it = indexMap.find(d);
        if (it != indexMap.end()) {
            t |= it->second << 4;
        } else {
            assert(false);
        }

        pair.second = 1;
    }

    pair.first = t;
    assert(pair.second == 1 || pair.second == 2);
    return pair;
}

std::pair<Move, int> XqBoard::decode1Byte(const int8_t* d)
{

    uint8_t dd = *d;
    auto idx = static_cast<int>(dd & 0xf);

    std::pair<Move, int> pair;
    int from;

    for (from = 0; from < 90; ++from) {
        if (pieces[from].idx == idx && pieces[from].side == side) {
            break;
        }
    }

    if (from >= 90) {
        std::cout << "Error on decode1Byte. Stop!" << std::endl;
        return pair; // something wrong
    }

    auto n = 1;
    Move m;
    m.promotion = 0;

    auto piece = pieces[from];
    m.from = from;

    auto type = static_cast<PieceTypeXq>(piece.type);
    if (type == PieceTypeXq::rook || type == PieceTypeXq::cannon) {
        auto q = reinterpret_cast<const uint16_t*>(d);
        auto qq = *q;
        assert((qq & 0xff) == dd);
        m.dest = qq >> 4;
        n = 2;
        assert(BoardCore::isValid(m));
    } else {
        auto k = (dd >> 4) & 0xf;

        switch (type) {
            case PieceTypeXq::king:
            case PieceTypeXq::pawn: {
                m.dest = from + kingPawnMoveIndexVec.at(k);
                break;
            }
            case PieceTypeXq::advisor: {
                m.dest = from + advisorMoveIndexVec.at(k);
                break;
            }
            case PieceTypeXq::elephant: {
                m.dest = from + elephantMoveIndexVec.at(k);
                break;
            }
            case PieceTypeXq::horse: {
                m.dest = from + horseMoveIndexVec.at(k);
                break;
            }
            default:
                assert(false);
                break;
        }
        assert(BoardCore::isValid(m));
    }

    pair.first = m;
    pair.second = n;
    assert(BoardCore::isValid(m));
    return pair;
}


/// the key of the position, mixed with the length of the game (the main line)
uint64_t XqBoard::getHashKeyForCheckingDuplicates(int h) const
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

    /// Mix the length in with splitmix64: it takes any number of plies and needs no table.
    auto mix = static_cast<u64>(t) + 0x9E3779B97F4A7C15ULL;
    mix = (mix ^ (mix >> 30)) * 0xBF58476D1CE4E5B9ULL;
    mix = (mix ^ (mix >> 27)) * 0x94D049BB133111EBULL;
    mix ^= mix >> 31;

    return hk ^ mix;
}
