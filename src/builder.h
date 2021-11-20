/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCDB_BUILDER_H
#define OCDB_BUILDER_H

#include <vector>
#include <unordered_map>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "board/types.h"
#include "board/base.h"

namespace ocgdb {

class GameRecord {
public:
    int plyCount = 0, whiteElo = 0, blackElo = 0, round = -1;
    ResultType resultType = ResultType::noresult;
    const char *eventName, *whiteName, *blackName; // must have
    const char *siteName = nullptr, *timer = nullptr, *dateString = nullptr, *eco = nullptr;
    const char *fen = nullptr, *moveString;
};

class Builder
{
public:
    Builder();
    virtual ~Builder();

    void convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath);

    void bench(const std::string& path);

private:
    static SQLite::Database* createDb(const std::string& path);
    static std::string encodeString(const std::string& name);
//    static BoardCore* createBoard(ChessVariant variant);

    void setDatabasePath(const std::string& path);
    SQLite::Database* openDbToWrite();

    uint64_t processPgnFile(const std::string& path);

//    bool addGame(const GameRecord& r);
//    bool addGame(const std::unordered_map<std::string, const char*>& itemMap, const char* moveText);

    bool addGame(const std::unordered_map<const char*, const char*>& itemMap, const char* moveText);
    int getEventNameId(const char* name);
    int getSiteNameId(const char* name);
    int getPlayerNameId(const char* name, int elo);

    void printStats() const;

    void processDataBlock(char* buffer, long sz, bool);
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);
    
private:
    void queryGameData(SQLite::Database& db, int gameIdx);

private:
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;


//    BoardCore* board = nullptr; /// For verifying games, count moves
    ChessVariant chessVariant = ChessVariant::standard;

    std::string dbPath;
    SQLite::Database* mDb = nullptr;

    // Prepared statements
    SQLite::Statement* insertGameStatement = nullptr;
    SQLite::Statement* playerGetIdStatement = nullptr;
    SQLite::Statement* playerInsertStatement = nullptr;

    SQLite::Statement* eventGetIdStatement = nullptr;
    SQLite::Statement* eventInsertStatement = nullptr;

    SQLite::Statement* siteGetIdStatement = nullptr;
    SQLite::Statement* siteInsertStatement = nullptr;

    SQLite::Statement* benchStatement = nullptr;

    /// For stats
    std::chrono::steady_clock::time_point startTime;
    uint64_t gameCnt, errCnt;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
