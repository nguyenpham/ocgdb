/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_PGNREAD_H
#define OCGDB_PGNREAD_H

#include <memory>

#include "core.h"

namespace ocgdb {

    /// Reads PGN files in blocks of 8 MB and hands each game (its tags and its move text) to
    /// processPGNGameWithAThread() on a thread of the pool. The tags and moves point into the
    /// block, which is kept until every game of it has been processed.
    class PGNRead : public virtual Core
    {
    public:
        virtual ~PGNRead();

        virtual void processPGNGame(const std::unordered_map<const char*, char*>&, const char*);
        virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<const char*, char*>&,
                                               const char*);
        virtual void processPGNGameByAThread(const std::unordered_map<const char*, char*>&, const char*,
                                             int64_t gameIndex);

        /// the place of the game being processed in its file (1, 2, ...), on the thread processing it
        static thread_local int64_t currentGameIndex;

    protected:
        uint64_t processPgnFiles(const std::vector<std::string>& pathList);
        virtual uint64_t processPgnFile(const std::string& path);
        virtual void processDataBlock(char* buffer, long sz, bool connectBlock);

        /// End each block at its last newline (PGN text): a block that ended just after the '['
        /// of a tag line lost that game. WXF text keeps its own carrying (wxfread.cpp).
        virtual bool cutBlocksAtNewline() const { return true; }

        /// WXF's reader (wxfread.cpp) carries a cut game with these
        void processHalfBegin(char* buffer, long len);

        /// true when gameNumberLimit (-limit) has been reached
        bool reachedGameLimit() const;

        /// games handed to the pool from the current file, in the order of the file
        int64_t pgnGameIndex = 0;
        /// games handed to the pool in this run (all files), for -limit
        int64_t pushedGameCnt = 0;

    private:
        void processPending();

        const size_t blockSz_Pgn = 8 * 1024 * 1024;
        const int halfBlockSz_Pgn = 16 * 1024;

        char* halfBuf_Pgn = nullptr;
        long halfBufSz_Pgn = 0;

        /// PGN: the game a block boundary cut in two, from its FIRST TAG to the end of the block,
        /// completed from the next block (as Banksia GUI's and OOBS's readers), so a boundary inside
        /// a header, a final block without an Event and games without an Event tag all read right.
        std::string pending_Pgn;
        bool pendingHasMoves_Pgn = false;
        int pendingState_Pgn = 0;
        static constexpr size_t maxPendingSz_Pgn = 4 * 1024 * 1024;
    };

} // namespace ocgdb

#endif /* OCGDB_PGNREAD_H */
