/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCDB_BUILDER_H
#define OCDB_BUILDER_H

#include <vector>
#include <unordered_map>
#include <fstream>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/threadpool/thread_pool.hpp"

#include "board/types.h"
#include "board/base.h"


#define _ONE_HASH_TABLE_

namespace ocgdb {


//class HashData
//{
//public:
//    std::vector<int> gameIdVec;
//};

class ThreadRecord
{
public:
    ~ThreadRecord();
    void init(SQLite::Database* mDb);

public:
    int64_t errCnt = 0, gameCnt = 0, hdpLen = 0;
    
    bslib::BoardCore *board = nullptr;
    int16_t* buf = nullptr;
    SQLite::Statement *insertGameStatement = nullptr;
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

    void convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath, int cpu);

    void bench(const std::string& path, int cpu);

    bool addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static bslib::BoardCore* createBoard(bslib::ChessVariant variant);

    std::set<int> gameIdSet;

    void parsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText);

private:
    void searchPosition(SQLite::Database& db, const std::string& query);

    void searchPositions(SQLite::Database& db, std::function<bool(int64_t gameId, const std::vector<uint64_t>&, const bslib::BoardCore*)> checkToStop);
    
    static SQLite::Database* createDb(const std::string& path);
    static std::string encodeString(const std::string& name);

    void setDatabasePath(const std::string& path);
    SQLite::Database* openDbToWrite();

    uint64_t processPgnFile(const std::string& path);

    int getEventNameId(char* name);
    int getSiteNameId(char* name);
    int getPlayerNameId(char* name, int elo);

    int getNameId(char* name, int elo, int& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, int>& idMap);

    void printStats() const;

    void processDataBlock(char* buffer, long sz, bool);
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);

    void updateBoard(bslib::BoardCore*, const std::vector<uint64_t>& bbvec);

    void queryGameData(SQLite::Database& db, int gameIdx);
    void threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static int standardizeFEN(char *fenBuf);

private:
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;

//    void threadParsePGNGame(int64_t gameID, const char* fenText, const char* moveText);
    void threadParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText);

    std::function<bool(int64_t gameId, const std::vector<uint64_t>&, const bslib::BoardCore*)> checkToStop = nullptr;

    std::unordered_map<std::string, int> playerIdMap, eventIdMap, siteIdMap;

    bslib::ChessVariant chessVariant = bslib::ChessVariant::standard;

    std::string dbPath;
    SQLite::Database* mDb = nullptr;

    /// Prepared statements
    SQLite::Statement *playerInsertStatement = nullptr;
    SQLite::Statement *eventInsertStatement = nullptr;
    SQLite::Statement *siteInsertStatement = nullptr;

//    SQLite::Statement *updateHashStatement = nullptr; // *selectHashStatement = nullptr,

//    SQLite::Statement *insertHashStatement_one = nullptr, *insertHashStatement_blob = nullptr;

//    SQLite::Statement *bitboardStatement = nullptr;
    SQLite::Statement *benchStatement = nullptr;

    thread_pool* pool = nullptr;
    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    mutable std::mutex parsingMutex;
    
    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int gameCnt, eventCnt, playerCnt, siteCnt, hashHit;
    int64_t errCnt, posCnt, succCount; // hashCnt,
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
