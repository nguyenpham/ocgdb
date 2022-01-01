/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCDB_BUILDER_H
#define OCDB_BUILDER_H

#include <vector>
#include <unordered_map>
#include <fstream>

#include "3rdparty/threadpool/thread_pool.hpp"

#include "board/types.h"
#include "board/base.h"


#define _ONE_HASH_TABLE_

namespace ocgdb {

class ThreadRecord
{
public:
    ~ThreadRecord();
    void init();

public:
    int64_t errCnt;
    bslib::BoardCore *board = nullptr;
};


class GameRecord {
public:
    int plyCount = 0, whiteElo = 0, blackElo = 0, round = -1;
    bslib::ResultType resultType = bslib::ResultType::noresult;
    const char *eventName, *whiteName, *blackName;
    const char *siteName = nullptr, *timer = nullptr, *dateString = nullptr, *eco = nullptr;
    const char *fen = nullptr, *moveString;
};

class Builder
{
public:
    Builder();
    virtual ~Builder();

    void bench(const std::string& path, int cpu);
    bool addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static bslib::BoardCore* createBoard(bslib::ChessVariant variant);

private:
    uint64_t processPgnFile(const std::string& path);

    void processDataBlock(char* buffer, long sz, bool);
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);

    void updateBoard(bslib::BoardCore*, const std::vector<uint64_t>& bbvec);

private:
    void threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

private:
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;

    std::function<bool(int64_t gameId, const std::vector<uint64_t>&, const bslib::BoardCore*)> checkToStop = nullptr;
    
    bslib::ChessVariant chessVariant = bslib::ChessVariant::standard;

    thread_pool* pool = nullptr;
    mutable std::mutex gameMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t gameCnt, succCount;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
