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


namespace ocgdb {

// Current limit is about 4 billion, we can change later by changing this define
#define IDInteger uint32_t

enum class ColumnMovesMode
{
    none,
    moves, moves1, moves2,
    moves_moves1, moves_moves2
};

enum class Task
{
    createSQLdatabase,
    query,
    bench
};

enum class Option
{
    comment_discard,
    site_discard,
    accept_newTags,
    player_limit_elo,
    player_discard_no_elo,

    query_stop_at_first_result,
    query_print_all,
    query_print_fen,
    query_print_pgn,
};

class ParaRecord
{
public:
    std::vector<std::string> pgnPaths;
    std::string dbPath;

    std::vector<std::string> queries;
    std::set<Option> optionSet;

    Task task = Task::createSQLdatabase;
    int cpuNumber = -1, lowElo = 0;
    int64_t gameNumberLimit = 0xffffffffffffULL; // stop when the number of games reached that limit
    
    ColumnMovesMode columnMovesMode = ColumnMovesMode::moves;
    
    mutable std::string errorString;
    
    std::string getErrorString() const {
        return errorString;
    }
    std::string toString() const;
    bool isValid() const;
};

class ThreadRecord
{
public:
    ~ThreadRecord();
    void init(SQLite::Database* mDb);

    bool createInsertGameStatement(SQLite::Database* mDb, const std::unordered_map<std::string, int>&);
    
public:
    int64_t errCnt = 0, gameCnt = 0, hdpLen = 0;
    int insertGameStatementIdxSz = -1;

    bslib::BoardCore *board = nullptr;
    int8_t* buf = nullptr;
    SQLite::Statement *insertGameStatement = nullptr;
    SQLite::Statement *insertCommentStatement = nullptr;
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

    void runTask(const ParaRecord&);
    
public:
    bool addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static bslib::BoardCore* createBoard(bslib::ChessVariant variant);

    std::set<IDInteger> gameIdSet;

    void parsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText, const std::vector<int8_t>& vec);

    enum class SearchField
    {
        none,
        moves,          // text only
        moves1,         // 1.5 byte per move
        moves2,         // 2 bytes per move
    };


private:
    void convertPgn2Sql(const ParaRecord&);
    void bench(const std::string& path, int cpu, const std::set<Option>& optionSet);
    void query(const std::string& dbPath, int cpu, const std::vector<std::string>& queries, const std::set<Option>& optionSet);

    void searchPosition(SQLite::Database& db, const std::string& query);
    
    SQLite::Database* createDb(const std::string& path);
    static std::string encodeString(const std::string& name);

    void setDatabasePath(const std::string& path);
    SQLite::Database* openDbToWrite();

    uint64_t processPgnFile(const std::string& path);

    int getEventNameId(char* name);
    int getSiteNameId(char* name);
    int getPlayerNameId(char* name, int elo);

    IDInteger getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap);

    void printStats() const;

    void processDataBlock(char* buffer, long sz, bool);
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);

    void updateBoard(bslib::BoardCore*, const std::vector<uint64_t>& bbvec);

    void queryGameData(SQLite::Database& db, int gameIdx);
    void threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static int standardizeFEN(char *fenBuf);

    int addNewField(const char* fieldName);

private:
    SearchField searchField;
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;

    void threadParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText, const std::vector<int8_t>&);

    std::function<bool(int64_t gameId, const std::vector<uint64_t>&, const bslib::BoardCore*)> checkToStop = nullptr;

    std::unordered_map<std::string, IDInteger> playerIdMap, eventIdMap, siteIdMap;

    bslib::ChessVariant chessVariant = bslib::ChessVariant::standard;

    std::string dbPath;
    SQLite::Database* mDb = nullptr;

    /// Prepared statements
    SQLite::Statement *playerInsertStatement = nullptr;
    SQLite::Statement *eventInsertStatement = nullptr;
    SQLite::Statement *siteInsertStatement = nullptr;

    SQLite::Statement *benchStatement = nullptr;

    thread_pool* pool = nullptr;
    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    std::unordered_map<std::string, int> fieldOrderMap;
    mutable std::mutex parsingMutex, tagFieldMutex;
    std::set<std::string> extraFieldSet;

    int tagIdx_Moves, tagIdx_MovesBlob, insertGameStatementIdxSz;
    IDInteger gameCnt, eventCnt, playerCnt, siteCnt;

    bool createoption_site_discard = true;
    bool createoption_comment_discard = true;
    int  createoption_EncodeMoveSize = 1;
    bool createoption_KeepMovesField = false;
    bool createoption_AcceptNewField = false;
    int64_t createoption_gameNumberLimit;
    uint32_t createoption_lowElo;

    bool createoption_elo_limit = true;
    bool createoption_elo_discard_no_elo = true;
    
    enum {
        query_flag_stop_at_first_result     = 1 << 0,
        query_flag_print_all                = 1 << 1,
        query_flag_print_fen                = 1 << 2,
        query_flag_print_pgn                = 1 << 3,
    };
    int query_flag;

    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t hashHit;
    int64_t blockCnt, processedPgnSz, errCnt, posCnt, succCount;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
