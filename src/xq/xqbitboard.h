/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef XQBITBOARD_H
#define XQBITBOARD_H

#include <vector>

#include "base/bitboardbase.h"


namespace banksia {

    enum class XQBBIdx {
        hash,
        blackkingsquare,
        whitekingsquare,
        black,
        white,
        kings,
        advisors,
        elephants,
        rooks,
        cannons,
        knights,
        pawns,
        prop,
        max
    };

    class XqBB : public BB
    {
    public:
        XqBB() { }
        XqBB(uint64_t u0, uint64_t u1)
        {
            bits[0] = u0;
            bits[1] = u1;
        }

        virtual std::string toString() const override;
        virtual void setBit(int pos) override;
        virtual bool isZero() const override;

        XqBB operator~();
        XqBB operator|(const XqBB& other) const;
        void operator|=(const XqBB& other);
        XqBB operator&(const XqBB& other) const;
        void operator&=(const XqBB& other);

        int _popCount() const;
        uint64_t getLowUInt64() const;

        uint64_t bits[2] = { 0ULL, 0ULL };
    };

    class XqBitboard : public BitboardBase
    {
    public:
        XqBitboard();

        virtual uint64_t getHash() const override;
        virtual bool isEmpty() const override;
        virtual int size() const override;
        virtual int getPieceCount(const std::string&, BB*) const override;

        virtual bool evaluate_pattern(const BitboardBase* bbSuperVec, PatternOperand operand,
                                      int tolerance) const override;

        std::vector<XqBB> bitboardVec;
    };

} // namespace banksia

#endif // XQBITBOARD_H
