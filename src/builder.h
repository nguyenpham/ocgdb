/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_BUILDER_H
#define OCGDB_BUILDER_H

#include <vector>
#include <unordered_map>
#include <fstream>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/threadpool/thread_pool.hpp"

#include "board/types.h"
#include "board/base.h"

#include "records.h"
#include "pgnread.h"
#include "dbcore.h"


namespace ocgdb {



class Builder : virtual public PGNRead, virtual public DbCore
{
protected:
    void create();

    bool createDb(const std::string& path);

    void createDb_EPD();

protected:
    virtual void runTask() override;
    virtual int getEventNameId(char* name);
    virtual int getSiteNameId(char* name);
    virtual int getPlayerNameId(char* name, int elo);

    void setupTagVec(const std::vector<std::string>& tagVec, int optionFlag);

    virtual IDInteger getNewGameID();

    virtual void updateInfoTable();

    
private:

    bool createInsertStatements(SQLite::Database& mDb);
    virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *) override;
    void processPGNGameWithAThread_OCGDB(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *);

    static SQLite::Database* createDb(const std::string& path, int optionFlag, const std::vector<std::string>& tagVec, const std::string& dbDescription);

    IDInteger getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap);

    virtual void printStats() const override;

    bool addNewField(const std::string& fieldName);

public:
    static int standardizeFEN(char *fenBuf);
    static void standardizeDate(char* date);
    static std::string standardizeDate(const std::string& date);
    static std::string encodeString(const std::string& name);

private:
    void processFile_EPD(const std::string& path);
    void updateInfoTable_EPD();
    bool createDb_EPD(const std::string& path, const std::string& description);

    static EPDRecord processALine_EPD(const std::string&);
    void saveToDb_EPD();
    void processPGNGameWithAThread_EPD(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *);
    EPDRecord process_addBoard_EPD(const bslib::BoardCore* board, const bslib::Hist*);

protected:
    std::vector<std::string> create_tagVec;
    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, commentMutex;

    mutable std::mutex create_tagFieldMutex;
    std::unordered_map<std::string, int> create_tagMap;

private:
    mutable std::mutex transactionMutex;
    const int TransactionCommit = 256 * 1024;
    int transactionCnt = 0;
    
    std::unordered_map<std::string, IDInteger> playerIdMap, eventIdMap, siteIdMap;

    /// Prepared statements
    SQLite::Statement *playerInsertStatement = nullptr;
    SQLite::Statement *eventInsertStatement = nullptr;
    SQLite::Statement *siteInsertStatement = nullptr;

    SQLite::Statement *benchStatement = nullptr;

    mutable std::mutex epdRecordVecMutex;
    std::vector<EPDRecord> epdRecordVec;
    mutable std::mutex epdVistedHashSetMutex;
    std::set<uint64_t> epdVistedHashSet;

    SQLite::Statement *epdInsertStatement = nullptr;
    std::vector<std::string> epdFieldList;

    bslib::BoardCore* epdBoard = nullptr;

};


} // namespace ocdb
#endif // OCGDB_BUILDER_H
