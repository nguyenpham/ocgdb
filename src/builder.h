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


class Report
{
public:
    Report() {}
    void init(bool print, const std::string& path) {
        printConsole = print;
        if (!path.empty()) {
            ofs = bslib::Funcs::openOfstream2write(path);
            openingstream = ofs.is_open();
        }
    }
    ~Report() {
        close();
    }
    
    bool isOn() const {
        return printConsole || openingstream;
    }

    void printOut(const std::string& str) {
        if (str.empty()) return;

        if (openingstream) {
            std::lock_guard<std::mutex> dolock(ofsMutex);
            ofs << str << std::endl;
        }
        
        if (printConsole) {
            std::lock_guard<std::mutex> dolock(printMutex);
            std::cout << str << std::endl;
        }
    }
    void close() {
        if (openingstream && ofs.is_open()) {
            ofs.close();
        }
        openingstream = false;
    }

public:
    bool printConsole = true, openingstream = false;
    mutable std::mutex printMutex, ofsMutex;
    std::ofstream ofs;
};

class Builder
{
public:
    Builder();
    virtual ~Builder();

    void runTask(const ParaRecord&);
    

public:
    bool create_addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText);
    
    bool addGame(const std::string& dbPath, const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board);
    bool addGame(const std::string& dbPath, const std::string& pgnString);

    bool queryGame(const bslib::PgnRecord&);

    std::set<IDInteger> gameIdSet;

    void searchPosition(const bslib::PgnRecord& record,
                        const std::vector<int8_t>& moveVec);

    static void printGamePGNByIDs(SQLite::Database& db, const std::vector<int>& gameIDVec, SearchField);
    
    static void printGamePGNByIDs(QueryGameRecord&, const std::vector<int>&);

    void checkDuplicates(const bslib::PgnRecord&, const std::vector<int8_t>& moveVec);

private:
    void convertPgn2Sql(const ParaRecord&);
    void searchPosition_basic(const std::vector<std::string>& pgnPaths);

    
private:
    void convertSql2Pgn(const ParaRecord&);
    void threadConvertSql2Pgn(const bslib::PgnRecord& record,
                                       const std::vector<int8_t>& moveVec, int flag);
    
public:
    void convertSql2PgnByAThread(const bslib::PgnRecord& record,
                                          const std::vector<int8_t>& moveVec, int flag);
private:
    bool addGame(const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board);

    void mergeDatabases(const ParaRecord&);
    void findDuplicatedGames(const ParaRecord&);
    bool processDuplicate(ThreadRecord* t, const bslib::PgnRecord& record, int theDupID, int, uint64_t);

    void bench(ParaRecord paraRecord);
    void searchPostion(const ParaRecord& paraRecord);
    void getGame(const ParaRecord&);

    void searchPosition(SQLite::Database* db, const std::vector<std::string>& pgnPaths, std::string query);
    
    static SQLite::Database* createDb(const std::string& path, int optionFlag);
    bool createInsertStatements(SQLite::Database& mDb);

    void setDatabasePath(const std::string& path);
    SQLite::Database* openDbToWrite();

    uint64_t processPgnFile(const std::string& path);

    int create_getEventNameId(char* name);
    int create_getSiteNameId(char* name);
    int create_getPlayerNameId(char* name, int elo);

    IDInteger create_getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap);

    IDInteger getNameId(const std::string& tableName, const std::string& name, int elo = -1);
    
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
                              const std::vector<int8_t>&);

    std::function<bool(const std::vector<uint64_t>&, const bslib::BoardCore*, const bslib::PgnRecord*)> checkToStop = nullptr;
    std::function<bool(const bslib::BoardCore*, const bslib::PgnRecord*)> boardCallback = nullptr;

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
    QueryGameRecord* qgr = nullptr;

    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, commentMutex, threadMapMutex, dupHashKeyMutex, printMutex;
    std::unordered_map<std::thread::id, ThreadRecord> threadMap;

    std::unordered_map<std::string, int> fieldOrderMap;
    mutable std::mutex parsingMutex, tagFieldMutex;
    std::set<std::string> extraFieldSet;
    
    std::unordered_map<int64_t, std::vector<int>> hashGameIDMap;

    int tagIdx_Moves, tagIdx_MovesBlob, insertGameStatementIdxSz;
    IDInteger gameCnt, eventCnt, playerCnt, siteCnt, commentCnt;

    ParaRecord paraRecord;

    mutable std::mutex pgnOfsMutex;
    std::ofstream pgnOfs;
    
    /// For stats
    std::chrono::steady_clock::time_point startTime;
    int64_t blockCnt, processedPgnSz, errCnt, succCount;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
