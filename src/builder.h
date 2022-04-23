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

#include "records.h"
#include "pgnread.h"


namespace ocgdb {



class Builder : public PGNRead
{
protected:
    void create();

    bool createDb(const std::string& path);

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
    
    static SQLite::Database* createDb(const std::string& path, int optionFlag, const std::vector<std::string>& tagVec, const std::string& dbDescription);

    IDInteger getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap);

    virtual void printStats() const override;

    bool addNewField(const std::string& fieldName);

public:
    static int standardizeFEN(char *fenBuf);
    static void standardizeDate(char* date);
    static std::string standardizeDate(const std::string& date);
    static std::string encodeString(const std::string& name);

protected:
    std::vector<std::string> create_tagVec;
    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, commentMutex;

    mutable std::mutex create_tagFieldMutex;
    std::unordered_map<std::string, int> create_tagMap;

private:
    std::unordered_map<std::string, IDInteger> playerIdMap, eventIdMap, siteIdMap;

    /// Prepared statements
    SQLite::Statement *playerInsertStatement = nullptr;
    SQLite::Statement *eventInsertStatement = nullptr;
    SQLite::Statement *siteInsertStatement = nullptr;

    SQLite::Statement *benchStatement = nullptr;


};


} // namespace ocdb
#endif // OCDB_BUILDER_H
