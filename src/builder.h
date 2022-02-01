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

const std::string VersionString = "Beta 4";

// Current limit is about 4 billion, we can change later by changing this define
#define IDInteger uint32_t

enum class Task
{
    create,
    export_,
    merge,
    query,
    bench,
    getgame,
    dup
};

enum {
    create_flag_moves                   = 1 << 0,
    create_flag_moves1                  = 1 << 1,
    create_flag_moves2                  = 1 << 2,
    create_flag_accept_new_tags         = 1 << 3,
    create_flag_discard_comments        = 1 << 4,
    create_flag_discard_sites           = 1 << 5,
    create_flag_discard_no_elo          = 1 << 6,

    query_flag_print_all                = 1 << 7,
    query_flag_print_fen                = 1 << 8,
    query_flag_print_pgn                = 1 << 9,

    dup_flag_remove                     = 1 << 10,
};

class ParaRecord
{
public:
    std::vector<std::string> pgnPaths, dbPaths;

    std::vector<std::string> queries;
    int optionFlag;

    Task task = Task::create;
    int cpuNumber = -1, limitElo = 0, limitLen = 0;
    std::vector<int> gameIDVec;
    
    int64_t gameNumberLimit = 0xffffffffffffULL; // stop when the number of games reached that limit
    int64_t resultNumberLimit = 0xffffffffffffULL; // stop when the number of results reached that limit

    mutable std::string errorString;
    
    std::string getErrorString() const {
        return errorString;
    }
    std::string toString() const;
    bool isValid() const;
    
    void setupOptions(const std::string& optionString);
};

class ThreadRecord
{
public:
    ~ThreadRecord();
    void init(SQLite::Database* mDb);

    bool createInsertGameStatement(SQLite::Database* mDb, const std::unordered_map<std::string, int>&);
    
    void deleteAllStatements();

    void resetStats();
    
public:
    int64_t errCnt = 0, gameCnt = 0, hdpLen = 0, dupCnt = 0, delCnt = 0;
    int insertGameStatementIdxSz = -1;

    bslib::BoardCore *board = nullptr;
    int8_t* buf = nullptr;
    SQLite::Statement *insertGameStatement = nullptr;
    SQLite::Statement *insertCommentStatement = nullptr;
    SQLite::Statement *removeGameStatement = nullptr;
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
    
    enum class SearchField
    {
        none,
        moves,          // text only
        moves1,         // 1.5 byte per move
        moves2,         // 2 bytes per move
    };


public:
    bool addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);
    bool queryGame(const bslib::PgnRecord&);

    static bslib::BoardCore* createBoard(bslib::ChessVariant variant);

    std::set<IDInteger> gameIdSet;

    void searchPosition(const bslib::PgnRecord& record,
                        const std::vector<int8_t>& moveVec);

    static void getGameDataByID(SQLite::Database& db, const std::vector<int> gameIDVec, SearchField);
    void checkDuplicates(const bslib::PgnRecord&, const std::vector<int8_t>& moveVec);

private:
    void convertPgn2Sql(const ParaRecord&);
    void convertSql2Pgn(const ParaRecord&);

    void mergeDatabases(const ParaRecord&);
    void findDuplicatedGames(const ParaRecord&);

    void bench(const ParaRecord& paraRecord);
    void searchPostion(const ParaRecord& paraRecord, const std::vector<std::string>& queries);
    void getGame(const ParaRecord&);

    void searchPosition(SQLite::Database* db, const std::vector<std::string>& pgnPaths, std::string query);
    
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

    void threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);
    void threadQueryGame(const bslib::PgnRecord& record);
    void threadQueryGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);

    static int standardizeFEN(char *fenBuf);

    int addNewField(const char* fieldName);
    static SearchField getMoveField(SQLite::Database* db);

    static bool queryGameData(SQLite::Statement& query, SQLite::Statement* queryComments, std::string* toPgnString, bslib::BoardCore* board, char* tmpBuf, SearchField);
    
    void threadCheckDupplication(const bslib::PgnRecord&, const std::vector<int8_t>& moveVec);
    void createPool();

private:
    SearchField searchField;
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;

    void threadSearchPosition(const bslib::PgnRecord& record,
                              const std::vector<int8_t>& moveVec);

    std::function<bool(const std::vector<uint64_t>&, const bslib::BoardCore*, const bslib::PgnRecord*)> checkToStop = nullptr;

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

    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, threadMapMutex, dupHashKeyMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    std::unordered_map<std::string, int> fieldOrderMap;
    mutable std::mutex parsingMutex, tagFieldMutex;
    std::set<std::string> extraFieldSet;
    
    std::unordered_map<int64_t, int> hashGameIDMap;
    std::set<int64_t> hashSet;

    int tagIdx_Moves, tagIdx_MovesBlob, insertGameStatementIdxSz;
    IDInteger gameCnt, eventCnt, playerCnt, siteCnt;

    ParaRecord paraRecord;

    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t blockCnt, processedPgnSz, errCnt, succCount;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
