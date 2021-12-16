/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 Developers
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

Builder* builder = nullptr;

void ThreadRecord::init(SQLite::Database* mDb)
{
    if (board) return;
    
    errCnt = 0;
    
    assert(mDb);
    board = Builder::createBoard(bslib::ChessVariant::standard);

    const std::string sql = "INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves, ID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    insertGameStatement = new SQLite::Statement(*mDb, sql);    
}

ThreadRecord::~ThreadRecord()
{
    delete board;
    delete insertGameStatement;
}

Builder::Builder()
{
    builder = this;
}

Builder::~Builder()
{
    // delete all statements
    threadMap.clear();
    if (mDb) delete mDb;
}

std::chrono::steady_clock::time_point getNow()
{
    return std::chrono::steady_clock::now();
}

void Builder::printStats() const
{
    int64_t eCnt = errCnt;
    for(auto && t : threadMap) {
        eCnt += t.second.errCnt;
    }

    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    std::cout << "#games: " << gameCnt
              << ", #errors: " << errCnt
              << ", #hashKeys: " << hashMap.size()
              << ", #posCnt: " << posCnt
              << ", elapsed: " << elapsed << "ms "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << gameCnt * 1000ULL / elapsed << " games/s"
              << std::endl;
}

bslib::BoardCore* Builder::createBoard(bslib::ChessVariant variant)
{
    return variant == bslib::ChessVariant::standard ? new bslib::ChessBoard : nullptr;
}


void Builder::saveHashKey(bslib::BoardCore* board, int gameID)
{
    assert(board);
    std::lock_guard<std::mutex> dolock(hashMutex);

    int sz = board->getHistListSize();
    posCnt += sz;
    
    for(int i = 0; i < sz; ++i) {
        auto hashKey = board->_getHistAt(i).hashKey;
        auto r = &hashMap[hashKey];
        r->gameIdVec.push_back(gameID);
    }

    // last one
    {
        auto r = &hashMap[board->hashKey];
        r->gameIdVec.push_back(gameID);
    }
}

void Builder::convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath, int cpu)
{
    // Prepare
    setDatabasePath(sqlitePath);
    
    // Create database
    mDb = createDb(sqlitePath);
    if (!mDb) {
        return;
    }

    // init
    {
        startTime = getNow();
        gameCnt = 0;
        eventCnt = playerCnt = siteCnt = 1;
        errCnt = posCnt = 0;

        if (cpu < 0) cpu = std::thread::hardware_concurrency();
        pool = new thread_pool(cpu);
        std::cout << "Thread count: " << pool->get_thread_count() << std::endl;

        playerIdMap.reserve(1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);
        
        hashMap.reserve(64 * 1024 * 1024);
        
        // prepared statements
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");
        
#ifdef _ONE_HASH_TABLE_
        insertHashStatement_blob = new SQLite::Statement(*mDb, "INSERT INTO HashPositions (HashKey, MultiGameIDs) VALUES (?, ?)");
        insertHashStatement_one = new SQLite::Statement(*mDb, "INSERT INTO HashPositions (HashKey, GameID) VALUES (?, ?)");
#else
        insertHashStatement_blob = new SQLite::Statement(*mDb, "INSERT INTO HashPositionsBlob (HashKey, MultiGameIDs) VALUES (?, ?)");
        insertHashStatement_one = new SQLite::Statement(*mDb, "INSERT INTO HashPositionsOne (HashKey, GameID) VALUES (?, ?)");
#endif
    }

    processPgnFile(pgnPath);
    
    // completing
    {
        auto str = std::string("INSERT INTO Info (Name, Value) VALUES ('GameCount', '") + std::to_string(gameCnt) + "')";
        mDb->exec(str);

        str = std::string("INSERT INTO Info (Name, Value) VALUES ('PlayerCount', '") + std::to_string(playerCnt) + "')";
        mDb->exec(str);

        str = std::string("INSERT INTO Info (Name, Value) VALUES ('EventCount', '") + std::to_string(eventCnt) + "')";
        mDb->exec(str);

        int siteCnt = mDb->execAndGet("SELECT COUNT(*) FROM Sites");
        str = std::string("INSERT INTO Info (Name, Value) VALUES ('SiteCount', '") + std::to_string(siteCnt) + "')";
        mDb->exec(str);

        delete playerInsertStatement;
        delete eventInsertStatement;
        delete siteInsertStatement;
        
        delete insertHashStatement_blob;
        delete insertHashStatement_one;

//        hashMap.clear();
    }

    std::cout << "Completed! " << std::endl;
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
    
    std::unordered_map<char*, char*> tagMap;

    auto evtCnt = 0;
    auto hasEvent = false;
    char *tagName = nullptr, *tagContent = nullptr, *event = nullptr, *moves = nullptr;

    enum class ParsingState {
        none, tagName, tag_after, tag_content, tag_content_after
    };
    
    auto st = ParsingState::none;
    
    for(char *p = buffer, *end = buffer + sz; p < end; p++) {
        char ch = *p;
        
        switch (st) {
            case ParsingState::none:
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

                            threadAddGame(tagMap, moves);
                        }

                        tagMap.clear();
                        hasEvent = false;
                        moves = nullptr;
                    }

                    tagName = p;
                    st = ParsingState::tagName;
                } else if (ch > ' ') {
                    if (!moves && hasEvent) {
                        moves = p;
                    }
                }
                break;
            }
            case ParsingState::tagName: // name tag
            {
                assert(tagName);
                if (!isalpha(ch)) {
                    if (ch <= ' ') {
                        *p = 0; // end of the tag name
                        st = ParsingState::tag_after;
                    } else { // something wrong
                        st = ParsingState::none;
                    }
                }
                break;
            }
            case ParsingState::tag_after: // between name and content of a tag
            {
                if (ch == '"') {
                    st = ParsingState::tag_content;
                    tagContent = p + 1;
                }
                break;
            }
            case ParsingState::tag_content:
            {
                if (ch == '"' || ch == 0) { // == 0 trick to process half begin+end
                    *p = 0;
                    
                    if (strcmp(tagName, "Event") == 0) {
                        event = tagName - 1;
                        if (evtCnt == 0 && connectBlock) {
                            long len =  (event - buffer) - 1;
                            processHalfEnd(buffer, len);
                        }
                        hasEvent = true;
                        evtCnt++;
                    }

                    if (hasEvent) {
                        tagMap[tagName] = tagContent;
                    }

                    tagName = tagContent = nullptr;
                    st = ParsingState::tag_content_after;
                }
                break;
            }
            default: // the rest of the tag
            {
                if (ch == '\n' || ch == 0) {
                    st = ParsingState::none;
                }
                break;
            }
        }
    }
    
    if (connectBlock) {
        processHalfBegin(event, (long)sz - (event - buffer));
    } else if (moves) {
        threadAddGame(tagMap, moves);
    }
}

uint64_t Builder::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;
    
    // Begin transaction
    SQLite::Transaction transaction(*mDb);

    {
        char *buffer = (char*)malloc(blockSz + 16);

        FILE *stream = fopen(path.c_str(), "r");
        assert(stream != NULL);
        auto size = bslib::Funcs::getFileSize(stream);
        
        for (size_t sz = 0, idx = 0; sz < size; idx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (fread(buffer, k, 1, stream)) {
                processDataBlock(buffer, k, true);
                pool->wait_for_tasks();

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
                pool->wait_for_tasks();
            }
            
            free(halfBuf);
            halfBuf = 0;
        }
    }

    // Commit transaction
    transaction.commit();

    writeHashTable();

    printStats();

    return gameCnt;
}


void Builder::writeHashTable()
{
    std::cout << "writing hash... size: "  << hashMap.size() << std::endl;
    auto startTime = getNow();

    int64_t cnt = 0, blobCnt = 0, oneCnt = 0;
    
    {
        std::cout   << "* updating hash keys with multi games (using blob)..." << std::endl;
        SQLite::Transaction hashTransaction(*mDb);
        for(auto && h : hashMap) {
            auto n = h.second.gameIdVec.size();
            if (n <= 1) {
                continue;
            }

            cnt++;

            auto hashKey = static_cast<int64_t>(h.first);
            posCnt += n;

            try {
                blobCnt++;
                auto sz = static_cast<int>(n * sizeof(int));

                insertHashStatement_blob->reset();
                insertHashStatement_blob->bind(1, hashKey);
                insertHashStatement_blob->bind(2, h.second.gameIdVec.data(), sz);
                insertHashStatement_blob->exec();

                h.second.gameIdVec.clear();
            } catch (std::exception& e) {
                std::cout << "saveHashKey, insertHashStatement, SQLite exception: " << e.what() << std::endl;
                errCnt++;
            }
            
            if ((cnt & 0x1fffff) == 0) {
                int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count();
                
                std::cout   << "  updating..., cnt: " << cnt << ", one: " << oneCnt << ", blob: " << blobCnt
                            << ", elapsed: " << elapsed << "ms "
                            << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                            << ", speed: " << cnt * 1000 / elapsed << " hash keys/s"
                            << std::endl;
            }
        }
        
        hashTransaction.commit();
    }

    {
        std::cout   << "* updating hash keys with one game each only..." << std::endl;
        SQLite::Transaction hashTransaction(*mDb);
        for(auto && h : hashMap) {
            auto n = h.second.gameIdVec.size();
            if (n != 1) {
                continue;
            }

            cnt++;

            auto hashKey = static_cast<int64_t>(h.first);
            posCnt += n;

            try {
                oneCnt++;
                insertHashStatement_one->reset();
                insertHashStatement_one->bind(1, hashKey);
                insertHashStatement_one->bind(2, h.second.gameIdVec.at(0));
                insertHashStatement_one->exec();

                h.second.gameIdVec.clear();
            } catch (std::exception& e) {
                std::cout << "saveHashKey, insertHashStatement, SQLite exception: " << e.what() << std::endl;
                errCnt++;
            }
            
            if ((cnt & 0x1fffff) == 0) {
                int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count();
                
                std::cout   << "  updating..., cnt: " << cnt << ", one: " << oneCnt << ", blob: " << blobCnt
                            << ", elapsed: " << elapsed << "ms "
                            << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                            << ", speed: " << cnt * 1000 / elapsed << " hash keys/s"
                            << std::endl;
            }
        }
        
        hashTransaction.commit();
    }
    
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count();
    
    std::cout   << "Completed writing hash keys, total: " << hashMap.size()
                << ", one: " << oneCnt << ", blob: " << blobCnt
                << "%, elapsed: " << elapsed << "ms "
                << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                << ", speed: " << hashMap.size() * 1000ULL / elapsed << " hash keys/s"
                << std::endl;

//    hashMap.clear();
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
        mDb->exec("CREATE TABLE Players (ID INTEGER PRIMARY KEY, Name TEXT UNIQUE, Elo INTEGER)");
        mDb->exec("INSERT INTO Players (ID, Name) VALUES (1, \"\")"); // default empty

        mDb->exec("DROP TABLE IF EXISTS Games");
        mDb->exec("CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, Moves TEXT, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)");
        
#ifdef _ONE_HASH_TABLE_
        mDb->exec("DROP TABLE IF EXISTS HashPositions");
        mDb->exec("CREATE TABLE HashPositions (HashKey INTEGER, GameID INTEGER DEFAULT NULL, MultiGameIDs BLOB DEFAULT NULL)");
#else
        mDb->exec("DROP TABLE IF EXISTS HashPositionsBlob");
        mDb->exec("CREATE TABLE HashPositionsBlob (HashKey INTEGER, MultiGameIDs BLOB)");

        mDb->exec("DROP TABLE IF EXISTS HashPositionsOne");
        mDb->exec("CREATE TABLE HashPositionsOne (HashKey INTEGER, GameID INTEGER )");
#endif
        
        mDb->exec("PRAGMA synchronous=OFF");
        mDb->exec("PRAGMA journal_mode=OFF");
        mDb->exec("PRAGMA cache_size=64000");

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
    return bslib::Funcs::replaceString(str, "\"", "\\\"");
}

int Builder::getPlayerNameId(char* name, int elo)
{
    std::lock_guard<std::mutex> dolock(playerMutex);
    return getNameId(name, elo, playerCnt, playerInsertStatement, playerIdMap);
}

int Builder::getEventNameId(char* name)
{
    std::lock_guard<std::mutex> dolock(eventMutex);
    return getNameId(name, -1, eventCnt, eventInsertStatement, eventIdMap);
}

int Builder::getNameId(char* name, int elo, int& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, int>& idMap)
{
    name = bslib::Funcs::trim(name);

    // null, empty or ?
    if (!name || *name == 0 || *name == '?') return 1;

    auto s = std::string(name);
    bslib::Funcs::toLower(s);
    auto it = idMap.find(s);
    if (it != idMap.end()) {
        return it->second;
    }

    auto theId = ++cnt;

    insertStatement->reset();
    insertStatement->bind(1, theId);
    insertStatement->bind(2, name);
    
    if (elo > 0) {
        insertStatement->bind(3, elo);
    }

    if (insertStatement->exec() != 1) {
        return -1; // something wrong
    }

    idMap[s] = theId;

    assert(theId > 1);
    return theId;
}

int Builder::getSiteNameId(char* name)
{
    std::lock_guard<std::mutex> dolock(siteMutex);
    return getNameId(name, -1, siteCnt, siteInsertStatement, siteIdMap);
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
    TagIdx_Result, TagIdx_Timer, TagIdx_ECO, TagIdx_PlyCount,
    TagIdx_FEN, TagIdx_Moves,
    TagIdx_GameID,
    TagIdx_Max
};

void doAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(builder);
    builder->addGame(itemMap, moveText);
}

void Builder::threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    pool->submit(doAddGame, itemMap, moveText);
}

bool Builder::addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    auto threadId = std::this_thread::get_id();
    auto t = &threadMap[threadId];
    t->init(mDb);
    assert(t->board);

    if (itemMap.size() < 3) {
        t->errCnt++;
        return false;
    }
    
    try {
        t->insertGameStatement->reset();

        auto eventId = 1, whiteElo = 0, blackElo = 0, plyCount = 0;
        char* whiteName = nullptr, *blackName = nullptr, *date = nullptr;
        std::string fenString;

        for(auto && it : itemMap) {
            for(auto i = 0; tagNames[i]; i++) {
                if (strcmp(it.first, tagNames[i]) != 0) {
                    if (strcmp(it.first, "Variant") == 0) {
                        auto variant = bslib::Funcs::string2ChessVariant(it.second);
                        // at this moment, support only the standard variant
                        if (variant != bslib::ChessVariant::standard) {
                            t->errCnt++;
                            return false;
                        }
                    }
                    if (strcmp(it.first, "UTCDate") == 0 && !date) {
                        date = it.second;
                    }
                    continue;
                }

                auto s = it.second;
                while(*s <= ' ' && *s > 0) s++; // trim left
                assert(strlen(s) < 1024);
                                
                switch (i) {
                    case TagIdx_Event:
                    {
                        eventId = getEventNameId(s);
                        break;
                    }
                    case TagIdx_Site:
                    {
                        auto siteId = getSiteNameId(s);
                        t->insertGameStatement->bind(i + 1, siteId);
                        break;
                    }
                    case TagIdx_White:
                    {
                        whiteName = s;
                        break;
                    }
                    case TagIdx_Black:
                    {
                        blackName = s;
                        break;
                    }

                    case TagIdx_Date:
                    {
                        date = s;
                        break;
                    }

                    case TagIdx_FEN:
                        fenString = s;
                    case TagIdx_Result:
                    case TagIdx_Timer:
                    case TagIdx_ECO:
                    {
                        if (s[0]) { // ignored empty, use NULL instead
                            t->insertGameStatement->bind(i + 1, s);
                        }
                        break;
                    }
                    case TagIdx_WhiteElo:
                    {
                        whiteElo = std::atoi(s);
                        if (whiteElo > 0) {
                            t->insertGameStatement->bind(i + 1, whiteElo);
                        }
                        break;
                    }
                    case TagIdx_BlackElo:
                    {
                        blackElo = std::atoi(s);
                        if (blackElo > 0) {
                            t->insertGameStatement->bind(i + 1, blackElo);
                        }
                        break;
                    }
                    case TagIdx_PlyCount:
                    {
                        plyCount = std::atoi(s);
                        break;
                    }
                    case TagIdx_Round:
                    {
                        auto k = std::atoi(s);
                        t->insertGameStatement->bind(i + 1, k);
                        break;
                    }

                    default:
                        assert(false);
                        break;
                }
                break;
            }
        }
        
        auto whiteId = getPlayerNameId(whiteName, whiteElo);
        auto blackId = getPlayerNameId(blackName, blackElo);
        t->insertGameStatement->bind(TagIdx_Black + 1, whiteId);
        t->insertGameStatement->bind(TagIdx_White + 1, blackId);

        t->insertGameStatement->bind(TagIdx_Event + 1, eventId);

        if (date) {
            t->insertGameStatement->bind(TagIdx_Date + 1, date);
        }

        // trim left
        while(*moveText <= ' ') moveText++;
        t->insertGameStatement->bind(TagIdx_Moves + 1, moveText);

        int gameID;
        {
            std::lock_guard<std::mutex> dolock(gameMutex);
            ++gameCnt;
            gameID = gameCnt;
        }
        t->insertGameStatement->bind(TagIdx_GameID + 1, gameID);


        // Parse moves
        {
            //assert(t->board);
            t->board->newGame(fenString);
            t->board->fromMoveList(moveText, bslib::Notation::san, false);

            plyCount = t->board->getHistListSize();
            saveHashKey(t->board, gameID);
        }

        if (plyCount > 0) {
            t->insertGameStatement->bind(TagIdx_PlyCount + 1, plyCount);
        }

        t->insertGameStatement->exec();
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << std::endl;
        t->errCnt++;
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
                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                  << ", speed: " << total * 1000 / elapsed << " query/s"
                  << std::endl;
    }

    delete benchStatement;
    
    std::cout << "Test DONE." << std::endl;
}
    
static void inResultSet(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc == 1) {
        if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER ){
            auto gameID = sqlite3_value_int(argv[0]);
            auto isIn = builder->gameIdSet.find(gameID) != builder->gameIdSet.end();
//            std::cout << "inResultSet, gameID: " << gameID << ", isIn: " << isIn << std::endl;
            sqlite3_result_int(context, isIn);
            return;
        }
        sqlite3_result_error(context, "invalid argument", -1);
    } else {
        sqlite3_result_null(context);
    }
}

static void makehash(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc == 1) {
        if (sqlite3_value_type(argv[0]) == SQLITE_TEXT ){
            auto text = (const char*)sqlite3_value_text(argv[0]);
            if (text && text[0]) {
                bslib::ChessBoard board;
                board.newGame(text);
                auto hashKey = static_cast<int64_t>(board.hashKey);
                std::cout << "makehash, FEN: " << text << ", hashKey: " << hashKey << std::endl;
                sqlite3_result_int64(context, hashKey);
                return;
            }
        }
        sqlite3_result_error(context, "invalid argument", -1);
    } else {
        sqlite3_result_null(context);
    }
}

int64_t makehash(const std::string& fenString)
{
    bslib::ChessBoard board;
    board.newGame(fenString);
    return static_cast<int64_t>(board.hashKey);
}

void Builder::benchMatchingMoves(const std::string& dbPath)
{
    const std::vector<std::string> gameBodyVec {
"1.d4 Nf6 2.Nf3 d5 3.e3 Bf5 4.c4 c6 5.Nc3 e6 6.Bd3 Bxd3 7.Qxd3 Nbd7 8.b3 Bd6 \
9.O-O O-O 10.Bb2 Qe7 11.Rad1 Rad8 12.Rfe1 dxc4 13.bxc4 e5 14.dxe5 Nxe5 15.Nxe5 Bxe5 \
16.Qe2 Rxd1 17.Rxd1 Rd8 18.Rxd8+ Qxd8 19.Qd1 Qxd1+ 20.Nxd1 Bxb2 21.Nxb2 b5 \
22.f3 Kf8 23.Kf2 Ke7  1/2-1/2",

"1.e4 e5 2.Nf3 Nf6 3.d3 d6 4.c3 g6 5.Bg5 Bg7 6.Be2 O-O 7.d4 h6 8.Bxf6 Qxf6 \
9.dxe5 dxe5 10.O-O Nd7 11.Nbd2 Nc5 12.Qc2 Bg4 13.Rfe1 Rfe8 14.b4 Ne6 15.h3 Bxf3 \
16.Nxf3 Nf4 17.Bc4 Nxh3+ 18.gxh3 Qxf3 19.Re3 Qh5 20.Qb3 Rf8 21.Rd1 Qg5+ 22.Rg3 Qf6 \
23.Rdd3 g5 24.Rgf3 Qe7 25.Rf5 Kh8 26.Bxf7 Rad8 27.Rdf3 Rd6 28.Kf1 Qd8 29.Bh5 Rxf5 \
30.Rxf5 Rc6 31.Rf3 Rd6 32.Rf5 Rd2 33.Rf7 Qd3+ 34.Kg2 Qxe4+ 35.Bf3 Qh4 36.Bxb7 g4 \
37.Qe6 Qxh3+ 38.Kg1 Rd1+  0-1",

"1.e4 e5 2.Nf3 Nf6 3.Nc3 Nc6 4.Bc4 Bb4 5.d3 d5 6.exd5 Nxd5 7.Bd2 Nxc3 8.bxc3 Be7 \
9.Qe2 Bf6 10.O-O O-O 11.Rfe1 Re8 12.Qe4 g6 13.g4 Na5 14.Bb3 Nxb3 15.axb3 Bd7 \
16.g5 Bc6 17.Qh4 Bg7 18.Qg4 Qd6 19.Re2 Re6 20.Rae1 Rae8 21.c4 b6 22.Bc1 Ba8 \
23.Nd2 Bc6 24.Ne4 Qe7 25.Re3 h5 26.Qg3 Bd7 27.Bb2 Bc6 28.Nf6+ Bxf6 29.gxf6 Qxf6 \
30.Rxe5 Rxe5 31.Rxe5 Rd8 32.Qe3 Qh4 33.Qg5 Qxg5+ 34.Rxg5 Re8 35.Kf1 Bf3 36.Re5 Rd8 \
37.Ke1 Kf8 38.Ba3+ Kg8 39.Re7 Rc8 40.Kd2 h4 41.Ke3 Bh5 42.Kd2 g5 43.Rd7 g4 \
44.Re7 Kg7 45.Ke3 Kf6 46.d4 c5 47.Rxa7 Re8+ 48.Kd2 cxd4 49.Rd7 g3 50.fxg3 Re2+ \
51.Kd3 Rxh2 52.gxh4 Bg6+ 53.Kxd4 Rd2+  0-1 ",

"1.c4 c5 2.g3 Nc6 3.Bg2 g6 4.Nc3 Bg7 5.a3 d6 6.Rb1 a5 7.Nf3 Nf6 8.O-O O-O \
9.Ne1 Bd7 10.Nc2 Rb8 11.d3 Ne8 12.Bd2  1/2-1/2"
    };

    std::cout << "Benchmark matching moves dbPath: " << dbPath << std::endl;

    SQLite::Database db(dbPath, SQLite::OPEN_READWRITE);
    {
        std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

        db.createFunction("makehash", 1, true, nullptr, &makehash, nullptr, nullptr, nullptr);
//        sqlite3_create_function(db, "firstchar", 1, SQLITE_UTF8, NULL, &firstchar, NULL, NULL)
//        EXPECT_EQ(1, db.exec("SELECT makehash(value) FROM test WHERE id=1"));

        db.createFunction("inResultSet", 1, true, nullptr, &inResultSet, nullptr, nullptr, nullptr);

//        int64_t hashKey = db.execAndGet("SELECT * FROM HashPositions WHERE HashKey=makehash('rnbqkbnr/pppppppp/8/8/1P6/8/P1PPPPPP/RNBQKBNR b KQkq -')");
//        std::cout << "hash key: " << hashKey << std::endl;
//
//        return;
//
//#define _USE_INDEX_
//
//#ifdef _USE_INDEX_
//        std::cout << "Creating Index if not exists..." << std::endl;
//        auto startTime = getNow();
//        db.exec("CREATE INDEX IF NOT EXISTS HashKeyIndex ON HashPositions (HashKey)");
//        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//        std::cout << "Create Index, elapsed: " << elapsed << " ms, "
//                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << std::endl;
//#else
//        std::cout << "Drop Index if not exists..." << std::endl;
//        db.exec("DROP INDEX IF EXISTS HashKeyIndex");
//#endif
    }
    
//    SQLite::Database memdb(":memory:", SQLite::OPEN_READWRITE);
//
//    std::cout << "Load database into in-memory one" << std::endl;
//
//    auto startTime = getNow();
//    memdb.backup(dbPath.c_str(), SQLite::Database::BackupType::Load);
//
//    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//    std::cout << "Loaded into memory, elapsed: " << elapsed << " ms, "
//              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//              << std::endl;
//
//
//    std::cout << "SQLite database file '" << memdb.getFilename().c_str() << "' opened successfully\n";

    std::vector<bslib::BoardCore*> boardVec;
    for(auto && str : gameBodyVec) {
        auto board = new bslib::ChessBoard();
        board->newGame("");
        board->fromMoveList(str, bslib::Notation::san, false);
        assert(board->getHistListSize() > 1);
        boardVec.push_back(board);
    }
    
    {
        std::string sqlString = "SELECT GameID, MultiGameIDs FROM HashPositions WHERE HashKey=?";
        hashStatement = new SQLite::Statement(db, sqlString);

        sqlString =
        "SELECT g.ID, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves " \
        "FROM Games g " \
        "INNER JOIN Players w ON WhiteID = w.ID " \
        "INNER JOIN Players b ON BlackID = b.ID " \
        "WHERE inResultSet(g.ID)";

        benchStatement = new SQLite::Statement(db, sqlString);
    }

    for(auto ply = 8; ply < 30; ply++) {
        auto startTime = getNow();
        uint64_t total = 0, sucCnt = 0;
        for(auto j = 0; j < 1000; j++, total++) {

            auto k = std::rand() % boardVec.size();
            auto board = boardVec.at(k);
            auto sz = board->getHistListSize();
            if (ply >= sz) {
                continue;
            }

            auto hist = board->getHistAt(ply);

            hashStatement->reset();
            hashStatement->bind(1, static_cast<int64_t>(hist.hashKey));
            if (hashStatement->executeStep()) {
                gameIdSet.clear();
                auto c = hashStatement->getColumn(0);
                if (!c.isNull()) {
                    auto gameID = c.getInt();
                    gameIdSet.insert(gameID);
                } else {
                    auto c = hashStatement->getColumn(1);
                    assert(!c.isNull());
                    auto blob = c.getBlob(); assert(blob);
                    auto sz = c.size();
                    assert(blob && sz >= 4);
                    for(auto i = 0; i < sz / sizeof(int); i++) {
                        auto gameID = ((int*)blob)[i];
                        gameIdSet.insert(gameID);
                    }
                }

                benchStatement->reset();
                while (benchStatement->executeStep()) {
                    sucCnt++;
                }
            }
        }

        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
        std::cout << "ply: " << ply
                  << ", #total queries: " << total
                  << ", elapsed: " << elapsed << " ms, "
                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                  << ", time per query: " << elapsed / total  << " ms"
                  << ", total results: " << sucCnt << ", results/query: " << sucCnt / total
                  << std::endl;
    }

    // clean up
    {
        for(auto && board : boardVec) {
            delete board;
        }
        boardVec.clear();
        
        delete benchStatement;
    }
    std::cout << "Completed! " << std::endl;
}


