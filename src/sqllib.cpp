/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <set>
#include <fstream>

#include "3rdparty/SQLiteCpp/VariadicBind.h"
#include "3rdparty/sqlite3/sqlite3.h"

#include "board/chess.h"
#include "builder.h"

#include "parser.h"
#include "sqllib.h"

bool debugMode = false;

using namespace ocgdb;


const std::string SqlLib::fullGameQueryString =
    "SELECT e.Name Event, s.Name Site, w.Name White, b.Name Black, g.* " \
    "FROM Games g " \
    "INNER JOIN Players w ON WhiteID = w.ID " \
    "INNER JOIN Players b ON BlackID = b.ID " \
    "INNER JOIN Events e ON EventID = e.ID " \
    "INNER JOIN Sites s ON SiteID = s.ID";

const std::string SqlLib::searchFieldNames[] = {
    "",
    "Moves",
    "Moves1",
    "Moves2"
};


bool ParaRecord::isValid() const
{
    if (dbPaths.empty() && task != Task::query) {
        errorString = "Must have a database (.db3) path. Mising or wrong parameter -db";
        return false;
    }
    
    auto hasPgn = false;
    for(auto && s : pgnPaths) {
        if (!s.empty()) {
            hasPgn = true;
            break;
        }
    }

    errorString.clear();
    auto ok = false;
    switch (task) {
        case Task::create:
        {
            if (!hasPgn) {
                errorString = "Must have at least one PGN path. Mising or wrong parameter -pgn";
                break;
            }
            
            ok = true;
            break;
        }
        case Task::merge:
        {
            if (dbPaths.size() < 2) {
                errorString = "Must have from 2 database (.db3) paths. Mising or wrong parameter -db";
                return false;
            }
            ok = true;
            break;
        }
        case Task::dup:
        {
            if (dbPaths.empty()) {
                errorString = "Must have at least a database (.db3) path. Mising or wrong parameter -db";
                return false;
            }
            ok = true;
            break;
        }

        case Task::export_:
        {
            if (!hasPgn || pgnPaths.size() != 1) {
                errorString = "Must have one PGN path. Mising or wrong parameter -pgn";
                break;
            }

            ok = true;
            break;
        }

        case Task::query:
            if (dbPaths.empty() && !hasPgn) {
                errorString = "Must have a database (.db3) path or a PGN path. Mising or wrong parameter -db and -pgn";
                return false;
            }
            if (queries.empty()) {
                errorString = "Must have at least one query. Mising or wrong parameter -q";
                break;
            }
        case Task::bench:
        {
            ok = true;
            break;
        }
        case Task::getgame:
        {
            if (gameIDVec.empty()) {
                errorString = "gameID must be greater than zero";
                break;
            }

            ok = true;
            break;
        }

        default:
            break;
    }
    return ok;
}


static const std::map<std::string, int> optionNameMap = {
    // creating
    {"moves", 0},
    {"moves1", 1},
    {"moves2", 2},
    {"acceptnewtags", 3},
    {"discardcomments", 4},
    {"discardsites", 5},
    {"discardnoelo", 6},
    // query
    {"printall", 7},
    {"printfen", 8},
    {"printpgn", 9},

    {"remove", 10},
};


std::string ParaRecord::toString() const
{
    std::string s;
    
    const std::string taskNames[] = {
        "create SQL database",
        "export",
        "merge",
        "query",
        "bench",
        "get game",
        "duplicate"

    };
        
    s = "\tTask: " + taskNames[static_cast<int>(task)] + "\n";
    
    s += "\tPGN paths:\n";
    for(auto && path : pgnPaths) {
        s += "\t\t" + path + "\n";
    }
    
    s += "\tDatabase paths:\n";
    for(auto && path : dbPaths) {
        s += "\t\t" + path + "\n";
    }

    s += "\tQueries:\n";
    for(auto && query : queries) {
        s += "\t\t" + query + "\n";
    }

    const std::string moveModeNames[] = {
        "none",
        "Moves", "Moves1", "Moves2",
        "Moves + Moves1", "Moves + Moves2"
    };

    s += "\tOptions: ";
    
    for(auto && it : optionNameMap) {
        if (optionFlag & (1 << it.second)) {
            s += it.first + ";";
        }
    }

    s += "\n";
    s += "\tgameNumberLimit: " + std::to_string(gameNumberLimit) + "\n"
        + "\tresultNumberLimit: " + std::to_string(resultNumberLimit) + "\n"
        + "\tcpu: " + std::to_string(cpuNumber)
        + ", min Elo: " + std::to_string(limitElo)
        + ", min game length: " + std::to_string(limitLen)
        + "\n";

    return s;
}


void ParaRecord::setupOptions(const std::string& optionString)
{
//    optionFlag = 0;
    auto vec = bslib::Funcs::splitString(optionString, ';');
    
    for(auto && s : vec) {
        auto it = optionNameMap.find(s);
        if (it == optionNameMap.end()) {
            std::cerr << "Error: Don't know option string: " << it->first << std::endl;
        } else {
            optionFlag |= 1 << it->second;
            
            if (s == "printpgn" || s == "printfen") {
                auto it2 = optionNameMap.find("printall");
                assert(it2 != optionNameMap.end());
                optionFlag |= 1 << it2->second;
            }
        }
    }
}

const char* SqlLib::tagNames[] = {
    "GameID", // Not real PGN tag, added for convernience
    
    "Event", "Site", "Date", "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount", "FEN",

    nullptr, nullptr
};


void ThreadRecord::init(SQLite::Database* mDb)
{
    if (board) return;
    
    errCnt = 0;
    
    board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    buf = new int8_t[1024 * 2];

    if (mDb) {
        insertCommentStatement = new SQLite::Statement(*mDb, "INSERT INTO Comments (GameID, Ply, Comment) VALUES (?, ?, ?)");
    }
}


ThreadRecord::~ThreadRecord()
{
    if (buf) delete buf;
    if (board) delete board;
    if (board2) delete board2;    
    deleteAllStatements();
}


void ThreadRecord::deleteAllStatements()
{
    if (insertGameStatement) delete insertGameStatement;
    if (insertCommentStatement) delete insertCommentStatement;
    if (removeGameStatement) delete removeGameStatement;
    if (getGameStatement) delete getGameStatement;
    if (queryComments) delete queryComments;
    if (qgr) delete qgr;
    insertGameStatement = nullptr;
    insertCommentStatement = nullptr;
    removeGameStatement = nullptr;
    getGameStatement = nullptr;
    queryComments = nullptr;
    qgr = nullptr;
}


void ThreadRecord::resetStats()
{
    errCnt = gameCnt = hdpLen = dupCnt = delCnt = 0;
}


bool ThreadRecord::createInsertGameStatement(SQLite::Database* mDb, const std::unordered_map<std::string, int>& fieldOrderMap)
{
    if (insertGameStatement) {
        delete insertGameStatement;
        insertGameStatement = nullptr;
    }

    std::string sql0 = "INSERT INTO Games (ID, EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, TimeControl, ECO, PlyCount, FEN";
    std::string sql1 = ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?";

    std::unordered_map<int, std::string> map;
    for(auto && it : fieldOrderMap) {
        if (it.second >= TagIdx_Max) {
            map[it.second] = it.first;
        }
    }
    
    insertGameStatementIdxSz = TagIdx_Max;

    if (!map.empty()) {
        for(int i = TagIdx_Max; ; ++i) {
            auto it = map.find(i);
            if (it == map.end()) break;
            
            sql0 += ", " + it->second;
            sql1 += ", ?";
            
            insertGameStatementIdxSz++;
        }
    }

    insertGameStatement = new SQLite::Statement(*mDb, sql0 + sql1 + ")");
    return true;
}
//////////////////////////////////




QueryGameRecord::QueryGameRecord(SQLite::Database& db, SearchField searchField)
: searchField(searchField)
{
    std::string str = SqlLib::fullGameQueryString + " WHERE g.ID = ?";
    queryGameByID = new SQLite::Statement(db, str);
    queryComments = new SQLite::Statement(db, "SELECT * FROM Comments WHERE GameID = ?");
    
    board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
}


std::string QueryGameRecord::queryAndCreatePGNByGameID(bslib::PgnRecord& record)
{
    std::lock_guard<std::mutex> dolock(queryMutex);

    assert(queryGameByID);

    queryGameByID->reset();
    queryGameByID->bind(1, record.gameID);
    
    std::string str;
    
    if (queryGameByID->executeStep()) {
        SqlLib::queryForABoard(record, searchField, queryGameByID, queryComments, board);
        str = board->toPgn(&record);
    }
    assert(!str.empty());
    return str;
}

//////////////////////////////////

SearchField SqlLib::getMoveField(SQLite::Database* db, bool* hashMoves)
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



void SqlLib::extractHeader(SQLite::Statement& query, bslib::PgnRecord& record)
{
    for(int i = 0, cnt = query.getColumnCount(); i < cnt; ++i) {
        auto c = query.getColumn(i);
        std::string name = c.getName();
        if (name == "ID") {
            record.gameID = c.getInt();
            continue;
        }

        // Ignore all ID columns and Moves, Moves1, Moves2
        if (name == "EventID" || name == "SiteID"
            || name == "WhiteID" || name == "BlackID"
            || name == "Moves" || name == "Moves1" || name == "Moves2") {
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


void SqlLib::queryForABoard(bslib::PgnRecord& record,
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


// replace Halfmove clock to 0 and Fullmove number to 1
int SqlLib::standardizeFEN(char *fenBuf)
{
    assert(fenBuf);
    auto fenSz = static_cast<int>(strlen(fenBuf));
    assert(fenSz > 10 && fenSz < 90);
    
    for(auto i = fenSz - 2, c = 0; i > 0; --i) {
        if (fenBuf[i] == ' ') {
            c++;
            if (c >= 2) {
                fenBuf[i + 1] = '0';
                fenBuf[i + 2] = ' ';
                fenBuf[i + 3] = '1';
                fenBuf[i + 4] = 0;
                return i + 4;
            }
        }
    }

    return fenSz;
}
