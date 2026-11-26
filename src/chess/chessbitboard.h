/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef CHESSBITBOARD_H
#define CHESSBITBOARD_H

#include <vector>

#include "base/bitboardbase.h"

namespace banksia {

    enum class BBIdx {
        hash,
        blackkingsquare,
        whitekingsquare,
        black,
        white,
        kings,
        queens,
        rooks,
        bishops,
        knights,
        pawns,
        prop,
        max
    };

    class ChessBB : public BB
    {
    public:
        virtual std::string toString() const override;
        virtual void setBit(int pos) override;
        virtual bool isZero() const override;

        uint64_t bits = 0ULL;
    };

    class ChessBitboard : public BitboardBase
    {
    public:
        ChessBitboard();

        virtual uint64_t getHash() const override;
        virtual bool isEmpty() const override;
        virtual int size() const override;
        virtual int getPieceCount(const std::string&, BB*) const override;

        virtual bool evaluate_pattern(const BitboardBase* bbSuperVec, PatternOperand operand,
                                      int tolerance) const override;

        std::vector<uint64_t> bitboardVec;
    };

} // namespace banksia

#endif // CHESSBITBOARD_H
