/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef SqlLib_hpp
#define SqlLib_hpp

#include <vector>
#include <unordered_map>
#include <fstream>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/threadpool/thread_pool.hpp"

#include "board/types.h"
#include "board/base.h"


namespace ocgdb {

class QueryGameRecord;

// Developer may change this string
const std::string VersionUserDatabaseString = "0.1";

const std::string VersionString = "Beta 8";
const std::string VersionDatabaseString = "0.6";

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
    dup,
    none,
};

enum {
    TagIdx_GameID,
    TagIdx_Event, TagIdx_Site, TagIdx_Date,
    TagIdx_Round,
    TagIdx_White, TagIdx_WhiteElo, TagIdx_Black, TagIdx_BlackElo,
    TagIdx_Result, TagIdx_TimeControl, TagIdx_ECO, TagIdx_PlyCount,
    TagIdx_FEN,

    TagIdx_Max
};

extern const std::vector<std::string> knownPgnTagVec;

enum {
    create_flag_moves                   = 1 << 0,
    create_flag_moves1                  = 1 << 1,
    create_flag_moves2                  = 1 << 2,
    create_flag_accept_new_tags         = 1 << 3,
    create_flag_discard_comments        = 1 << 4,
    create_flag_discard_sites           = 1 << 5,
    create_flag_discard_no_elo          = 1 << 6,
    create_flag_discard_fen             = 1 << 7,
    create_flag_reset_eco               = 1 << 8,

    query_flag_print_all                = 1 << 10,
    query_flag_print_fen                = 1 << 11,
    query_flag_print_pgn                = 1 << 12,

    dup_flag_remove                     = 1 << 15,
    dup_flag_embededgames               = 1 << 16,
    
    lichess_flag_nobot                  = 1 << 20,
    lichess_flag_bot                    = 1 << 21,

};

class ParaRecord
{
public:
    std::vector<std::string> pgnPaths, dbPaths;
    std::string reportPath, desc;

    std::vector<std::string> queries;
    int optionFlag = 0;

    Task task = Task::none;
    int cpuNumber = -1, limitElo = 0, limitLen = 0;
    std::vector<int> gameIDVec;
    
    int64_t gameNumberLimit = 0xffffffffffffULL; // stop when the number of games reached that limit
    int64_t resultNumberLimit = 0xffffffffffffULL; // stop when the number of results reached that limit

    mutable std::string errorString;
    
    std::string getErrorString() const {
        return errorString;
    }

    static std::string toString(Task task);
    std::string toString() const;
    bool isValid() const;
    
    void setupOptions(const std::string& optionString);
};

class ThreadRecord
{
public:
    ~ThreadRecord();
    void init(SQLite::Database* mDb);

    bool createInsertGameStatement(SQLite::Database* mDb, const std::vector<std::string>&);
    
    void deleteAllStatements();

    void resetStats();
    
public:
    int64_t errCnt = 0, gameCnt = 0, hdpLen = 0, dupCnt = 0, delCnt = 0;
    int insertGameStatementIdxSz = -1;

    bslib::BoardCore *board = nullptr, *board2 = nullptr;
    int8_t* buf = nullptr;
    SQLite::Statement *insertGameStatement = nullptr;
    SQLite::Statement *insertCommentStatement = nullptr;
    SQLite::Statement *removeGameStatement = nullptr;
    SQLite::Statement *getGameStatement = nullptr;
    SQLite::Statement *queryComments = nullptr;
    
    QueryGameRecord* qgr = nullptr;
};


class GameRecord {
public:
    int plyCount = 0, whiteElo = 0, blackElo = 0, round = -1;
    bslib::ResultType resultType = bslib::ResultType::noresult;
    const char *eventName, *whiteName, *blackName;
    const char *siteName = nullptr, *timer = nullptr, *dateString = nullptr, *eco = nullptr;
    const char *fen = nullptr, *moveString;
};

enum class SearchField
{
    none,
    moves,          // text only
    moves1,         // 1.5 byte per move
    moves2,         // 2 bytes per move
};


class QueryGameRecord {
public:
    QueryGameRecord(SQLite::Database& db, SearchField searchField);

    ~QueryGameRecord() {
        if (queryGameByID) delete queryGameByID;
        if (queryComments) delete queryComments;
        if (board) delete board;
    }
    
    std::string queryAndCreatePGNByGameID(bslib::PgnRecord& record);
    
public:

    SQLite::Statement* queryGameByID = nullptr, *queryComments = nullptr;
    bslib::BoardCore* board = nullptr;
    SearchField searchField;
    
private:
    std::mutex queryMutex;
};

}

#endif /* SqlLib_hpp */
