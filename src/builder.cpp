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

using namespace ocgdb;

Builder* builder = nullptr;

void ThreadRecord::init(SQLite::Database* mDb)
{
    if (board) return;
    
    errCnt = 0;
    
    assert(mDb);
    board = Builder::createBoard(bslib::ChessVariant::standard);

    std::string moveBlobFieldName, moveTextFieldName, moveTextFieldName2;
    if (EncodeMoveSize == 1) {
        moveBlobFieldName = "Moves1";
    } else {
        moveBlobFieldName = "Moves2";
    }
    if (KeepMovesField) {
        moveTextFieldName = "Moves,";
        moveTextFieldName2 = "?,";
    }

    const std::string sql = "INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, ID, "
    + moveTextFieldName
    + moveBlobFieldName +
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
    + moveTextFieldName2 +
    + "?, ?)";

    insertGameStatement = new SQLite::Statement(*mDb, sql);
    insertCommentStatement = new SQLite::Statement(*mDb, "INSERT INTO Comments (GameID, Ply, Comment) VALUES (?, ?, ?)");

    buf = new int8_t[1024 * 2];
}

ThreadRecord::~ThreadRecord()
{
    if (buf) delete buf;
    if (board) delete board;
    if (insertGameStatement) delete insertGameStatement;
    if (insertCommentStatement) delete insertCommentStatement;
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
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    std::cout << "#games: " << gameCnt
              << ", elapsed: " << elapsed << "ms "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << gameCnt * 1000ULL / elapsed << " games/s"
              << std::endl;
}

bslib::BoardCore* Builder::createBoard(bslib::ChessVariant variant)
{
    return variant == bslib::ChessVariant::standard ? new bslib::ChessBoard : nullptr;
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
        
        playerIdMap.reserve(1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);
        
        // prepared statements
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");
        
        std::cout << "Thread count: " << pool->get_thread_count()
        << std::endl;
        
        std::cout << "Move encoding size: "
        << EncodeMoveSize
        << (KeepMovesField ? ", keep field Moves" : " without field Moves")
        << std::endl;

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
        
//        delete insertHashStatement_blob;
//        delete insertHashStatement_one;
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

    printStats();

    return gameCnt;
}

// replace Halfmove clock to 0 and Fullmove number to 1
int Builder::standardizeFEN(char *fenBuf)
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
        
        std::string moveBlobFieldName, moveTextFieldName;
        if (EncodeMoveSize == 1) {
            moveBlobFieldName = "Moves1";
        } else {
            moveBlobFieldName = "Moves2";
        }
        
        if (KeepMovesField) {
            moveTextFieldName = "Moves TEXT,";
        }
        std::string sqlstring = "CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, "
        + moveTextFieldName
        + moveBlobFieldName
        + " BLOB DEFAULT NULL, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)";
        
        mDb->exec(sqlstring);

        mDb->exec("DROP TABLE IF EXISTS Comments");
        mDb->exec("CREATE TABLE Comments (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INTEGER, Ply INTEGER, Comment TEXT)");


        mDb->exec("PRAGMA journal_mode=OFF");
//        mDb->exec("PRAGMA synchronous=OFF");
//        mDb->exec("PRAGMA cache_size=64000");

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
    TagIdx_FEN,
    TagIdx_GameID,
    TagIdx_Moves,
//    TagIdx_MoveBlob,
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

static void standardizeDate(char* date)
{
    assert(date);
    auto c = 0;
    for(char* p = date; *p; p++) {
        if (*p == '.') {
            *p = '-';
            c++;
        } else if (*p == '?') {
            *p = '1';
        }
    }
    
    if (c != 2) {
        date[0] = 0;
    }
    
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
        t->insertGameStatement->clearBindings();

        auto eventId = 1, whiteElo = 0, blackElo = 0, plyCount = 0;
        char* whiteName = nullptr, *blackName = nullptr, *date = nullptr;
        std::string fenString;

        for(auto && it : itemMap) {
            for(auto i = 0; tagNames[i]; i++) {
                if (strcmp(it.first, tagNames[i]) != 0) {
                    if (strcmp(it.first, "Variant") == 0) {
                        auto variant = bslib::Funcs::string2ChessVariant(it.second);
                        // at this moment, support only the standard variant
                        assert(variant != bslib::ChessVariant::standard);

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
            standardizeDate(date);
            if (*date) {
                t->insertGameStatement->bind(TagIdx_Date + 1, date);
            }
        }

        int gameID;
        {
            std::lock_guard<std::mutex> dolock(gameMutex);
            ++gameCnt;
            gameID = gameCnt;
        }
        t->insertGameStatement->bind(TagIdx_GameID + 1, gameID);

        int tagIdx_MoveBlob = TagIdx_Moves;

        if (KeepMovesField) {
            // trim left
            while(*moveText <= ' ') moveText++;
            t->insertGameStatement->bind(TagIdx_Moves + 1, moveText);
            
            tagIdx_MoveBlob++;
        }

        // Parse moves
        {
            //assert(t->board);
            t->board->newGame(fenString);

            t->board->fromMoveList(gameID, moveText, bslib::Notation::san, bslib::BoardCore::ParseMoveListFlag_quick_check);
//            t->board->fromMoveList(gameID, moveText, bslib::Notation::san, 0);

            plyCount = t->board->getHistListSize();
            if (plyCount == 0) {
                return false;
            }

            if (plyCount > 0) {
                auto p = t->buf;
                for(auto i = 0; i < plyCount; i++) {
                    auto h = t->board->_getHistPointerAt(i);
                    auto move = h->move;
                    
                    if (EncodeMoveSize == 1) {
                        auto pair = bslib::ChessBoard::encode1Byte(move);
                        assert(pair.second == 1 || pair.second == 2);
                        if (pair.second == 1) {
                            *p = static_cast<int8_t>(pair.first);
                            p++;
                        } else {
                            *(int16_t*)p = pair.first;
                            assert(*p == static_cast<int8_t>(pair.first));
                            assert(*(p + 1) == static_cast<int8_t>(pair.first >> 8));
                            p += 2;
                        }
                    } else { // 2 bytes encoding
                        *(int16_t*)p = bslib::ChessBoard::encode2Bytes(move);
                        p += 2;
                    }
                    
                    if (!h->comment.empty()) {
                        t->insertCommentStatement->reset();
                        t->insertCommentStatement->bind(1, gameID);
                        t->insertCommentStatement->bind(2, i);
                        t->insertCommentStatement->bind(3, h->comment);
                        t->insertCommentStatement->exec();
                    }
                }
                
                auto cnt = static_cast<int>(p - t->buf);
                assert(cnt >= plyCount);
                t->insertGameStatement->bind(tagIdx_MoveBlob + 1, t->buf, cnt);
            }
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


int popCount(uint64_t x) {
   int count = 0;
   while (x) {
       count++;
       x &= x - 1; // reset LS1B
   }
   return count;
}


static const int lsb_64_table[64] =
{
   63, 30,  3, 32, 59, 14, 11, 33,
   60, 24, 50,  9, 55, 19, 21, 34,
   61, 29,  2, 53, 51, 23, 41, 18,
   56, 28,  1, 43, 46, 27,  0, 35,
   62, 31, 58,  4,  5, 49, 54,  6,
   15, 52, 12, 40,  7, 42, 45, 16,
   25, 57, 48, 13, 10, 39,  8, 44,
   20, 47, 38, 22, 17, 37, 36, 26
};

static int _bitScanForward(uint64_t bb) {
   unsigned int folded;
   assert (bb != 0);
   bb ^= bb - 1;
   folded = (int) bb ^ (bb >> 32);
   return lsb_64_table[folded * 0x78291ACF >> 26];
}

static int bitScanForwardWithReset(uint64_t &bb) {
    int idx = _bitScanForward(bb);
    bb &= bb - 1; // reset bit outside
    return idx;
}


void Builder::updateBoard(bslib::BoardCore* board, const std::vector<uint64_t>& bbvec)
{
    // bitboards of black, white, king must not be zero
    assert(board && bbvec.size() >= 9 && bbvec[0] && bbvec[1] && bbvec[2]);
    
    for(auto i = 0; i < 64; ++i) {
        board->setEmpty(i);
    }

    auto bbblack = bbvec[0];
    for(int type = bslib::KING; type <= bslib::PAWNSTD; ++type) {
        auto bb = bbvec.at(type + 1);
        
        while(bb) {
            auto pos = bitScanForwardWithReset(bb); assert(pos >= 0 && pos <  64);
            auto side = (bbblack & bslib::ChessBoard::_posToBitboard[pos]) ? bslib::Side::black : bslib::Side::white;
            bslib::Piece piece(type, side);
            board->setPiece(pos, piece);
        }
    }
    
    auto chessBoard = static_cast<bslib::ChessBoard*>(board);
    auto prop = bbvec.at(8);
    auto enpassant = static_cast<int8_t>(prop & 0xff);
    chessBoard->setEnpassant(enpassant);
    chessBoard->setCastleRights(0, (prop >> 8) & bslib::CastleRight_mask);
    chessBoard->setCastleRights(1, (prop >> 10) & bslib::CastleRight_mask);
}

void doParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText, const std::vector<int8_t>& moveVec)
{
    assert(builder);
    builder->parsePGNGame(gameID, fenText, moveText, moveVec);
}

void Builder::threadParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText, const std::vector<int8_t>& moveVec)
{
    pool->submit(doParsePGNGame, gameID, fenText, moveText, moveVec);
}

void Builder::parsePGNGame(int64_t gameID, const std::string& fenText,
                           const std::string& moveText,
                           const std::vector<int8_t>& moveVec)
{
    assert(!moveVec.empty());

    auto threadId = std::this_thread::get_id();
    ThreadRecord* t;
    
    {
        std::lock_guard<std::mutex> dolock(parsingMutex);
        t = &threadMap[threadId];
    }
    if (!t->board) {
        t->board = Builder::createBoard(bslib::ChessVariant::standard);
    }
    assert(t->board);
    
    t->board->newGame(fenText);
    
    if (searchField == SearchField::moves) { // there is a text move only
        t->board->fromMoveList(gameID, moveText, bslib::Notation::san, bslib::BoardCore::ParseMoveListFlag_quick_check);

    } else {
        int flag = bslib::BoardCore::ParseMoveListFlag_create_bitboard;
        
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }
        
        t->board->fromMoveList(gameID, moveVec, flag, checkToStop);
    }

    t->gameCnt++;
    t->hdpLen += t->board->getHistListSize();
}


void Builder::searchPosition(SQLite::Database& db, const std::string& query)
{
    auto parser = new Parser;
    if (!parser->parse(query.c_str())) {
        std::cerr << "Error: " << parser->getErrorString() << std::endl;
        return;
    }
    
    auto startTime = getNow();
    
    // check if there at least a move fields (Moves, Moves1 or Moves2)
    {
        searchField = SearchField::none;

        SQLite::Statement stmt(db, "PRAGMA table_info(Games)");
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
                if (searchField < SearchField::moves) {
                    searchField = SearchField::moves;
                }
            }
        }

        if (searchField < SearchField::moves) {
            std::cerr << "FATAL ERROR: missing move field (Moves or Moves1 or Moves2)" << std::endl;
            return;
        }
    }
    

    checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board) -> bool {
        assert(board && bitboardVec.size() >= 11);
        
        if (parser->evaluate(bitboardVec)) {
            succCount++;
            std::cout << succCount << ". gameId: " << gameId << ", fen: " << board->getFen() << std::endl;
            return true;
        }

        return false;
    };

    std::cout << "Search with query " << query <<  "..."
    << ", searchField: " << static_cast<int>(searchField)
    << std::endl;
    succCount = 0;
    
    std::string moveFieldName;
    if (searchField == SearchField::moves1) {
        moveFieldName = "Moves1";
    } else if (searchField == SearchField::moves2) {
        moveFieldName = "Moves2";
    }

    SQLite::Statement statement(db, "SELECT * FROM Games");
    int64_t cnt = 0;
    for (; statement.executeStep(); cnt++) {
        auto gameID = statement.getColumn("ID").getInt64();
        auto fenText = statement.getColumn("FEN").getText();
        
        std::string moveText;
        std::vector<int8_t> moveVec;

        if (searchField == SearchField::moves) {
            moveText = statement.getColumn("Moves").getText();
        } else {
            auto c = statement.getColumn(moveFieldName.c_str());
            auto moveBlob = static_cast<const int8_t*>(c.getBlob());
            
            if (!moveBlob) {
                continue;
            }
            auto sz = c.size();
            
            for(auto i = 0; i < sz; ++i) {
                moveVec.push_back(moveBlob[i]);
            }
        }
        threadParsePGNGame(gameID, fenText, moveText, moveVec);
    }
    pool->wait_for_tasks();

    int64_t parsedGameCnt = 0, allHdpLen = 0;
    for(auto && t : threadMap) {
        parsedGameCnt += t.second.gameCnt;
        allHdpLen += t.second.hdpLen;
    }

    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    
    std::cout << std::endl << query << " DONE. Elapsed: " << elapsed << " ms, "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", total results: " << succCount
              << ", time per results: " << elapsed / std::max<int64_t>(1, succCount)  << " ms"
              << std::endl << std::endl << std::endl;
    
    delete parser;
}

void Builder::bench(const std::string& dbPath, int cpu)
{
    std::cout << "Benchmark position searching: " << dbPath << std::endl;

    SQLite::Database db(dbPath, SQLite::OPEN_READWRITE);

    // Reading all data, parsing moves, multi threads
    pool = new thread_pool(cpu);
    std::cout << "Thread count: " << pool->get_thread_count() << std::endl;

    const std::string queries[] = {
        "Q = 3",                            // three White Queens
        "r[e4, e5, d4,d5]= 2",              // two black Rooks in middle squares
        "P[d4, e5, f4, g4] = 4 and kb7",    // White Pawns in d4, e5, f4, g4 and black King in b7
//        "B[c-f] + b[c-f] == 2",               // There are two Bishops (any side) from column c to f
        "white6 = 5",                        // There are 5 white pieces on row 6
    };
    
    for(auto && s : queries) {
        searchPosition(db, s);
    }

    delete pool;

    std::cout << "Completed! " << std::endl;
}


