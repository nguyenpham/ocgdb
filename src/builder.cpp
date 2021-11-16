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
    if (mDb) delete mDb;
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
    setDatabasePath(sqlitePath);
    
    // Create database
    mDb = createDb(sqlitePath);
    if (!mDb) {
        return;
    }

    processPgnFile(pgnPath);
}

// the game between two blocks, first half
void Builder::processHalfBegin(char* buffer, long len)
{
    halfBufSz = 0;
    if (!buffer || len <= 0 || len >= halfBlockSz) {
        return;
    }
    
    if (!halfBuf) {
        halfBuf = (char*)malloc(halfBlockSz + 16);
    }
    
    memcpy(halfBuf, buffer, len);
    halfBufSz = len;
}

// the game between two blocks, second half
void Builder::processHalfEnd(char* buffer, long len)
{
    if (!buffer || !halfBuf) {
        return;
    }
    
    if (len > 0 && len + halfBufSz > halfBlockSz) {
        halfBufSz = 0;
        return;
    }
    
    if (len > 0) {
        memcpy(halfBuf + halfBufSz, buffer, len);
        halfBufSz += len;
    }
    
    halfBuf[halfBufSz] = 0;
    
    processDataBlock(halfBuf, halfBufSz, false);
    halfBufSz = 0;
}

void Builder::processDataBlock(char* buffer, long sz, bool connectBlock)
{
    assert(buffer && sz > 0);
    
    std::unordered_map<std::string, const char*> tagMap;
    
    auto st = 0, eventCnt = 0;
    auto hasEvent = false;
    char *tagName = nullptr, *tagContent = nullptr, *event = nullptr, *moves = nullptr;

    for(char *p = buffer, *end = buffer + sz; p < end; p++) {
        char ch = *p;
        
        switch (st) {
            case 0:
            {
                if (ch == '[') {
                    p++;
                    if (!isalpha(*p)) {
                        continue;
                    }
                    
                    // has a tag
                    if (moves) {
                        if (hasEvent && p - buffer > 2) {
                            *(p - 2) = 0;

                            if (!addGame(tagMap, moves)) {
                                errCnt++;
                            }
                        }

                        tagMap.clear();
                        hasEvent = false;
                        moves = nullptr;
                    }

                    tagName = p;
                    st = 1;
                } else if (ch > ' ') {
                    if (!moves && hasEvent) {
                        moves = p;
                    }
                }
                break;
            }
            case 1: // name tag
            {
                assert(tagName);
                if (!isalpha(ch)) {
                    if (ch <= ' ') {
                        *p = 0; // end of the tag name
                        st = 2;
                    } else { // something wrong
                        st = 0;
                    }
                }
                break;
            }
            case 2: // between name and content of a tag
            {
                if (ch == '"') {
                    st = 3;
                    tagContent = p + 1;
                }
                break;
            }
            case 3:
            {
                if (ch == '"' || ch == 0) { // == 0 trick to process half begin+end
                    *p = 0;
                    
                    if (strcmp(tagName, "Event") == 0) {
                        event = tagName - 1;
                        if (eventCnt == 0 && connectBlock) {
                            long len =  (event - buffer) - 1;
                            processHalfEnd(buffer, len);
                        }
                        hasEvent = true;
                        eventCnt++;
                        gameCnt++;
                    }

                    if (hasEvent) {
                        tagMap[tagName] = tagContent;
                    }

                    tagName = tagContent = nullptr;
                    st = 4;
                }
                break;
            }
            default: // the rest of the tag
            {
                if (ch == '\n' || ch == 0) {
                    st = 0;
                }
                break;
            }
        }
    }
    
    if (connectBlock) {
        processHalfBegin(event, (long)sz - (event - buffer));
    } else if (moves) {
        if (!addGame(tagMap, moves)) {
            errCnt++;
        }
    }
}

uint64_t Builder::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;

    startTime = getNow();
    gameCnt = errCnt = 0;

    // Begin transaction
    SQLite::Transaction transaction(*mDb);

    // prepared statements
    {
        const std::string sql = "INSERT INTO game(Event_id, White_id, WhiteElo, Black_id, BlackElo, Timer, Result, Date, ECO, PlyCount, FEN, Moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        insertGameStatement = new SQLite::Statement(*mDb, sql);
        
        playerGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM player WHERE name=?");
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO player(name, elo) VALUES (?, ?) RETURNING id");

        eventGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM event WHERE name=?");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO event(name) VALUES (?) RETURNING id");

    }
    
    {
        char *buffer = (char*)malloc(blockSz + 16);

        FILE *stream = fopen(path.c_str(), "r");
        assert(stream != NULL);
        fseek(stream, 0, SEEK_END);
        size_t size = ftell(stream);
        fseek(stream, 0, SEEK_SET);
        
        for (size_t sz = 0, idx = 0; sz < size; idx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (fread(buffer, k, 1, stream)) {
                processDataBlock(buffer, k, true);
                if (idx && (idx & 0xf) == 0) {
                    printStats();
                }
            }
            sz += k;
        }

        fclose(stream);
        free(buffer);

        if (halfBuf) {
            if (halfBufSz > 0) {
                processDataBlock(halfBuf, halfBufSz, false);
            }
            
            free(halfBuf);
            halfBuf = 0;
        }
    }
    
    {
        // int gameCnt = mDb->execAndGet("SELECT COUNT(*) FROM game");
        auto str = std::string("INSERT INTO info(name, value) VALUES ('games', '") + std::to_string(gameCnt) + "')";
        mDb->exec(str);

        int playerCnt = mDb->execAndGet("SELECT COUNT(*) FROM player");
        str = std::string("INSERT INTO info(name, value) VALUES ('players', '") + std::to_string(playerCnt) + "')";
        mDb->exec(str);

        int eventCnt = mDb->execAndGet("SELECT COUNT(*) FROM event");
        str = std::string("INSERT INTO info(name, value) VALUES ('events', '") + std::to_string(eventCnt) + "')";
        mDb->exec(str);
    }

    // Commit transaction
    transaction.commit();

    {
        delete insertGameStatement;
        delete playerGetIdStatement;
        delete playerInsertStatement;

        delete eventGetIdStatement;
        delete eventInsertStatement;
    }

    printStats();

    std::cout << "Completed! " << std::endl;
    return gameCnt;
}


SQLite::Database* Builder::createDb(const std::string& path)
{
    assert(!path.empty());

    try
    {
        // Open a database file in create/write mode
        auto mDb = new SQLite::Database(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
        std::cout << "SQLite database file '" << mDb->getFilename() << "' opened successfully\n";

        mDb->exec("DROP TABLE IF EXISTS info");
        mDb->exec("CREATE TABLE info (name TEXT UNIQUE NOT NULL, value TEXT)");
        mDb->exec("INSERT INTO info(name, value) VALUES ('version', '0.1')");
        mDb->exec("INSERT INTO info(name, value) VALUES ('variant', 'standard')");
        mDb->exec("INSERT INTO info(name, value) VALUES ('license', 'free')");

        mDb->exec("DROP TABLE IF EXISTS event");
        mDb->exec("CREATE TABLE event (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)");
        mDb->exec("INSERT INTO event(name) VALUES (\"\")"); // default empty

        mDb->exec("DROP TABLE IF EXISTS player");
        mDb->exec("CREATE TABLE player (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, elo INTEGER)");

        mDb->exec("DROP TABLE IF EXISTS game");
        mDb->exec("CREATE TABLE game(id INTEGER PRIMARY KEY AUTOINCREMENT, Event_id INTEGER, White_id INTEGER, WhiteElo INTEGER, Black_id INTEGER, BlackElo INTEGER, Timer TEXT, Date TEXT, ECO TEXT, Result INTEGER, PlyCount INTEGER, FEN TEXT, Moves TEXT, FOREIGN KEY(Event_id) REFERENCES event, FOREIGN KEY(White_id) REFERENCES player, FOREIGN KEY(Black_id) REFERENCES player)");
        
        return mDb;
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
    }

    return nullptr;
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

void Builder::queryGameData(SQLite::Database& db, int gameIdx)
{
    std::string str =
    "SELECT g.id, w.name White, WhiteElo, b.name Black, BlackElo, Timer, Date, Result, ECO, PlyCount, FEN, Moves " \
    "FROM game g " \
    "INNER JOIN player w ON White_id = w.id " \
    "INNER JOIN player b ON Black_id = b.id " \
    "WHERE g.id = " + std::to_string(gameIdx);
    
    SQLite::Statement query(db, str);

    auto ok = false;
    if (query.executeStep()) {
        const int         id     = query.getColumn("id");
        const std::string white  = query.getColumn("White");
        const std::string black  = query.getColumn("Black");
        const std::string fen  = query.getColumn("FEN");
        const std::string moves  = query.getColumn("Moves");
        const int plyCount  = query.getColumn("PlyCount");

        ok = id == gameIdx && !white.empty() && !black.empty()
            && (!fen.empty() || !moves.empty())
            && ((plyCount == 0 && moves.empty()) || (plyCount > 0 && !moves.empty()));
        
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


int Builder::getPlayerNameId(const std::string& name, int elo)
{
    if (name.empty()) {
        return -1;
    }
    playerGetIdStatement->bind(1, name);
    int playerId = -1;
    
    if (playerGetIdStatement->executeStep()) {
        playerId = playerGetIdStatement->getColumn(0);
    }
    playerGetIdStatement->reset();
    
    if (playerId < 0) {
        playerInsertStatement->bind(1, name);
        playerInsertStatement->bind(2, elo);
        if (playerInsertStatement->executeStep()) {
            playerId = playerInsertStatement->getColumn(0).getInt();
        }
        playerInsertStatement->reset();
    }

    assert(playerId >= 0);
    return playerId;
}

int Builder::getEventNameId(const std::string& name)
{
    if (name.empty()) {
        return 0;
    }
    eventGetIdStatement->bind(1, name);
    int eventId = -1;
    
    if (eventGetIdStatement->executeStep()) {
        eventId = eventGetIdStatement->getColumn(0);
    }
    eventGetIdStatement->reset();
    
    if (eventId < 0) {
        eventInsertStatement->bind(1, name);
        if (eventInsertStatement->executeStep()) {
            eventId = eventInsertStatement->getColumn(0).getInt();
        }
        eventInsertStatement->reset();
    }

    assert(eventId >= 0);
    return eventId;
}

bool Builder::addGame(const std::unordered_map<std::string, const char*>& itemMap, const char* moveText)
{
    assert(strlen(moveText) < 32 * 1024); // hard coding, just for quickly detecting incorrect data

    if (itemMap.size() < 3) {
        return false;
    }

    GameRecord r;

    auto it = itemMap.find("Event");
    if (it == itemMap.end())
        return false;
    r.eventName = it->second;

    it = itemMap.find("Variant");
    if (it != itemMap.end()) {
        auto variant = Funcs::string2ChessVariant(it->second);
        // at this moment, support only the standard variant
        if (variant != ChessVariant::standard) {
            return false;
        }
    }

    it = itemMap.find("White");
    if (it == itemMap.end())
        return false;
    r.whiteName = it->second;

    it = itemMap.find("WhiteElo");
    if (it != itemMap.end()) {
        r.whiteElo = std::atoi(it->second);
    }

    it = itemMap.find("Black");
    if (it == itemMap.end())
        return false;
    r.blackName = it->second;

    it = itemMap.find("BlackElo");
    if (it != itemMap.end()) {
        r.blackElo = std::atoi(it->second);
    }

    it = itemMap.find("Date");
    if (it != itemMap.end()) {
        r.dateString = it->second;
    }

    it = itemMap.find("UTCDate");
    if (it != itemMap.end()) {
        r.dateString = it->second;
    }

    it = itemMap.find("TimeControl");
    if (it != itemMap.end()) {
        r.timer = it->second;
    }

    it = itemMap.find("ECO");
    if (it != itemMap.end()) {
        r.eco = it->second;
    }

    it = itemMap.find("Result");
    if (it != itemMap.end()) {
        r.resultType = Funcs::string2ResultType(it->second);
    }

    r.moveString = moveText;

    it = itemMap.find("PlyCount");
    if (it != itemMap.end()) {
        r.plyCount = std::atoi(it->second);
    }

    return addGame(r);
}

bool Builder::addGame(const GameRecord& r)
{
    try
    {
        assert(mDb);

        auto eventId = getEventNameId(r.eventName);
        auto whiteId = getPlayerNameId(r.whiteName, r.whiteElo);
        auto blackId = getPlayerNameId(r.blackName, r.blackElo);

        insertGameStatement->bind(1, eventId);
        insertGameStatement->bind(2, whiteId);
        
        // use null if it is zero
        if (r.whiteElo > 0) {
            insertGameStatement->bind(3, r.whiteElo);
        }

        insertGameStatement->bind(4, blackId);
        
        if (r.blackElo > 0) {
            insertGameStatement->bind(5, r.blackElo);
        }

        if (r.timer) {
            insertGameStatement->bind(6, r.timer);
        }
        insertGameStatement->bind(7, Funcs::resultType2String(r.resultType, false));

        if (r.dateString) {
            insertGameStatement->bind(8, r.dateString);
        }
        
        if (r.eco) {
            insertGameStatement->bind(9, r.eco);
        }
        
        if (r.plyCount > 0) {
            insertGameStatement->bind(10, r.plyCount);
        }
        
        if (r.fen) {
            insertGameStatement->bind(11, r.fen);
        }
        insertGameStatement->bind(12, r.moveString);

        insertGameStatement->executeStep();
        insertGameStatement->reset();
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        return false;
    }

    return true;
}
