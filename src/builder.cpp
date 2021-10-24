/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
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


using namespace ocgdb;


Builder::Builder()
{
}

Builder::~Builder()
{
    if (mDb) {
        delete mDb;
        mDb = nullptr;
    }

    if (board) delete board;
}

BoardCore* Builder::createBoard(ChessVariant variant)
{
    return variant == ChessVariant::standard ? new ChessBoard() : nullptr;
}

std::chrono::steady_clock::time_point getNow()
{
    return std::chrono::steady_clock::now();
}

void Builder::printStats() const
{
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    std::cout   << "#games: " << gameCnt
                << ", #errors: " << errCnt
              << ", elapsed: " << elapsed << " ms, "
              << Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << gameCnt * 1000 / elapsed << " games/s"
              << std::endl;
}



void Builder::convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath)
{
    // Prepare
    board = createBoard(chessVariant);
    setDatabasePath(sqlitePath);
    
    // Create database
    createDb(sqlitePath);
    openDbToWrite();

    processPgnFile(pgnPath);

    // Clean up
    delete board;
    board = nullptr;
}

uint64_t Builder::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;

    startTime = getNow();
    gameCnt = errCnt = 0;

    std::ifstream inFile(path);
    if (inFile.fail()) {
        std::cerr << "Error opeing the PGN file" << std::endl;
        inFile.close();
        return false;
    }

    std::vector<std::string> lines;
    std::string str;
    while (getline(inFile, str)) {
        auto p = str.find("[Event");
        if (p != std::string::npos && p + 6 < str.length() && !std::isalnum(str.at(p + 6))) {
            parseAGame(lines);
            lines.clear();
            
            // Frequently update info
            if (gameCnt && (gameCnt & 0xffff) == 0) {
                printStats();
            }
        }
        lines.push_back(str);
    }

    parseAGame(lines);
    inFile.close();

    {
        mDb->exec("DELETE FROM info WHERE name = 'games'");
        mDb->exec("DELETE FROM info WHERE name = 'players'");
        
        int gameCnt = mDb->execAndGet("SELECT COUNT(*) FROM game");
        auto str = std::string("INSERT INTO info(name, value) VALUES ('games', '") + std::to_string(gameCnt) + "')";
        mDb->exec(str);

        int playerCnt = mDb->execAndGet("SELECT COUNT(*) FROM player");
        str = std::string("INSERT INTO info(name, value) VALUES ('players', '") + std::to_string(playerCnt) + "')";
        mDb->exec(str);
    }

    printStats();

    std::cout << "Completed! " << std::endl;
    return gameCnt;
}

bool Builder::parseAGame(const std::vector<std::string>& lines)
{
    if (lines.size() < 4) {
        return false;
    }

    gameCnt++;
    std::unordered_map<std::string, std::string> itemMap;
    std::string moveText;

    for(auto && s : lines) {
        auto p = s.find("[");
        if (p == 0) {
            p++;
            std::string key;
            for(auto q = p + 1; q < s.length(); q++) {
                if (s.at(q) <= ' ') {
                    key = s.substr(p, q - p);
                    break;
                }
            }
            if (key.empty()) continue;
            p = s.find("\"");
            if (p == std::string::npos) continue;
            p++;
            auto q = s.find("\"", p);
            if (q == std::string::npos) continue;
            auto str = s.substr(p, q - p);
            if (str.empty()) continue;

            Funcs::toLower(key);
            itemMap[key] = str;
            continue;
        }
        moveText += " " + s;
    }

    // at the moment, support only standard one
    auto p = itemMap.find("variant");
    if (p != itemMap.end()) {
        auto variant = Funcs::string2ChessVariant(p->second);

        if (board->variant != variant) {
            std::cerr << "Error: variant " << p->second << " is not supported." << std::endl;
            errCnt++;
            return false;
        }
    }

    if (addGame(itemMap, moveText)) {
        return true;
    }

    errCnt++;
    return false;
}

bool Builder::addGame(const std::unordered_map<std::string, std::string>& itemMap, const std::string& moveText)
{
    GameRecord r;

    auto it = itemMap.find("event");
    if (it == itemMap.end())
        return false;
    r.eventName = it->second;

    it = itemMap.find("white");
    if (it == itemMap.end())
        return false;
    r.whiteName = it->second;

    it = itemMap.find("whiteelo");
    if (it != itemMap.end()) {
        r.whiteElo = std::atoi(it->second.c_str());
    }

    it = itemMap.find("black");
    if (it == itemMap.end())
        return false;
    r.blackName = it->second;

    it = itemMap.find("blackelo");
    if (it != itemMap.end()) {
        r.blackElo = std::atoi(it->second.c_str());
    }

    it = itemMap.find("date");
    if (it != itemMap.end()) {
        r.dateString = it->second;
    }

    it = itemMap.find("utcdate");
    if (it != itemMap.end()) {
        r.dateString = it->second;
    }

    it = itemMap.find("timecontrol");
    if (it != itemMap.end()) {
        r.timer = it->second;
    }

    it = itemMap.find("eco");
    if (it != itemMap.end()) {
        r.eco = it->second;
    }

    it = itemMap.find("result");
    if (it != itemMap.end()) {
        r.resultType = Funcs::string2ResultType(it->second);
    }

    // Open game to verify and count the number of half-moves
    {
        auto it = itemMap.find("fen");
        if (it != itemMap.end()) {
            r.fen = it->second;
            Funcs::trim(r.fen);
        }

        if (r.fen.empty() && moveText.empty()) {
            return false;
        }

        assert(board);
        board->newGame(r.fen);

        if (!board->isValid()) {
            return false;
        }

        board->fromMoveList(moveText, Notation::san);

        r.moveCnt = board->getHistListSize();
        if (r.moveCnt == 0 && r.fen.empty()) {
            return false;
        }
        r.moveString = board->toMoveListString(Notation::san,
                                               10000000, false,
                                               CommentComputerInfoType::standard);
        assert(!r.moveString.empty());
    }

    return addGame(r);
}


bool Builder::createDb(const std::string& path)
{
    assert(!path.empty());

    try
    {
        // Open a database file in create/write mode
        SQLite::Database db(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
        std::cout << "SQLite database file '" << db.getFilename() << "' opened successfully\n";

        db.exec("DROP TABLE IF EXISTS info");
        db.exec("CREATE TABLE info (name TEXT UNIQUE NOT NULL, value TEXT)");
        db.exec("INSERT INTO info(name, value) VALUES ('version', '0.1')");
        db.exec("INSERT INTO info(name, value) VALUES ('variant', 'standard')");
        db.exec("INSERT INTO info(name, value) VALUES ('license', 'free')");

        db.exec("DROP TABLE IF EXISTS event");
        db.exec("CREATE TABLE event (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)");
        db.exec("INSERT INTO event(name) VALUES (\"\")"); // default empty

        db.exec("DROP TABLE IF EXISTS player");
        db.exec("CREATE TABLE player (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, elo INTEGER)");

        db.exec("DROP TABLE IF EXISTS game");
        db.exec("CREATE TABLE game(id INTEGER PRIMARY KEY AUTOINCREMENT, event_id INTEGER, white_id INTEGER, white_elo INTEGER, black_id INTEGER, black_elo INTEGER, timer TEXT, date TEXT, eco TEXT, result INTEGER, length INTEGER, fen TEXT, moves TEXT, FOREIGN KEY(event_id) REFERENCES event, FOREIGN KEY(white_id) REFERENCES player, FOREIGN KEY(black_id) REFERENCES player)");
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void Builder::setDatabasePath(const std::string& path)
{
    if (dbPath == path) {
        return;
    }

    dbPath = path;
    if (mDb) {
        delete mDb;
        mDb = nullptr;
    }
}

SQLite::Database* Builder::openDbToWrite()
{
    if (!mDb) {
        assert(!dbPath.empty());
        mDb = new SQLite::Database(dbPath, SQLite::OPEN_READWRITE);
    }

    assert(mDb);
    return mDb;
}

std::string Builder::encodeString(const std::string& str)
{
    return Funcs::replaceString(str, "\"", "\\\"");
}

int Builder::getNameId(const std::string& tableName, const std::string& name)
{
    std::string sQuery = "SELECT id FROM " + tableName + " WHERE name=\"" + encodeString(name) + "\"";
    SQLite::Statement query(*mDb, sQuery.c_str());
    return query.executeStep() ? query.getColumn(0) : -1;
}

int Builder::getEventNameId(const std::string& name)
{
    if (name.empty()) {
        return 0;
    }

    auto nameId = getNameId("event", name);
    if (nameId < 0) {
        nameId = mDb->execAndGet("INSERT INTO event(name) VALUES (\"" + name + "\") RETURNING id");
    }

    assert(nameId >= 0);
    return nameId;
}

int Builder::getPlayerNameId(const std::string& name, int elo)
{
    if (name.empty()) {
        return -1;
    }

    auto nameId = getNameId("player", name);
    if (nameId < 0) {
        std::string eloString = elo > 0 ? std::to_string(elo) : "NULL";
        nameId = mDb->execAndGet("INSERT INTO player(name, elo) VALUES (\"" + name + "\", " + eloString + ") RETURNING id");
    }

    assert(nameId >= 0);
    return nameId;
}

bool Builder::addGame(const GameRecord& r)
{
    std::string mQuery, gQuery;
    try
    {
        assert(mDb);
        // Begin transaction
        // SQLite::Transaction transaction(*mDb);

        auto eventId = getEventNameId(r.eventName);
        auto whiteId = getPlayerNameId(r.whiteName, r.whiteElo);
        auto blackId = getPlayerNameId(r.blackName, r.blackElo);

        std::string witeElo = r.whiteElo > 0 ? std::to_string(r.whiteElo) : "NULL";
        std::string blackElo = r.blackElo > 0 ? std::to_string(r.blackElo) : "NULL";

        gQuery = "INSERT INTO game(event_id, white_id, white_elo, black_id, black_elo, timer, result, date, eco, length, fen, moves) VALUES ("
                                           + std::to_string(eventId) + ","
                                           + std::to_string(whiteId) + ","
                                           + witeElo + ","
                                           + std::to_string(blackId) + ","
                                           + blackElo + ",'"
                                           + r.timer + "','"
                                           + Funcs::resultType2String(r.resultType, false) + "', '"
                                           + r.dateString + "', '"
                                           + r.eco + "',"
                                           + std::to_string(r.moveCnt) + ", '"
                                           + r.fen + "',\""
                                           + encodeString(r.moveString)
                                           + "\")";
        int nb = mDb->exec(gQuery);

        if (nb > 0) {
            // Commit transaction
            // transaction.commit();
            return true;
        }
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << ", mQuery: " << mQuery << ", gQuery: " << gQuery << std::endl;
    }

    return false;
}


void Builder::queryGameData(SQLite::Database& db, int gameIdx)
{
    std::string str =
    "SELECT g.id, w.name white, white_elo, b.name black, black_elo, timer, date, result, eco, length, fen, moves " \
    "FROM game g " \
    "INNER JOIN player w ON white_id = w.id " \
    "INNER JOIN player b ON black_id = b.id " \
    "WHERE g.id = " + std::to_string(gameIdx);
    
    SQLite::Statement query(db, str);

    auto ok = false;
    if (query.executeStep()) {
        const int         id     = query.getColumn("id");
        const std::string white  = query.getColumn("white");
        const std::string black  = query.getColumn("black");
        const std::string fen  = query.getColumn("fen");
        const std::string moves  = query.getColumn("moves");
        const int length  = query.getColumn("length");

        ok = id == gameIdx && !white.empty() && !black.empty()
            && (!fen.empty() || !moves.empty())
            && ((length == 0 && moves.empty()) || (length > 0 && !moves.empty()));
        
        if (!ok) {
            std::cerr << "IMHERE" << std::endl;
        }
    }

    if (!ok) {
        std::cerr << "Error: queryGameData incorrect for " << gameIdx << std::endl;
    }
}

void Builder::bench(const std::string& path)
{
    SQLite::Database db(path);  // SQLite::OPEN_READONLY
    std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

    std::cout << "Test Querying Speed..." << std::endl;

    auto query = "SELECT COUNT(id) FROM game";
    int gameCnt = db.execAndGet(query);
    
    std::cout << "" << gameCnt << std::endl;
    
    if (gameCnt < 1) {
        return;
    }
    
    auto startTime = getNow();
    for(auto i = 0, total = 0; i < 100; i++) {
        for(auto j = 0; j < 10000; j++) {
            total++;
            auto gameIdx = (std::rand() % gameCnt) + 1; // not start from 0
            queryGameData(db, gameIdx);
        }

        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;

        std::cout   << "#total queries: " << total
                  << ", elapsed: " << elapsed << " ms, "
                  << Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                  << ", speed: " << total * 1000 / elapsed << " query/s"
                  << std::endl;
    }

    std::cout << "Test DONE." << std::endl;
}
