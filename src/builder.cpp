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

// for mmap:
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>


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


void Builder::convertPgn2Sql(const std::string& pgnPath, const std::string& sqlitePath, bool _moveVerify)
{
    testReadTextFile(pgnPath);
    return;
    
    // Prepare
    moveVerify = _moveVerify;
    board = createBoard(chessVariant);
    setDatabasePath(sqlitePath);
    
    // Create database
    mDb = createDb(sqlitePath);
    if (!mDb) {
        return;
    }

    processPgnFile(pgnPath);

    // Clean up
    delete board;
    board = nullptr;
}

void handle_error(const char* msg) {
    perror(msg);
    exit(255);
}

const char* map_file(const char* fname, size_t& length)
{
    int fd = open(fname, O_RDONLY);
    if (fd == -1)
        handle_error("open");

    // obtain file size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
        handle_error("fstat");

    length = sb.st_size;

    const char* addr = static_cast<const char*>(mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0u));
    if (addr == MAP_FAILED)
        handle_error("mmap");

    // TODO close fd at some point in time, call munmap(...)
    return addr;
}

void Builder::testReadTextFile(const std::string& path)
{
    std::cout << "testReadTextFile PGN file: '" << path << "'" << std::endl;

    for(auto method = 0; method <= 6; method++) {
        testReadTextFile(path, method);
    }
    std::cout << "Completed! " << std::endl;
}

void Builder::testReadTextFile(const std::string& path, int method)
{
    startTime = getNow();

    int64_t lineCnt = 0;
    
    std::string methodName;
    
    switch (method) {
        case 0:
        {
            methodName = "C++ simple";
            std::string str;
            std::ifstream inFile(path);
            while (getline(inFile, str)) {
                lineCnt++;
            }
            inFile.close();
            break;
        }
        case 1:
        {
            methodName = "C way";
            std::ifstream inFile(path);
            const int MAX_LENGTH = 1024 * 16;
            char* line = new char[MAX_LENGTH];
            while (inFile.getline(line, MAX_LENGTH) && strlen(line) > 0) {
                lineCnt++;
            }
            delete[] line;
            inFile.close();
            break;
        }
        case 2:
        {
            methodName = "mmap (memory mapped files)";
            size_t length;
            auto f = map_file(path.c_str(), length);
            auto l = f + length;

            while (f && f != l) {
                if ((f = static_cast<const char*>(memchr(f, '\n', l - f)))) {
                    lineCnt++;
                    f++;
                }
            }
            break;
        }

        case 3:
        {
            methodName = "all to one block, C++ way";
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            if (file.read(buffer.data(), size)) {
                for(auto && ch : buffer) {
                    if (ch == '\n') lineCnt++;
                }
            }
            break;
        }
            
        case 4:
        {
            methodName = "all to one block, C way";
            FILE *stream = fopen(path.c_str(), "r");
            assert(stream != NULL);
            fseek(stream, 0, SEEK_END);
            long stream_size = ftell(stream);
            fseek(stream, 0, SEEK_SET);
            char *buffer = (char*)malloc(stream_size);
            fread(buffer, stream_size, 1, stream);
            assert(ferror(stream) == 0);
            fclose(stream);
            
            for(long i = 0; i < stream_size; i++) {
                if (buffer[i] == '\n') lineCnt++;
            }

            free((void *)buffer);
            break;
        }

        case 5:
        {
            methodName = "blocks of 4 MB, C++ way";
            const size_t blockSz = 4 * 1024 * 1024;

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            size_t sz = 0;
            
            std::vector<char> buffer(blockSz);
            while (sz < size) {
                auto k = std::min(blockSz, size - sz);
                if (k == 0) {
                    break;
                }
                if (file.read(buffer.data(), k)) {
                    for(size_t i = 0; i < k; i++) {
                        if (buffer[i] == '\n') lineCnt++;
                    }
                }
                
                sz += k;
            }
            break;
        }
        case 6:
        {
            methodName = "blocks of 4 MB, C way";
            const size_t blockSz = 4 * 1024 * 1024;
            char *buffer = (char*)malloc(blockSz + 16);

            FILE *stream = fopen(path.c_str(), "r");
            assert(stream != NULL);
            fseek(stream, 0, SEEK_END);
            size_t size = ftell(stream);
            fseek(stream, 0, SEEK_SET);

            
            size_t sz = 0;
            
            while (sz < size) {
                auto k = std::min(blockSz, size - sz);
                if (k == 0) {
                    break;
                }
                if (fread(buffer, k, 1, stream)) {
                    for(size_t i = 0; i < k; i++) {
                        if (buffer[i] == '\n') lineCnt++;
                    }
                }
                sz += k;
            }

            free(buffer);
            break;
        }

        default:
            break;
    }
    
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;

    std::cout << "Method: " << method << ", " << methodName
              << ", #lineCnt: " << lineCnt
              << ", elapsed: " << elapsed << " ms"
              << ", speed: " << lineCnt * 1000 / elapsed << " lines/s"
              << std::endl;

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
        return 0;
    }

    // Begin transaction
    SQLite::Transaction transaction(*mDb);

    // prepared statements
    {
        const std::string sql = "INSERT INTO game(event_id, white_id, white_elo, black_id, black_elo, timer, result, date, eco, length, fen, moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        insertGameStatement = new SQLite::Statement(*mDb, sql);
        
        playerGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM player WHERE name=?");
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO player(name, elo) VALUES (?, ?) RETURNING id");

        eventGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM event WHERE name=?");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO event(name) VALUES (?) RETURNING id");

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
        if (p == 0 && moveText.length() < 10) {
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
    if (moveVerify) {
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
    } else {
        r.moveString = moveText;
    }

    assert(!r.moveString.empty());

    // old way
    //return addGame(r);
    
    // new way
    return addGameWithPreparedStatement(r);
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
        mDb->exec("CREATE TABLE game(id INTEGER PRIMARY KEY AUTOINCREMENT, event_id INTEGER, white_id INTEGER, white_elo INTEGER, black_id INTEGER, black_elo INTEGER, timer TEXT, date TEXT, eco TEXT, result INTEGER, length INTEGER, fen TEXT, moves TEXT, FOREIGN KEY(event_id) REFERENCES event, FOREIGN KEY(white_id) REFERENCES player, FOREIGN KEY(black_id) REFERENCES player)");
        
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


int Builder::getPlayerNameIdWithPreparedStatements(const std::string& name, int elo)
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

int Builder::getEventNameIdWithPreparedStatements(const std::string& name)
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

bool Builder::addGameWithPreparedStatement(const GameRecord& r)
{
    try
    {
        assert(mDb);

        auto eventId = getEventNameIdWithPreparedStatements(r.eventName);
        auto whiteId = getPlayerNameIdWithPreparedStatements(r.whiteName, r.whiteElo);
        auto blackId = getPlayerNameIdWithPreparedStatements(r.blackName, r.blackElo);

        std::string whiteElo = r.whiteElo > 0 ? std::to_string(r.whiteElo) : "NULL";
        std::string blackElo = r.blackElo > 0 ? std::to_string(r.blackElo) : "NULL";

        insertGameStatement->bind(1, eventId);
        insertGameStatement->bind(2, whiteId);
        insertGameStatement->bind(3, whiteElo);

        insertGameStatement->bind(4, blackId);
        insertGameStatement->bind(5, blackElo);

        insertGameStatement->bind(6, r.timer);
        insertGameStatement->bind(7, Funcs::resultType2String(r.resultType, false));

        insertGameStatement->bind(8, r.dateString);
        insertGameStatement->bind(9, r.eco);
        insertGameStatement->bind(10, r.moveCnt);
        insertGameStatement->bind(11, r.fen);
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

void Builder::testInsertingSpeed(const std::string& dbPath)
{
    std::vector<std::string> gameBodyVec {
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

    setDatabasePath(dbPath);
    
    // Create database
    createDb(dbPath);
    openDbToWrite();

    std::cout << "testInsertingSpeed, dbPath: '" << dbPath << "'" << std::endl;

    startTime = getNow();
    gameCnt = errCnt = 0;

    // Begin transaction
    SQLite::Transaction transaction(*mDb);

    const std::string sql = "INSERT INTO game(event_id, white_id, white_elo, black_id, black_elo, timer, result, date, eco, length, fen, moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    insertGameStatement = new SQLite::Statement(*mDb, sql);
    
    playerGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM player WHERE name=?");
    playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO player(name, elo) VALUES (?, ?) RETURNING id");

    eventGetIdStatement = new SQLite::Statement(*mDb, "SELECT id FROM event WHERE name=?");
    eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO event(name) VALUES (?) RETURNING id");

    for(gameCnt = 0; gameCnt < 3450777; gameCnt++) {
        GameRecord r;
        auto  eIdx = rand() % 31049, wIdx = rand() % 284442, bIdx = rand() % 284442;
        r.eventName = "event" + std::to_string(eIdx);
        r.whiteName = "player" + std::to_string(wIdx);
        r.blackName = "player" + std::to_string(bIdx);
        
        r.whiteElo = rand() % 3000;
        r.blackElo = rand() % 3000;
        
        auto k = rand() % gameBodyVec.size();
        r.moveString = gameBodyVec.at(k);
        if (!addGameWithPreparedStatement(r)) {
            errCnt++;
        }

        // Frequently update info
        if (gameCnt && (gameCnt & 0xfff) == 0) {
            printStats();
        }
    }

    // Commit transaction
    transaction.commit();

    delete insertGameStatement;
    delete playerGetIdStatement;
    delete playerInsertStatement;

    delete eventGetIdStatement;
    delete eventInsertStatement;

    printStats();
    std::cout << "Completed! " << std::endl;
}
