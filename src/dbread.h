/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef DBREAD_H
#define DBREAD_H

#include "dbcore.h"

namespace ocgdb {


class DbRead : public virtual DbCore
{
public:
    DbRead();
    virtual ~DbRead();

    virtual void processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec);
    
protected:
    virtual void processAGameWithAThread(ThreadRecord* t, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec);

public:
    static SearchField getMoveField(SQLite::Database* db, bool* hashMoves = nullptr);

    static void extractHeader(SQLite::Statement& query, bslib::PgnRecord& record);
    static void queryForABoard( bslib::PgnRecord& record,
                                SearchField searchField,
                                SQLite::Statement* query,
                                SQLite::Statement* queryComments,
                                bslib::BoardCore* board);
    
    bool readADb(const std::string& dbPath, const std::string& sqlString);

public:
    static const std::string fullGameQueryString;
    static const std::string searchFieldNames[];
    static const char* tagNames[];

protected:
    virtual bool openDB(const std::string& dbPath);
    virtual void closeDb();

    static void printGamePGNByIDs(SQLite::Database& db, const std::vector<int>& gameIDVec, SearchField);
    
    static void printGamePGNByIDs(QueryGameRecord&, const std::vector<int>&);


protected:
    std::function<bool(const std::vector<uint64_t>&, const bslib::BoardCore*, const bslib::PgnRecord*)> checkToStop = nullptr;
    std::function<bool(const bslib::BoardCore*, const bslib::PgnRecord*)> boardCallback = nullptr;

private:
    void threadProcessAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec);

private:
    QueryGameRecord* qgr = nullptr;

};

} // namespace ocdb

#endif /* DBREAD_H */
