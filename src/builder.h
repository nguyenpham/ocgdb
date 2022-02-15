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

#include "sqllib.h"


namespace ocgdb {


class Builder
{
public:
    Builder();
    virtual ~Builder();

    void runTask(const ParaRecord&);
    

public:
    bool addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);
    bool queryGame(const bslib::PgnRecord&);

    std::set<IDInteger> gameIdSet;

    void searchPosition(const bslib::PgnRecord& record,
                        const std::vector<int8_t>& moveVec);

    static void printGamePGNByIDs(SQLite::Database& db, const std::vector<int>& gameIDVec, SearchField);
    
    static void printGamePGNByIDs(QueryGameRecord&, const std::vector<int>&);

    void checkDuplicates(const bslib::PgnRecord&, const std::vector<int8_t>& moveVec);

private:
    void convertPgn2Sql(const ParaRecord&);
    
    
private:
    void convertSql2Pgn(const ParaRecord&);
    void threadConvertSql2Pgn(const bslib::PgnRecord& record,
                                       const std::vector<int8_t>& moveVec, int flag);
    
public:
    void convertSql2PgnByAThread(const bslib::PgnRecord& record,
                                          const std::vector<int8_t>& moveVec, int flag);
private:
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

    void queryInfo();

    int addNewField(const char* fieldName);

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

    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, commentMutex, threadMapMutex, dupHashKeyMutex, printMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    std::unordered_map<std::string, int> fieldOrderMap;
    mutable std::mutex parsingMutex, tagFieldMutex;
    std::set<std::string> extraFieldSet;
    
    std::unordered_map<int64_t, std::vector<int>> hashGameIDMap;

    int tagIdx_Moves, tagIdx_MovesBlob, insertGameStatementIdxSz;
    IDInteger gameCnt, eventCnt, playerCnt, siteCnt, commentCnt;

    ParaRecord paraRecord;

    mutable std::mutex ofsMutex;
    std::ofstream ofs;
    
    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t blockCnt, processedPgnSz, errCnt, succCount;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
