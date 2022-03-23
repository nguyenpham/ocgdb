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

#include "board/chess.h"
#include "dbread.h"

#include "records.h"

bool debugMode = false;


using namespace ocgdb;


// same order and meaning with TagIdx_
const std::vector<std::string> ocgdb::knownPgnTagVec = {
    "ID",
    "Event", "Site", "Date",
    "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount",
    "FEN"
};


////////////////////////////////////////////////////////////////////////
bool ParaRecord::isValid() const
{
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
            if (!hasPgn || dbPaths.empty()) {
                errorString = "Must have a database path and at least one PGN path. Mising or wrong parameter -db -pgn";
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
            if (dbPaths.empty() || !hasPgn || pgnPaths.size() != 1) {
                errorString = "Must have a database path and a PGN path. Mising or wrong parameter -db -pgn";
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
            ok = true;
            break;
            
        case Task::bench:
        {
            if (dbPaths.empty()) {
                errorString = "Must have a database (.db3) path. Mising or wrong parameter -db";
                return false;
            }

            ok = true;
            break;
        }
        case Task::getgame:
        {
            if (dbPaths.empty() || gameIDVec.empty()) {
                errorString = "Must have a database (.db3) path and one or some game IDs, each game ID must be greater than zero";
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
    {"discardfen", 7},
    {"reseteco", 8},

    // query
    {"printall", 10},
    {"printfen", 11},
    {"printpgn", 12},

    {"remove", 15},
    {"embededgames", 16},
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

    s += "\tGame IDs:\n";
    for(auto && numb : gameIDVec) {
        s += "\t\t" + std::to_string(numb) + "\n";
    }

    s += "\tReport path:\n";
    s += "\t\t" + reportPath + "\n";

    const std::string moveModeNames[] = {
        "none",
        "Moves", "Moves1", "Moves2",
        "Moves + Moves1", "Moves + Moves2"
    };

    s += "\tOptions: ";
    
    for(auto && it : optionNameMap) {
        if (optionFlag & (1 << it.second)) {
            s += it.first + ",";
        }
    }
    
    s += "\n";

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
    auto vec = bslib::Funcs::splitString(optionString, ',');
    
    for(auto && s : vec) {
        auto it = optionNameMap.find(s);
        if (it == optionNameMap.end()) {
            std::cerr << "Error: Don't know option string: " << s << std::endl;
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


bool ThreadRecord::createInsertGameStatement(SQLite::Database* mDb, const std::vector<std::string>& fieldVec)
{
    if (insertGameStatement) {
        delete insertGameStatement;
        insertGameStatement = nullptr;
    }

    std::string sql0 = "INSERT INTO Games (";
    std::string sql1 = ") VALUES (";

    auto cnt = 0;
    for(auto str : fieldVec) {
        // Special fields, using ID
        if (str == "Event" || str == "Site" || str == "White" || str == "Black") {
            str += "ID";
        }
        if (cnt) {
            sql0 += ",";
            sql1 += ",";
        }
        sql0 += str;
        sql1 += ":" + str;
        cnt++;
    }
    
    insertGameStatementIdxSz = static_cast<int>(fieldVec.size());
    insertGameStatement = new SQLite::Statement(*mDb, sql0 + sql1 + ")");
    return true;
}

//////////////////////////////////




QueryGameRecord::QueryGameRecord(SQLite::Database& db, SearchField searchField)
: searchField(searchField)
{
    std::string str = DbRead::fullGameQueryString + " WHERE g.ID = ?";
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
        DbRead::queryForABoard(record, searchField, queryGameByID, queryComments, board);
        str = board->toPgn(&record);
    }
    return str;
}

//////////////////////////////////

