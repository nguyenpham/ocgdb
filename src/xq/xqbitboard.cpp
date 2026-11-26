/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <assert.h>

#include "xqbitboard.h"

using namespace banksia;

int popCount(uint64_t x);

std::string XqBB::toString() const
{
    return ""; // banksia::ChessBoard::bitboard2string(bits);
}

void XqBB::setBit(int pos)
{
    assert(pos >= 0 && pos < 90);
    if (pos < 64) {
        bits[0] |= 1ULL << pos;
    } else {
        bits[1] |= 1ULL << (pos - 64);
    }
}

bool XqBB::isZero() const
{
    return (bits[0] | bits[1]) == 0ULL;
}

int XqBB::_popCount() const
{
    return popCount(bits[0]) + popCount(bits[1]);
}

uint64_t XqBB::getLowUInt64() const
{
    return bits[0];
}

XqBB XqBB::operator~()
{
    return XqBB(~bits[0], ~bits[1]);
}

XqBB XqBB::operator|(const XqBB& other) const
{
    return XqBB(bits[0] | other.bits[0], bits[1] | other.bits[1]);
}

void XqBB::operator|=(const XqBB& other)
{
    bits[0] |= other.bits[0];
    bits[1] |= other.bits[1];
}

XqBB XqBB::operator&(const XqBB& other) const
{
    return XqBB(bits[0] & other.bits[0], bits[1] & other.bits[1]);
}

void XqBB::operator&=(const XqBB& other)
{
    bits[0] &= other.bits[0];
    bits[1] &= other.bits[1];
}


/////////////////////////////////
XqBitboard::XqBitboard() { }


uint64_t XqBitboard::getHash() const
{
    auto bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::hash)];
    return bb.getLowUInt64();
}

bool XqBitboard::isEmpty() const
{
    return bitboardVec.empty();
}


int XqBitboard::size() const
{
    return static_cast<int>(bitboardVec.size());
}


int XqBitboard::getPieceCount(const std::string& string, BB* bitSet) const
{
    XqBB bb;
    switch (string.at(0)) {
        case 'w':
            assert(string == "white");
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)];
            break;
        case 'b':
            assert(string == "black");
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)];
            break;

        case 'K':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::kings)];
            break;

        case 'A':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::advisors)];
            break;
        case 'E':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::elephants)];
            break;
        case 'R':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::rooks)];
            break;

        case 'C':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::cannons)];
            break;
        case 'H':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::knights)];
            break;
        case 'P':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::pawns)];
            break;

        case 'k':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::kings)];
            break;

        case 'a':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::advisors)];
            break;

        case 'e':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::elephants)];
            break;
        case 'r':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::rooks)];
            break;

        case 'c':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::cannons)];
            break;
        case 'h':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::knights)];
            break;
        case 'p':
            bb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)]
                 & bitboardVec[static_cast<int>(banksia::XQBBIdx::pawns)];
            break;

        default:
            return 0;
    }

    if (bitSet) {
        auto chessBB = static_cast<XqBB*>(bitSet);
        bb &= *chessBB;
    }

    return bb._popCount();
}


bool XqBitboard::evaluate_pattern(const BitboardBase* _bbSuperVec, PatternOperand operand,
                                  int tolerance) const
{
    auto bbSuperVec = static_cast<const XqBitboard*>(_bbSuperVec);

    auto blackSb = bitboardVec[static_cast<int>(banksia::XQBBIdx::black)],
         whiteSb = bitboardVec[static_cast<int>(banksia::XQBBIdx::white)];
    auto blackSp = bbSuperVec->bitboardVec[static_cast<int>(banksia::XQBBIdx::black)],
         whiteSp = bbSuperVec->bitboardVec[static_cast<int>(banksia::XQBBIdx::white)];

    XqBB blackDif, whiteDif;

    if (operand == PatternOperand::equal) {
        blackDif = (blackSb | blackSp) & ~(blackSb & blackSp);
        whiteDif = (whiteSb | whiteSp) & ~(whiteSb & whiteSp);
    } else {
        blackDif = blackSb & ~(blackSb & blackSp);
        whiteDif = whiteSb & ~(whiteSb & whiteSp);
    }

    auto cnt = (blackDif | whiteDif)._popCount();

    if (cnt > tolerance) return false;

    // check further, each position
    XqBB d;
    for (auto idx = static_cast<int>(banksia::XQBBIdx::kings); idx < static_cast<int>(banksia::XQBBIdx::max);
         ++idx) {
        auto sb = bitboardVec[idx], sp = bbSuperVec->bitboardVec[idx];

        XqBB dif;

        if (operand == PatternOperand::equal) {
            dif = (sb | sp) & ~(sb & sp);
        } else {
            dif = sb & ~(sb & sp);
        }

        d |= dif;
    }

    return d._popCount() <= tolerance;
}
