/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_WXFREAD_H
#define OCGDB_WXFREAD_H

#include "pgnread.h"

namespace ocgdb {

    /// Xiangqi games in WXF text (the format of the CXQ server and of many xiangqi programs:
    /// FORMAT, GAME, RED, BLACK, ... and the moves between START{ and }END), read like PGN.
    /// A file is WXF unless its name ends with .pgn (Builder sets isPgn). From Banksia GUI's
    /// ui/ocgdb/wxfread.cpp.
    class WXFRead : virtual public PGNRead
    {
    protected:
        virtual void processDataBlock(char* buffer, long sz, bool) override;
        virtual bool cutBlocksAtNewline() const override { return isPgn; }

        /// "a b2 C" -> "a=b2 C"... CXQ writes a jieqi reveal as a piece letter after the move
        static int fixJieqiWXFStringOfCXQ(char* wxfString);

    protected:
        bool isPgn = true;
    };

} // namespace ocgdb

#endif // OCGDB_WXFREAD_H
