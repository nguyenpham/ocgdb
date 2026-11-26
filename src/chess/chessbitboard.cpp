/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "chessbitboard.h"
#include "chess/chess.h"

using namespace banksia;

int popCount(uint64_t x);

std::string ChessBB::toString() const
{
    return banksia::ChessBoard::bitboard2string(bits);
}

void ChessBB::setBit(int pos)
{
    assert(pos >= 0 && pos < 64);
    bits |= banksia::ChessBoard::_posToBitboard[pos];
}

bool ChessBB::isZero() const
{
    return bits == 0ULL;
}

/////////////////////////////////
ChessBitboard::ChessBitboard() { }

uint64_t ChessBitboard::getHash() const
{
    auto hash = bitboardVec[static_cast<int>(banksia::BBIdx::hash)];
    return hash;
}

bool ChessBitboard::isEmpty() const
{
    return bitboardVec.empty();
}


int ChessBitboard::size() const
{
    return static_cast<int>(bitboardVec.size());
}


int ChessBitboard::getPieceCount(const std::string& string, BB* bitSet) const
{
    uint64_t bb;
    switch (string.at(0)) {
        case 'w':
            assert(string == "white");
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)];
            break;

        case 'K':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::kings)];
            break;

        case 'Q':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::queens)];
            break;
        case 'R':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::rooks)];
            break;

        case 'B':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::bishops)];
            break;
        case 'N':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::knights)];
            break;
        case 'P':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::pawns)];
            break;

        case 'k':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::kings)];
            break;

        case 'q':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::queens)];
            break;

        case 'r':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::rooks)];
            break;

        case 'b':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)];
            if (string == "b") {
                bb &= bitboardVec[static_cast<int>(banksia::BBIdx::bishops)];
            } else {
                assert(string == "black");
            }
            break;
        case 'n':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::knights)];
            break;
        case 'p':
            bb = bitboardVec[static_cast<int>(banksia::BBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::BBIdx::pawns)];
            break;

        default:
            return 0;
    }

    if (bitSet) {
        auto chessBB = static_cast<ChessBB*>(bitSet);
        bb &= chessBB->bits;
    }

    return popCount(bb);
}


bool ChessBitboard::evaluate_pattern(const BitboardBase* _bbSuperVec, PatternOperand operand,
                                     int tolerance) const
{
    auto bbSuperVec = static_cast<const ChessBitboard*>(_bbSuperVec);

    auto blackSb = bitboardVec[static_cast<int>(banksia::BBIdx::black)],
         whiteSb = bitboardVec[static_cast<int>(banksia::BBIdx::white)];
    auto blackSp = bbSuperVec->bitboardVec[static_cast<int>(banksia::BBIdx::black)],
         whiteSp = bbSuperVec->bitboardVec[static_cast<int>(banksia::BBIdx::white)];

    uint64_t blackDif, whiteDif;

    if (operand == PatternOperand::equal) {
        blackDif = (blackSb | blackSp) & ~(blackSb & blackSp);
        whiteDif = (whiteSb | whiteSp) & ~(whiteSb & whiteSp);
    } else {
        blackDif = blackSb & ~(blackSb & blackSp);
        whiteDif = whiteSb & ~(whiteSb & whiteSp);
    }

    auto cnt = popCount(blackDif | whiteDif);

    if (cnt > tolerance) return false;

    // check further, each position
    uint64_t d = 0;
    for (auto idx = static_cast<int>(banksia::BBIdx::kings); idx < static_cast<int>(banksia::BBIdx::max);
         ++idx) {
        auto sb = bitboardVec[idx], sp = bbSuperVec->bitboardVec[idx];

        uint64_t dif;

        if (operand == PatternOperand::equal) {
            dif = (sb | sp) & ~(sb & sp);
        } else {
            dif = sb & ~(sb & sp);
        }

        d |= dif;
    }

    return popCount(d) <= tolerance;
}
