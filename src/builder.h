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
    int moveCnt = 0, whiteElo = 0, blackElo = 0;
    ResultType resultType = ResultType::noresult;
    std::string whiteName, blackName, sName, eventName, timer, dateString, eco;
    std::string fen, moveString;
};

class Builder
{
public:
    Builder();
    virtual ~Builder();

    void convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath);

    void bench(const std::string& path);

private:
    void setDatabasePath(const std::string& path);
    bool addGame(const GameRecord& r);
    uint64_t processPgnFile(const std::string& path);
    bool parseAGame(const std::vector<std::string>& lines);
    bool addGame(const std::unordered_map<std::string, std::string>& itemMap, const std::string& moveText);

    SQLite::Database* openDbToWrite();
    int getNameId(const std::string& tableName, const std::string& name);
    int getPlayerNameId(const std::string& name, int elo);
    int getEventNameId(const std::string& name);

    static bool createDb(const std::string& path);
    static std::string encodeString(const std::string& name);
    static BoardCore* createBoard(ChessVariant variant);
    void printStats() const;

    void queryGameData(SQLite::Database& db, int gameIdx);

private:
    BoardCore* board = nullptr; /// For verifying games, count moves
    std::string dbPath;
    SQLite::Database* mDb = nullptr;    ///< Database connection

    ChessVariant chessVariant = ChessVariant::standard;
    
    /// For stats
    std::chrono::steady_clock::time_point startTime;
    uint64_t gameCnt, errCnt;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
