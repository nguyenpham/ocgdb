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
private:
    virtual void processPGNGameByAThread(const std::unordered_map<char*, char*>&, const char *) override;
    virtual void runTask() override;
    
    static SQLite::Database* createDb(const std::string& path, int optionFlag, const std::vector<std::string>& tagVec);
    bool createInsertStatements(SQLite::Database& mDb);

    int getEventNameId(char* name);
    int getSiteNameId(char* name);
    int getPlayerNameId(char* name, int elo);

    IDInteger getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap);

    virtual void printStats() const override;

    bool addNewField(const std::string& fieldName);

public:
    static int standardizeFEN(char *fenBuf);
    static void standardizeDate(char* date);
    static std::string standardizeDate(const std::string& date);
    static std::string encodeString(const std::string& name);

private:
    std::unordered_map<std::string, IDInteger> playerIdMap, eventIdMap, siteIdMap;

    /// Prepared statements
    SQLite::Statement *playerInsertStatement = nullptr;
    SQLite::Statement *eventInsertStatement = nullptr;
    SQLite::Statement *siteInsertStatement = nullptr;

    SQLite::Statement *benchStatement = nullptr;

    mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex, commentMutex;

    std::vector<std::string> create_tagVec;
    std::unordered_map<std::string, int> create_tagMap;
    mutable std::mutex create_tagFieldMutex;
};


} // namespace ocdb
#endif // OCDB_BUILDER_H
