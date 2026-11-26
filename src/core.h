/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_CORE_H
#define OCGDB_CORE_H

#include <atomic>
#include <chrono>
#include <stdio.h>
#include <unordered_map>

#include "3rdparty/threadpool/thread_pool.hpp"

#include "records.h"
#include "report.h"

namespace ocgdb {

    /// The base of every task: the thread pool, a ThreadRecord per thread, counters and timing.
    class Core
    {
    public:
        Core();
        virtual ~Core();

        /// runs the task of the command line; false when it failed
        virtual bool run(const ParaRecord&);

        ThreadRecord* getThreadRecord();
        virtual void resetCnts();

    protected:
        virtual bool runTask() = 0;
        /// whether the task works on the threads of a pool
        virtual bool needsPool() const { return true; }

        void createPool();
        virtual void printStats(bool completed) const;
        /// printStats(false), at most every 5 seconds
        void printProgress();
        static std::chrono::steady_clock::time_point getNow();
        int64_t elapsedMs() const;

    public:
        std::atomic<bool> working { true };
        banksia::ChessVariant variant = banksia::ChessVariant::none;

    protected:
        ParaRecord paraRecord;

        mutable std::mutex threadMapMutex;
        mutable std::mutex printMutex;

        /// the task's threads; null until createPool()
        thread_pool* pool = nullptr;
        std::unordered_map<std::thread::id, ThreadRecord> threadMap;

        std::atomic<int64_t> gameCnt { 0 }, processedCnt { 0 }, errCnt { 0 }, succCount { 0 };
        IDInteger eventCnt = 0, playerCnt = 0, siteCnt = 0;
        std::atomic<int64_t> commentCnt { 0 };

        /// For stats
        std::chrono::steady_clock::time_point startTime, lastProgressTime;
        std::mutex progressMutex;
        int64_t blockCnt = 0, processedPgnSz = 0, fileSize = 0;
    };

} // namespace ocgdb

#endif /* OCGDB_CORE_H */
