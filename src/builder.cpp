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

    const std::string sql = "INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves, MoveBlob, ID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    insertGameStatement = new SQLite::Statement(*mDb, sql);
    
    buf = new int16_t[1024 * 2];
}

ThreadRecord::~ThreadRecord()
{
    if (buf) delete buf;
    if (board) delete board;
    if (insertGameStatement) delete insertGameStatement;
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
        std::cout << "Thread count: " << pool->get_thread_count() << std::endl;

        playerIdMap.reserve(1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);
        
        // prepared statements
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");

//        insertHashStatement_blob = new SQLite::Statement(*mDb, "INSERT INTO BitboardPositions (Black, White, King, Queen, Rook, Bishop, Knight, Pawn, Prop, MultiGameIDs) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
//
//        insertHashStatement_one = new SQLite::Statement(*mDb, "INSERT INTO BitboardPositions (Black, White, King, Queen, Rook, Bishop, Knight, Pawn, Prop, GameID) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
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
        mDb->exec("CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, Moves TEXT, MoveBlob BLOB, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)");
        
//        mDb->exec("DROP TABLE IF EXISTS BitboardPositions");
//        mDb->exec("CREATE TABLE BitboardPositions (ID INTEGER PRIMARY KEY AUTOINCREMENT, White INTEGER, Black INTEGER, King INTEGER, Queen INTEGER, Rook INTEGER, Bishop INTEGER, Knight INTEGER, Pawn INTEGER, Prop INTEGER, GameID INTEGER DEFAULT NULL, MultiGameIDs BLOB DEFAULT NULL)");

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
    TagIdx_Moves,
    TagIdx_MoveBlob,
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
            t->board->fromMoveList(moveText, bslib::Notation::san, false, bslib::BoardCore::CreateExtra::bitboard);

            plyCount = t->board->getHistListSize();
//            saveBitboards(t->board, gameID);
            
            for(auto i = 0; i < plyCount; i++) {
                auto move = t->board->_getHistAt(i).move;
                auto m = move.from | move.dest << 6 | move.promotion << 12;
                t->buf[i] = m;
            }
            t->insertGameStatement->bind(TagIdx_MoveBlob + 1, t->buf, plyCount * sizeof(int16_t));
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

void doParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText)
{
//    const std::vector<std::string> gameBodyVec {
//"1.d4 Nf6 2.Nf3 d5 3.e3 Bf5 4.c4 c6 5.Nc3 e6 6.Bd3 Bxd3 7.Qxd3 Nbd7 8.b3 Bd6 \
//9.O-O O-O 10.Bb2 Qe7 11.Rad1 Rad8 12.Rfe1 dxc4 13.bxc4 e5 14.dxe5 Nxe5 15.Nxe5 Bxe5 \
//16.Qe2 Rxd1 17.Rxd1 Rd8 18.Rxd8+ Qxd8 19.Qd1 Qxd1+ 20.Nxd1 Bxb2 21.Nxb2 b5 \
//22.f3 Kf8 23.Kf2 Ke7  1/2-1/2",
//
//"1.e4 e5 2.Nf3 Nf6 3.d3 d6 4.c3 g6 5.Bg5 Bg7 6.Be2 O-O 7.d4 h6 8.Bxf6 Qxf6 \
//9.dxe5 dxe5 10.O-O Nd7 11.Nbd2 Nc5 12.Qc2 Bg4 13.Rfe1 Rfe8 14.b4 Ne6 15.h3 Bxf3 \
//16.Nxf3 Nf4 17.Bc4 Nxh3+ 18.gxh3 Qxf3 19.Re3 Qh5 20.Qb3 Rf8 21.Rd1 Qg5+ 22.Rg3 Qf6 \
//23.Rdd3 g5 24.Rgf3 Qe7 25.Rf5 Kh8 26.Bxf7 Rad8 27.Rdf3 Rd6 28.Kf1 Qd8 29.Bh5 Rxf5 \
//30.Rxf5 Rc6 31.Rf3 Rd6 32.Rf5 Rd2 33.Rf7 Qd3+ 34.Kg2 Qxe4+ 35.Bf3 Qh4 36.Bxb7 g4 \
//37.Qe6 Qxh3+ 38.Kg1 Rd1+  0-1",
//
//"1.e4 e5 2.Nf3 Nf6 3.Nc3 Nc6 4.Bc4 Bb4 5.d3 d5 6.exd5 Nxd5 7.Bd2 Nxc3 8.bxc3 Be7 \
//9.Qe2 Bf6 10.O-O O-O 11.Rfe1 Re8 12.Qe4 g6 13.g4 Na5 14.Bb3 Nxb3 15.axb3 Bd7 \
//16.g5 Bc6 17.Qh4 Bg7 18.Qg4 Qd6 19.Re2 Re6 20.Rae1 Rae8 21.c4 b6 22.Bc1 Ba8 \
//23.Nd2 Bc6 24.Ne4 Qe7 25.Re3 h5 26.Qg3 Bd7 27.Bb2 Bc6 28.Nf6+ Bxf6 29.gxf6 Qxf6 \
//30.Rxe5 Rxe5 31.Rxe5 Rd8 32.Qe3 Qh4 33.Qg5 Qxg5+ 34.Rxg5 Re8 35.Kf1 Bf3 36.Re5 Rd8 \
//37.Ke1 Kf8 38.Ba3+ Kg8 39.Re7 Rc8 40.Kd2 h4 41.Ke3 Bh5 42.Kd2 g5 43.Rd7 g4 \
//44.Re7 Kg7 45.Ke3 Kf6 46.d4 c5 47.Rxa7 Re8+ 48.Kd2 cxd4 49.Rd7 g3 50.fxg3 Re2+ \
//51.Kd3 Rxh2 52.gxh4 Bg6+ 53.Kxd4 Rd2+  0-1 ",
//
//"1.c4 c5 2.g3 Nc6 3.Bg2 g6 4.Nc3 Bg7 5.a3 d6 6.Rb1 a5 7.Nf3 Nf6 8.O-O O-O \
//9.Ne1 Bd7 10.Nc2 Rb8 11.d3 Ne8 12.Bd2  1/2-1/2"
//    };
//
//    std::cout << "Benchmark searching & matching dbPath: " << dbPath << std::endl;
//
//    SQLite::Database db(dbPath, SQLite::OPEN_READWRITE);
//    
//    db.createFunction("inResultSet", 1, true, nullptr, &inResultSet, nullptr, nullptr, nullptr);
//
//    db.createFunction("popCount", 1, true, nullptr, &popCount, nullptr, nullptr, nullptr);
//    db.createFunction("firstOne", 1, true, nullptr, &bitScanForward, nullptr, nullptr, nullptr);
//
//    // prepare tests
//    std::vector<bslib::BoardCore*> boardVec;
//    for(auto && str : gameBodyVec) {
//        auto board = new bslib::ChessBoard();
//        board->newGame("");
//        board->fromMoveList(str, bslib::Notation::san, false, bslib::BoardCore::CreateExtra::bitboard);
//        assert(board->getHistListSize() > 1);
//        boardVec.push_back(board);
//    }
//    
//    {
//        std::string sqlString = "SELECT * FROM BitboardPositions WHERE White=? AND King=? AND Queen=? AND Rook=? AND Bishop=? AND Knight=? AND Pawn=?";
//        bitboardStatement = new SQLite::Statement(db, sqlString);
//
//        sqlString =
//        "SELECT g.ID, e.Name Event, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves " \
//        "FROM Games g " \
//        "INNER JOIN Events e ON EventID = e.ID " \
//        "INNER JOIN Players w ON WhiteID = w.ID " \
//        "INNER JOIN Players b ON BlackID = b.ID " \
//        "WHERE inResultSet(g.ID)";
//
//        benchStatement = new SQLite::Statement(db, sqlString);
//    }
//    
//    // testing
//    // query positions with 2 Black Rooks in the middle squares
//    {
//        std::cout << "Finding all games with 2 Black Rooks..." << std::endl;
//        auto startTime = getNow();
//
//        std::string sqlString = "SELECT * FROM BitboardPositions WHERE popCount(Black & Rook & ?)=?";
//        SQLite::Statement statement(db, sqlString);
//        
//        int64_t maskMids =
////                              bslib::ChessBoard::posToBitboard[bslib::pos_c4]
////                            | bslib::ChessBoard::posToBitboard[bslib::pos_c5]
//                              bslib::ChessBoard::posToBitboard[bslib::pos_d4]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_d5]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_e4]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_e5]
////                            | bslib::ChessBoard::posToBitboard[bslib::pos_f4]
////                            | bslib::ChessBoard::posToBitboard[bslib::pos_f5]
//                            ;
//
//        statement.bind(1, maskMids);
//        statement.bind(2, 2);
//
//        auto cnt = benchMatchingMoves(&statement, BenchMatchingFlag_print_pgn);
//
//        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//        std::cout << "Finding 2 Black Rooks DONE. Elapsed: " << elapsed << " ms, "
//                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << ", total results: " << cnt
//                  << ", time per results: " << elapsed / std::max<int64_t>(1, cnt)  << " ms"
//                  << std::endl;
//    }
//
//    return;
//
//    // query positions with 3 White Queens
//    {
//        std::cout << "Finding all games with 3 White Queens..." << std::endl;
//        auto startTime = getNow();
//
//        std::string sqlString = "SELECT * FROM BitboardPositions WHERE popCount(White & Queen)=?";
//        SQLite::Statement statement(db, sqlString);
//        statement.bind(1, 3);
//
//        auto cnt = benchMatchingMoves(&statement, 0); //BenchMatchingFlag_print_pgn);
//
//        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//        std::cout << "Finding 3 White Queens DONE. Elapsed: " << elapsed << " ms, "
//                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << ", total results: " << cnt
//                  << ", time per results: " << elapsed / std::max<int64_t>(1, cnt)  << " ms"
//                  << std::endl;
//    }
//
//    // query games with White Pawns in d4, e5, f4, g4, Black King in b7
//    {
//        std::cout << "Finding all positions with White Pawns in d4, e5, f4, g4, Black King in b7..." << std::endl;
//        auto startTime = getNow();
//
//        std::string sqlString = "SELECT * FROM BitboardPositions WHERE (White & Pawn & ?)=? AND (Black & King)=?";
//        SQLite::Statement statement(db, sqlString);
//        
//        int64_t maskPawns =   bslib::ChessBoard::posToBitboard[bslib::pos_d4]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_e5]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_f4]
//                            | bslib::ChessBoard::posToBitboard[bslib::pos_g4];
//        int64_t maskKing = bslib::ChessBoard::posToBitboard[bslib::pos_b7];
//
//        statement.bind(1,  maskPawns);
//        statement.bind(2,  maskPawns);
//        statement.bind(3,  maskKing);
//
//        auto cnt = benchMatchingMoves(&statement, 0); //BenchMatchingFlag_print_fen|BenchMatchingFlag_print_pgn);
//
//        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//        std::cout
//                  << "Finding 4 Pawns & King in specific squares DONE. Elapsed: " << elapsed << " ms, "
//                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << ", total results: " << cnt
//                  << ", time per results: " << elapsed / std::max<int64_t>(1, cnt)  << " ms"
//                  << std::endl;
//    }
//    
//    // matching
//    std::cout << "Matching exactly..." << std::endl;
//    for(auto ply = 1; ply < 30; ply++) {
//        auto startTime = getNow();
//        uint64_t total = 0, sucCnt = 0;
//        for(auto j = 0; j < 1000; j++, total++) {
//
//            auto k = std::rand() % boardVec.size();
//            auto board = boardVec.at(k);
//            auto sz = board->getHistListSize();
//            if (ply >= sz) {
//                continue;
//            }
//
//            auto hist = board->getHistAt(ply);
//
//            bitboardStatement->reset();
//            
//            for(auto i = 1; i < 8; i++) {
//                bitboardStatement->bind(i, static_cast<int64_t>(hist.bitboardVec.at(i)));
//            }
//
//            sucCnt += benchMatchingMoves(bitboardStatement, BenchMatchingFlag_oneOnly);
//        }
//
//        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
//        std::cout << "ply: " << ply
//                  << ", #total queries: " << total
//                  << ", elapsed: " << elapsed << " ms, "
//                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
//                  << ", time per query: " << elapsed / total  << " ms"
//                  << ", total results: " << sucCnt << ", results/query: " << sucCnt / total
//                  << std::endl;
//    }
//
//    // clean up
//    {
//        for(auto && board : boardVec) {
//            delete board;
//        }
//        boardVec.clear();
//        
//        delete bitboardStatement;
//        delete benchStatement;
//    }
    assert(builder);
    builder->parsePGNGame(gameID, fenText, moveText);
}

void Builder::threadParsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText)
{
    pool->submit(doParsePGNGame, gameID, fenText, moveText);
}

void Builder::parsePGNGame(int64_t gameID, const std::string& fenText, const std::string& moveText)
{
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
    t->board->fromMoveList(gameID, moveText, bslib::Notation::san, false, bslib::BoardCore::CreateExtra::bitboard, checkToStop);

    t->gameCnt++;
    t->hdpLen += t->board->getHistListSize();
}

void Builder::searchPositions(SQLite::Database& db, std::function<bool(int64_t gameId, const std::vector<uint64_t>&, const bslib::BoardCore*)> checkToStop)
{
    auto board = new bslib::ChessBoard();
    SQLite::Statement statement(db, "SELECT * FROM Games");
    
    int64_t cnt = 0;
    while (statement.executeStep()) {
        cnt++;
        auto gameID = statement.getColumn("ID").getInt64();
        auto fenString = statement.getColumn("FEN").getText();
        auto moveText = statement.getColumn("Moves").getText();
        
        board->newGame(fenString);
        board->fromMoveList(gameID, moveText, bslib::Notation::san, false, bslib::BoardCore::CreateExtra::none, nullptr);
    }
    
    delete board;
    
    std::cout << "Builder::searchPositions, cnt: " << cnt << std::endl;
}

void Builder::searchPosition(SQLite::Database& db, const std::string& query)
{
    auto parser = new Parser;
    if (!parser->parse(query.c_str())) {
        std::cerr << "Error: " << parser->getErrorString() << std::endl;
        return;
    }
    
    auto startTime = getNow();

    checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board) -> bool {
        assert(board && bitboardVec.size() >= 11);
        
        if (parser->evaluate(bitboardVec)) {
            succCount++;
            std::cout << succCount << ". gameId: " << gameId << ", fen: " << board->getFen() << std::endl;
            return true;
        }

        return false;
    };

    std::cout << "Search with query " << query <<  "..." << std::endl;
    succCount = 0;
    
    SQLite::Statement statement(db, "SELECT * FROM Games");
    int64_t cnt = 0;
    for (; statement.executeStep(); cnt++) {
        auto gameID = statement.getColumn("ID").getInt64();
        auto fenText = statement.getColumn("FEN").getText();
        auto moveText = statement.getColumn("Moves").getText();
        threadParsePGNGame(gameID, fenText, moveText);
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
        "B[c-f] + b[c-f] == 2",               // There are two Bishops (any side) from column c to f
        "white6 = 5",                        // There are 5 white pieces on row 6
    };
    
    for(auto && s : queries) {
        searchPosition(db, s);
    }

    delete pool;

    std::cout << "Completed! " << std::endl;
}


