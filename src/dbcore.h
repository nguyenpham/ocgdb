/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef DBCORE_H
#define DBCORE_H

#include <stdio.h>
#include <unordered_map>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/threadpool/thread_pool.hpp"

#include "records.h"
#include "report.h"

namespace ocgdb {


class DbCore
{
public:
    DbCore();
    virtual ~DbCore();

    virtual void run(const ocgdb::ParaRecord&);
    
protected:
    virtual void runTask() = 0;

    void createPool();
    virtual void printStats() const;
    static std::chrono::steady_clock::time_point getNow();

protected:
    bslib::ChessVariant chessVariant = bslib::ChessVariant::standard;

    ParaRecord paraRecord;
    SearchField searchField;

    SQLite::Database* mDb = nullptr;

    mutable std::mutex threadMapMutex;
    mutable std::mutex printMutex;
    thread_pool* pool = nullptr;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;
    
    IDInteger gameCnt, eventCnt, playerCnt, siteCnt, commentCnt;

    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t blockCnt, processedPgnSz, errCnt, succCount;
};

} // namespace ocdb

#endif /* DBCORE_H */
