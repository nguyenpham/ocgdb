/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BITBOARDBASE_H
#define BITBOARDBASE_H

#include <string>
#include <cstdint>

#include "base/types.h"

enum class PatternOperand { equal, lessthan, greaterthan, shift, none };

/// Population count. Declared at global scope, not inside namespace banksia,
/// because chessbitboard.cpp and xqbitboard.cpp forward-declare it that way.
/// It belongs here with the other bitboard primitives, in the pure-C++ core.
int popCount(uint64_t x);

namespace banksia {

    class BB
    {
    public:
        virtual ~BB() { }

        virtual std::string toString() const = 0;
        virtual void setBit(int pos) = 0;
        virtual bool isZero() const = 0;
    };

    class BitboardBase
    {
    public:
        BitboardBase();
        virtual ~BitboardBase();

        virtual uint64_t getHash() const = 0;
        virtual bool isEmpty() const = 0;
        virtual int size() const = 0;

        virtual int getPieceCount(const std::string&, BB*) const = 0;

        virtual bool evaluate_pattern(const BitboardBase* bbSuperVec, PatternOperand operand,
                                      int tolerance) const
                = 0;
    };

} // namespace banksia

#endif // BITBOARDBASE_H
