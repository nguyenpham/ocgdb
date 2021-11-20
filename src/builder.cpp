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
    std::cout << "#games: " << gameCnt
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
    
//    std::unordered_map<std::string, const char*> tagMap;
    std::unordered_map<const char*, const char*> tagMap;

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
        const std::string sql = "INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        insertGameStatement = new SQLite::Statement(*mDb, sql);
        
        playerGetIdStatement = new SQLite::Statement(*mDb, "SELECT ID FROM Players WHERE Name=?");
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (Name, Elo) VALUES (?, ?) RETURNING ID");

        eventGetIdStatement = new SQLite::Statement(*mDb, "SELECT ID FROM Events WHERE Name=?");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (Name) VALUES (?) RETURNING ID");

        siteGetIdStatement = new SQLite::Statement(*mDb, "SELECT ID FROM Sites WHERE Name=?");
        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (Name) VALUES (?) RETURNING ID");
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
        auto str = std::string("INSERT INTO Info (Name, Value) VALUES ('GameCount', '") + std::to_string(gameCnt) + "')";
        mDb->exec(str);

        int playerCnt = mDb->execAndGet("SELECT COUNT(*) FROM Players");
        str = std::string("INSERT INTO Info (Name, Value) VALUES ('PlayerCount', '") + std::to_string(playerCnt) + "')";
        mDb->exec(str);

        int eventCnt = mDb->execAndGet("SELECT COUNT(*) FROM Events");
        str = std::string("INSERT INTO Info (Name, Value) VALUES ('EventCount', '") + std::to_string(eventCnt) + "')";
        mDb->exec(str);

        int siteCnt = mDb->execAndGet("SELECT COUNT(*) FROM Sites");
        str = std::string("INSERT INTO Info (Name, Value) VALUES ('SiteCount', '") + std::to_string(siteCnt) + "')";
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

        delete siteGetIdStatement;
        delete siteInsertStatement;
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

        mDb->exec("DROP TABLE IF EXISTS Info");
        mDb->exec("CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT)");
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Version', '0.1')");
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Variant', 'standard')");
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('License', 'free')");

        mDb->exec("DROP TABLE IF EXISTS Events");
        mDb->exec("CREATE TABLE Events (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE)");
        mDb->exec("INSERT INTO Events (Name) VALUES (\"\")"); // default empty

        mDb->exec("DROP TABLE IF EXISTS Sites");
        mDb->exec("CREATE TABLE Sites (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE)");
        mDb->exec("INSERT INTO Sites (Name) VALUES (\"\")"); // default empty

        mDb->exec("DROP TABLE IF EXISTS Players");
        mDb->exec("CREATE TABLE Players (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE, Elo INTEGER)");

        mDb->exec("DROP TABLE IF EXISTS Games");
        mDb->exec("CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, Moves TEXT, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)");
        
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

int Builder::getPlayerNameId(const char* name, int elo)
{
    if (!name) {
        return -1;
    }
    
    // trim left
    while(*name && *name <= ' ') name++;

    // empty
    if (*name == 0) return -1;

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

int Builder::getEventNameId(const char* name)
{
    if (!name) {
        return 1;
    }
    
    // trim left
    while(*name && *name <= ' ') name++;

    // empty
    if (*name == 0) return 1;

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

int Builder::getSiteNameId(const char* name)
{
    if (!name) {
        return 1;
    }
    
    // trim left
    while(*name && *name <= ' ') name++;

    // empty
    if (*name == 0) return 1;
    
    siteGetIdStatement->bind(1, name);
    int siteId = -1;
    
    if (siteGetIdStatement->executeStep()) {
        siteId = siteGetIdStatement->getColumn(0);
    }
    siteGetIdStatement->reset();
    
    if (siteId < 0) {
        siteInsertStatement->bind(1, name);
        if (siteInsertStatement->executeStep()) {
            siteId = siteInsertStatement->getColumn(0).getInt();
        }
        siteInsertStatement->reset();
    }

    assert(siteId >= 0);
    return siteId;
}

const char* tagNames[] = {
    "Event", "Site", "Date", "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount", "FEN",
    nullptr, nullptr
};

enum {
    TagIdx_Event, TagIdx_Site, TagIdx_Date, TagIdx_Round,
    TagIdx_White, TagIdx_WhiteElo, TagIdx_Black, TagIdx_BlackElo,
    TagIdx_Result, TagIdx_Timer, TagIdx_ECO, TagIdx_PlyCount, TagIdx_FEN,
    TagIdx_Moves,
    TagIdx_Max
};

bool Builder::addGame(const std::unordered_map<const char*, const char*>& itemMap, const char* moveText)
{
    if (itemMap.size() < 3) {
        return false;
    }
    
    try {
        insertGameStatement->reset();

        auto eventId = -1, whiteElo = 0, blackElo = 0;
        const char* whiteName = nullptr, *blackName = nullptr, *date = nullptr;
        
        for(auto && it : itemMap) {
            for(int i = 0; tagNames[i]; i++) {
                if (strcmp(it.first, tagNames[i]) != 0) {
                    if (strcmp(it.first, "Variant") == 0) {
                        auto variant = Funcs::string2ChessVariant(it.second);
                        // at this moment, support only the standard variant
                        if (variant != ChessVariant::standard) {
                            return false;
                        }
                    }
                    if (strcmp(it.first, "UTCDate") == 0 && !date) {
                        date = it.second;
                    }
                    continue;
                }

                switch (i) {
                    case TagIdx_Event:
                    {
                        eventId = getEventNameId(it.second);
                        insertGameStatement->bind(i + 1, eventId);
                        break;
                    }
                    case TagIdx_Site:
                    {
                        auto siteId = getSiteNameId(it.second);
                        insertGameStatement->bind(i + 1, siteId);
                        break;
                    }
                    case TagIdx_White:
                    {
                        whiteName = it.second;
                        break;
                    }
                    case TagIdx_Black:
                    {
                        blackName = it.second;
                        break;
                    }

                    case TagIdx_Date:
                    {
                        date = it.second;
                        break;
                    }
                    case TagIdx_Result:
                    case TagIdx_Timer:
                    case TagIdx_ECO:
                    case TagIdx_FEN:
                    {
                        insertGameStatement->bind(i + 1, it.second);
                        break;
                    }
                    case TagIdx_WhiteElo:
                    {
                        whiteElo = std::atoi(it.second);
                        insertGameStatement->bind(i + 1, whiteElo);
                        break;
                    }
                    case TagIdx_BlackElo:
                    {
                        blackElo = std::atoi(it.second);
                        insertGameStatement->bind(i + 1, blackElo);
                        break;
                    }
                    case TagIdx_Round:
                    case TagIdx_PlyCount:
                    {
                        insertGameStatement->bind(i + 1, std::atoi(it.second));
                        break;
                    }

                        
                    default:
                        assert(false);
                        break;
                }
                break;
            }
        }
        
        if (eventId < 0 || !whiteName || !blackName) {
            return false;
        }
        
        if (date) {
            insertGameStatement->bind(TagIdx_Date + 1, date);
        }

        auto whiteId = getPlayerNameId(whiteName, whiteElo);
        auto blackId = getPlayerNameId(blackName, blackElo);
        insertGameStatement->bind(TagIdx_Black + 1, whiteId);
        insertGameStatement->bind(TagIdx_White + 1, blackId);

        // trim left
        while(*moveText <= ' ') moveText++;
        insertGameStatement->bind(TagIdx_Moves + 1, moveText);

        insertGameStatement->executeStep();
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void Builder::queryGameData(SQLite::Database& db, int gameIdx)
{
    auto ok = false;

    benchStatement->reset();
    benchStatement->bind(1, gameIdx);

    if (benchStatement->executeStep()) {
        const int id = benchStatement->getColumn("ID");
        const std::string white  = benchStatement->getColumn("White");
        const std::string black  = benchStatement->getColumn("Black");
        const std::string fen  = benchStatement->getColumn("FEN");
        const std::string moves  = benchStatement->getColumn("Moves");
        const int plyCount = benchStatement->getColumn("PlyCount");

        ok = id == gameIdx && !white.empty() && !black.empty()
            && (!fen.empty() || !moves.empty())
            && plyCount >= 0;
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

    auto query = "SELECT COUNT(id) FROM Games";
    int gameCnt = db.execAndGet(query);
    
    std::cout << "GameCount: " << gameCnt << std::endl;
    
    if (gameCnt < 1) {
        return;
    }

    {
        std::string str =
        "SELECT g.ID, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves " \
        "FROM Games g " \
        "INNER JOIN Players w ON WhiteID = w.ID " \
        "INNER JOIN Players b ON BlackID = b.ID " \
        "WHERE g.ID = ?";

        benchStatement = new SQLite::Statement(db, str);
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

    delete benchStatement;
    
    std::cout << "Test DONE." << std::endl;
}
