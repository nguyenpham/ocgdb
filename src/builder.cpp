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
    
    std::unordered_map<char*, char*> tagMap;

    auto st = 0, evtCnt = 0;
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
                        if (evtCnt == 0 && connectBlock) {
                            long len =  (event - buffer) - 1;
                            processHalfEnd(buffer, len);
                        }
                        hasEvent = true;
                        evtCnt++;
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
    eventCnt = playerCnt = siteCnt = 1;

    // Begin transaction
    SQLite::Transaction transaction(*mDb);

    playerIdMap.reserve(1024 * 1024);
    eventIdMap.reserve(128 * 1024);
    siteIdMap.reserve(128 * 1024);
    
    // prepared statements
    {
        const std::string sql = "INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        insertGameStatement = new SQLite::Statement(*mDb, sql);
        
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");

        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (ID, Name) VALUES (?, ?)");

        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");
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

        str = std::string("INSERT INTO Info (Name, Value) VALUES ('PlayerCount', '") + std::to_string(playerCnt) + "')";
        mDb->exec(str);

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
        delete playerInsertStatement;
        delete eventInsertStatement;
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
        mDb->exec("CREATE TABLE Players (ID INTEGER PRIMARY KEY, Name TEXT UNIQUE, Elo INTEGER)");
        mDb->exec("INSERT INTO Players (ID, Name) VALUES (1, \"\")"); // default empty

        mDb->exec("DROP TABLE IF EXISTS Games");
        mDb->exec("CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, Moves TEXT, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)");
        
//        mDb->exec("PRAGMA synchronous=OFF");
        mDb->exec("PRAGMA journal_mode=MEMORY");
        mDb->exec("PRAGMA cache_size=64000;");
        
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

int Builder::getPlayerNameId(char* name, int elo)
{
    return getNameId(name, elo, playerCnt, playerInsertStatement, playerIdMap);
}

int Builder::getEventNameId(char* name)
{
    return getNameId(name, -1, eventCnt, eventInsertStatement, eventIdMap);
}

int Builder::getNameId(char* name, int elo, int& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, int>& idMap)
{
    name = Funcs::trim(name);

    // null, empty or ?
    if (!name || *name == 0 || *name == '?') return 1;

    auto s = std::string(name);
    Funcs::toLower(s);
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
    TagIdx_Result, TagIdx_Timer, TagIdx_ECO, TagIdx_PlyCount, TagIdx_FEN,
    TagIdx_Moves,
    TagIdx_Max
};

bool Builder::addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    if (itemMap.size() < 3) {
        return false;
    }
    
    try {
        insertGameStatement->reset();

        auto eventId = 1, whiteElo = 0, blackElo = 0;
        char* whiteName = nullptr, *blackName = nullptr, *date = nullptr;

        for(auto && it : itemMap) {
            for(auto i = 0; tagNames[i]; i++) {
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
                        insertGameStatement->bind(i + 1, siteId);
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
                    case TagIdx_Result:
                    case TagIdx_Timer:
                    case TagIdx_ECO:
                    case TagIdx_FEN:
                    {
                        if (s[0]) { // ignored empty, use NULL instead
                            insertGameStatement->bind(i + 1, s);
                        }
                        break;
                    }
                    case TagIdx_WhiteElo:
                    {
                        whiteElo = std::atoi(s);
                        if (whiteElo > 0) {
                            insertGameStatement->bind(i + 1, whiteElo);
                        }
                        break;
                    }
                    case TagIdx_BlackElo:
                    {
                        blackElo = std::atoi(s);
                        if (blackElo > 0) {
                            insertGameStatement->bind(i + 1, blackElo);
                        }
                        break;
                    }
                    case TagIdx_Round:
                    case TagIdx_PlyCount:
                    {
                        insertGameStatement->bind(i + 1, std::atoi(s));
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
        insertGameStatement->bind(TagIdx_Black + 1, whiteId);
        insertGameStatement->bind(TagIdx_White + 1, blackId);

        insertGameStatement->bind(TagIdx_Event + 1, eventId);

        if (date) {
            insertGameStatement->bind(TagIdx_Date + 1, date);
        }

        // trim left
        while(*moveText <= ' ') moveText++;
        insertGameStatement->bind(TagIdx_Moves + 1, moveText);

        insertGameStatement->exec();
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

void Builder::benchMatchMoves(const std::string& dbPath)
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

    SQLite::Database db(dbPath);  // SQLite::OPEN_READONLY
    std::cout << "SQLite database file '" << db.getFilename().c_str() << "' opened successfully\n";

    std::cout << "Bench Match Moves..." << std::endl;

    std::vector<BoardCore*> boardVec;
    for(auto && str : gameBodyVec) {
        auto board = new ChessBoard();
        board->newGame("");
        board->fromMoveList(str, Notation::san);
        assert(board->getHistListSize() > 1);
        boardVec.push_back(board);
    }
    
    {
        std::string sqlString =
        "SELECT g.ID, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves " \
        "FROM Games g " \
        "INNER JOIN Players w ON WhiteID = w.ID " \
        "INNER JOIN Players b ON BlackID = b.ID " \
        "WHERE g.Moves LIKE ?";

        benchStatement = new SQLite::Statement(db, sqlString);
    }

    for(auto ply = 1; ply < 20; ply++) {
        auto startTime = getNow();
        auto total = 0, sucCnt = 0;
        for(auto j = 0; j < 1000; j++, total++) {

            auto k = std::rand() % boardVec.size();
            auto board = boardVec.at(k);
            auto sz = board->getHistListSize();
            auto n = std::min(ply, sz);
            
            std::string s;
            for(auto t = 0; t < n; t++) {
                auto hist = board->getHistAt(t);
                if (t) s += " ";
                
                if ((t & 1) == 0) {
                    s += std::to_string(t / 2 + 1) + ".";
                }
                
                s += hist.sanString;
            }
            s += "%";
            
            benchStatement->reset();
            benchStatement->bind(1, s);
//            std::cout << benchStatement->getExpandedSQL() << std::endl << std::endl;

            while (benchStatement->executeStep()) {
                sucCnt++;
            }
        }
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;

        std::cout   << "ply: " << ply
                    << ", #total queries: " << total
                  << ", elapsed: " << elapsed << " ms, "
                  << Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << ", speed: " << total * 1000 / elapsed << " query/s"
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
