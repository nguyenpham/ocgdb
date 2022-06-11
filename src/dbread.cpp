/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "3rdparty/SQLiteCpp/VariadicBind.h"
#include "3rdparty/sqlite3/sqlite3.h"

#include "dbread.h"

using namespace ocgdb;

const std::string DbRead::fullGameQueryString =
    "SELECT e.Name Event, s.Name Site, w.Name White, b.Name Black, g.* " \
    "FROM Games g " \
    "INNER JOIN Players w ON WhiteID = w.ID " \
    "INNER JOIN Players b ON BlackID = b.ID " \
    "INNER JOIN Events e ON EventID = e.ID " \
    "INNER JOIN Sites s ON SiteID = s.ID";

const std::string DbRead::searchFieldNames[] = {
    "",
    "Moves",
    "Moves1",
    "Moves2"
};


const char* DbRead::tagNames[] = {
    "GameID", // Not real PGN tag, added for convernience
    
    "Event", "Site", "Date", "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount", "FEN",

    nullptr, nullptr
};




DbRead::DbRead()
{
}

DbRead::~DbRead()
{
    closeDb();
}

void DbRead::closeDb()
{
    if (mDb) {
        delete mDb;
        mDb = nullptr;
    }
}


SearchField DbRead::getMoveField(SQLite::Database* db, bool* hashMoves)
{
    assert(db);
    auto searchField = SearchField::none;
    if (hashMoves) *hashMoves = false;

    SQLite::Statement stmt(*db, "PRAGMA table_info(Games)");
    while (stmt.executeStep()) {
        std::string fieldName = stmt.getColumn(1).getText();
        
        if (fieldName == "Moves2") {
            searchField = SearchField::moves2;
            break;
        }
        if (fieldName == "Moves1") {
            if (searchField < SearchField::moves1) {
                searchField = SearchField::moves1;
            }
        }
        if (fieldName == "Moves") {
            if (hashMoves) *hashMoves = true;
            if (searchField < SearchField::moves) {
                searchField = SearchField::moves;
            }
        }
    }

    return searchField;
}



void DbRead::extractHeader(SQLite::Statement& query, bslib::PgnRecord& record)
{
    for(int i = 0, cnt = query.getColumnCount(); i < cnt; ++i) {
        auto c = query.getColumn(i);
        std::string name = c.getName();
        if (name == "ID") {
            record.gameID = c.getInt();
            continue;
        }
        if (name == "EventID") {
            record.eventID = c.getInt();
            continue;
        }
        if (name == "SiteID") {
            record.siteID = c.getInt();
            continue;
        }
        if (name == "WhiteID") {
            record.whiteID = c.getInt();
            continue;
        }
        if (name == "BlackID") {
            record.blackID = c.getInt();
            continue;
        }

        // Ignore Moves, Moves1, Moves2
        if (name == "Moves" || name == "Moves1" || name == "Moves2") {
            continue;
        }
        
        std::string str;
        
        switch (c.getType())
        {
            case SQLITE_INTEGER:
            {
                auto k = c.getInt();
                str = std::to_string(k);
                break;
            }
            case SQLITE_FLOAT:
            {
                auto k = c.getDouble();
                str = std::to_string(k);
                break;
            }
            case SQLITE_BLOB:
            {
                // something wrong
                break;
            }
            case SQLITE_NULL:
            {
                // something wrong
                break;
            }
            case SQLITE3_TEXT:
            {
                str = c.getString();
                if (name == "FEN") record.fenText = str;
                break;
            }

            default:
                assert(0);
                break;
        }

        if (name != "Event" && str.empty()) {
            continue;
        }
        record.tags[name] = str;
    }
}


void DbRead::queryForABoard(bslib::PgnRecord& record,
                            SearchField searchField,
                            SQLite::Statement* query,
                            SQLite::Statement* queryComments,
                            bslib::BoardCore* board)
{
    assert(query && board);

    extractHeader(*query, record);

    board->newGame(record.fenText);

    // Tables games may have 0-2 columns for moves
    if (searchField == SearchField::moves1 || searchField == SearchField::moves2) {
        auto moveName = "Moves2";

        int flag = bslib::BoardCore::ParseMoveListFlag_create_san;

        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
            moveName = "Moves1";
        }

        auto c = query->getColumn(moveName);
        auto moveBlob = static_cast<const int8_t*>(c.getBlob());

        if (moveBlob) {
            std::vector<int8_t> moveVec;
            auto sz = c.size();
            for(auto i = 0; i < sz; ++i) {
                moveVec.push_back(moveBlob[i]);
            }

            board->fromMoveList(&record, moveVec, flag, nullptr);

            if (queryComments) {
                queryComments->reset();
                queryComments->bind(1, record.gameID);
                while (queryComments->executeStep()) {
                    auto comment = queryComments->getColumn("Comment").getString();
                    if (comment.empty()) continue;

                    auto ply = queryComments->getColumn("Ply").getInt();
                    if (ply >= 0 && ply < board->getHistListSize()) {
                        board->_getHistPointerAt(ply)->comment = comment;
                    } else {
                        board->setFirstComment(comment);
                    }
                }
            }
        }
    } else if (searchField == SearchField::moves) {
        record.moveString = query->getColumn("Moves").getString();

        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                    | bslib::BoardCore::ParseMoveListFlag_discardComment
                    | bslib::BoardCore::ParseMoveListFlag_create_san;

        board->fromMoveList(&record, bslib::Notation::san, flag, nullptr);
    }
}


/*
 This function is just an example how to query and extract data from a record with a given game ID
 */
void DbRead::printGamePGNByIDs(SQLite::Database& db, const std::vector<int>& gameIDVec, SearchField searchField)
{
    QueryGameRecord qgr(db, searchField);
    printGamePGNByIDs(qgr, gameIDVec);
}


void DbRead::printGamePGNByIDs(QueryGameRecord& qgr, const std::vector<int>& gameIDVec)
{
    for(auto && gameID : gameIDVec) {
        bslib::PgnRecord record;
        record.gameID = gameID;
        
        std::string str = "\n\n;ID: " + std::to_string(gameID) + "\n"
                + qgr.queryAndCreatePGNByGameID(record);
        printOut.printOut(str);
    }
}


bool DbRead::openDB(const std::string& dbPath)
{
    mDb = DbCore::openDB(dbPath, (paraRecord.optionFlag & dup_flag_remove) == 0);
    if (!mDb) {
        return false;
    }

    searchField = DbRead::getMoveField(mDb);
    if (searchField == SearchField::none) {
        std::cerr << "Error: database " << dbPath << " has not any move field" << std::endl;
        return false;
    }
    return true;
}

bool DbRead::readADb(const std::string& dbPath, const std::string& sqlString)
{
    if (!openDB(dbPath)) {
        return false;
    }
    
    for(auto && t : threadMap) {
        t.second.resetStats();
    }

    auto moveName = DbRead::searchFieldNames[static_cast<int>(searchField)];
    assert(!moveName.empty());
    
    int flag = bslib::BoardCore::ParseMoveListFlag_quick_check | bslib::BoardCore::ParseMoveListFlag_discardComment;
    if (searchField == SearchField::moves1) {
        flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
    }

    {
        SQLite::Statement statement(*mDb, sqlString);
        
        for (gameCnt = 0; statement.executeStep(); ++gameCnt) {
            if (paraRecord.limitLen) {
                auto c = statement.getColumn("PlyCount");
                if (!c.isNull() && c.getInt() < paraRecord.limitLen) {
                    continue;
                }
            }

            bslib::PgnRecord record;

            record.gameID = statement.getColumn("ID").getInt();
            record.fenText = statement.getColumn("FEN").getText();
            std::vector<int8_t> moveVec;

            if (searchField == SearchField::moves) {
                record.moveString = statement.getColumn("Moves").getText();
                if (record.moveString.empty()) {
                    continue;
                }
            } else {
                auto c = statement.getColumn(moveName.c_str());
                auto moveBlob = static_cast<const int8_t*>(c.getBlob());
                
                if (moveBlob) {
                    auto sz = c.size();
                    for(auto i = 0; i < sz; ++i) {
                        moveVec.push_back(moveBlob[i]);
                    }
                }
                
                if (moveVec.empty()) {
                    continue;
                }
            }

            if (paraRecord.optionFlag & query_flag_print_pgn) {
                DbRead::extractHeader(statement, record);
            }
            threadProcessAGame(record, moveVec);

            if (gameCnt && (gameCnt & 0xffff) == 0) {
                printStats();
            }

            if (succCount >= paraRecord.resultNumberLimit) {
                break;
            }
        }

        pool->wait_for_tasks();
        printStats();
    }

    int64_t delCnt = 0;
    for(auto && t : threadMap) {
        t.second.deleteAllStatements();
        delCnt += t.second.delCnt;
    }

    // Update table Info
    if (delCnt > 0) {
        int64_t gCnt = gameCnt - delCnt;
        
        std::string sqlstr = "UPDATE Info SET Value = '" + std::to_string(gCnt) + "' WHERE Name = 'GameCount'";
        mDb->exec(sqlstr);

        mDb->exec("COMMIT");
    }

    closeDb();
    return true;
}

void doProcessAGame(DbRead* instance, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(instance);
    instance->processAGame(record, moveVec);
}

void DbRead::threadProcessAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(pool);
    pool->submit(doProcessAGame, this, record, moveVec);
}


void DbRead::processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(!record.moveString.empty() || record.moveText || !moveVec.empty());
    assert(record.gameID > 0);

    auto t = getThreadRecord(); assert(t);
    processAGameWithAThread(t, record, moveVec);
}

void DbRead::processAGameWithAThread(ThreadRecord* t, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    
}
