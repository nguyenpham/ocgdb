/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "dbcore.h"

using namespace ocgdb;

void DbCore::queryInfo()
{
    playerCnt = eventCnt = gameCnt = siteCnt = -1;
    
    if (!mDb) return;

    SQLite::Statement query(*mDb, "SELECT * FROM Info");
    
    while (query.executeStep()) {
        auto name = query.getColumn(0).getString();
        auto v = query.getColumn(1);
        if (name == "GameCount") {
            gameCnt = v.getInt();
        } else if (name == "PlayerCount") {
            playerCnt = v.getInt();
        } else if (name == "EventCount") {
            eventCnt = v.getInt();
        } else if (name == "SiteCount") {
            siteCnt = v.getInt();
        } else if (name == "CommentCount") {
            commentCnt = v.getInt();
        }
    }

    if (gameCnt < 0) {
        gameCnt = 0;
        SQLite::Statement query(*mDb, "SELECT COUNT(*) FROM Games");
        if (query.executeStep()) {
            gameCnt = query.getColumn(0).getInt();
        }
    }

    if (playerCnt < 0) {
        playerCnt = 0;
        SQLite::Statement query(*mDb, "SELECT COUNT(*) FROM Players");
        if (query.executeStep()) {
            playerCnt = query.getColumn(0).getInt();
        }
    }
}

void DbCore::sendTransaction(SQLite::Database* db, bool begin)
{
    if (db) {
        db->exec(begin ? "BEGIN" : "COMMIT");
    }
}

SQLite::Database* DbCore::openDB(const std::string& dbPath, bool readonly)
{
    auto mDb = new SQLite::Database(dbPath, readonly ? SQLite::OPEN_READONLY : SQLite::OPEN_READWRITE);
    if (!mDb) {
        std::cerr << "Error: can't open database " << dbPath << std::endl;
        return mDb;
    }

//    searchField = DbRead::getMoveField(mDb);
//    if (searchField == SearchField::none) {
//        std::cerr << "Error: database " << dbPath << " has not any move field" << std::endl;
//        return false;
//    }
    return mDb;
}
