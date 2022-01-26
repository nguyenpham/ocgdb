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


bool ParaRecord::isValid() const
{
    if (dbPath.empty() && task != Task::query) {
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
            if (cpuNumber <= 0) {
                errorString = "CPU number must be greater than zero";
                break;
            }

            if (!hasPgn) {
                errorString = "Must have at least one PGN path. Mising or wrong parameter -pgn";
                break;
            }
            
            ok = true;
            break;
        }
            
        case Task::query:
            if (dbPath.empty() && !hasPgn) {
                errorString = "Must have a database (.db3) path or a PGN path. Mising or wrong parameter -db and -pgn";
                return false;
            }
            if (queries.empty()) {
                errorString = "Must have at least one query. Mising or wrong parameter -q";
                break;
            }
        case Task::bench:
        {
            if (cpuNumber <= 0) {
                errorString = "CPU number must be greater than zero";
                break;
            }

            ok = true;
            break;
        }
        case Task::getgame:
        {
            if (gameID <= 0) {
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
};

std::string ParaRecord::toString() const
{
    std::string s;
    
    const std::string taskNames[] = {
        "create SQL database",
        "query",
        "bench",
        "get game"

    };
        
    s = "\tTask: " + taskNames[static_cast<int>(task)] + "\n";
    
    s += "\tPGN paths:\n";
    for(auto && path : pgnPaths) {
        s += "\t\t" + path + "\n";
    }
    
    s += "\tDatabase path: " + dbPath + "\n";

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
    optionFlag = 0;
    auto vec = bslib::Funcs::splitString(optionString, ';');
    
    for(auto && s : vec) {
        auto it = optionNameMap.find(s);
        if (it == optionNameMap.end()) {
            std::cerr << "Error: Don't know option string: " << it->first << std::endl;
        } else {
            optionFlag |= 1 << it->second;
        }
    }
}

static const char* tagNames[] = {
    "GameID", // Not real PGN tag, added for convernience
    
    "Event", "Site", "Date", "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount", "FEN",

    nullptr, nullptr
};

enum {
    TagIdx_GameID,
    TagIdx_Event, TagIdx_Site, TagIdx_Date, TagIdx_Round,
    TagIdx_White, TagIdx_WhiteElo, TagIdx_Black, TagIdx_BlackElo,
    TagIdx_Result, TagIdx_Timer, TagIdx_ECO, TagIdx_PlyCount,
    TagIdx_FEN,

    TagIdx_Max
};


void ThreadRecord::init(SQLite::Database* mDb)
{
    if (board) return;
    
    errCnt = 0;
    
    board = Builder::createBoard(bslib::ChessVariant::standard);
    buf = new int8_t[1024 * 2];

    if (mDb) {
        insertCommentStatement = new SQLite::Statement(*mDb, "INSERT INTO Comments (GameID, Ply, Comment) VALUES (?, ?, ?)");
    }
}

ThreadRecord::~ThreadRecord()
{
    if (buf) delete buf;
    if (board) delete board;
    if (insertGameStatement) delete insertGameStatement;
    if (insertCommentStatement) delete insertCommentStatement;
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
              << ", #blocks: " << blockCnt << ", processed size: " << processedPgnSz / (1024 * 1024ULL) << " MB"
              << std::endl;
}

bslib::BoardCore* Builder::createBoard(bslib::ChessVariant variant)
{
    return variant == bslib::ChessVariant::standard ? new bslib::ChessBoard : nullptr;
}

void Builder::runTask(const ParaRecord& param)
{
    switch (param.task) {
        case Task::create:
            convertPgn2Sql(param);
            break;
        case Task::bench:
            bench(param);
            break;
        case Task::query:
            query(param, param.queries);
            break;
        case Task::getgame:
            getGame(param);
            break;

        default:
            break;
    }
}

void Builder::convertPgn2Sql(const ParaRecord& _paraRecord)
{
    // Prepare
    setDatabasePath(paraRecord.dbPath);
    
    // remove old db file if existed
    std::remove(paraRecord.dbPath.c_str());

    startTime = getNow();

    // options
    {
        paraRecord = _paraRecord;

        int movebit = paraRecord.optionFlag & (create_flag_moves|create_flag_moves1|create_flag_moves2);
        
        if (!movebit) {
            std::cout << "WARNING: there is not any column for storing moves" << std::endl;
        } else if (movebit != create_flag_moves && movebit != create_flag_moves1 && movebit != create_flag_moves2) {
            std::cout << "WARNING: redundant! There are more than one column for storing moves" << std::endl;
            
            if ((paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) == (create_flag_moves1 | create_flag_moves2)) {
                std::cout << "WARNING: redundant! There are two binary columns for storing moves. Use Moves2, discard Move1" << std::endl;
            }
        }
        
    }

    // init
    {
        gameCnt = 0;
        eventCnt = playerCnt = siteCnt = 1;
        errCnt = 0;
        
        playerIdMap.reserve(8 * 1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);

        extraFieldSet.clear();
        fieldOrderMap.clear();
        
        int idx = 0;
        for(; tagNames[idx]; ++idx) {
            fieldOrderMap[tagNames[idx]] = static_cast<int>(fieldOrderMap.size());
        }
        assert(fieldOrderMap.size() == TagIdx_Max && TagIdx_Max == idx);
        fieldOrderMap["UTCDate"] = TagIdx_Date;

        tagIdx_Moves = -1; tagIdx_MovesBlob = -1;
        if (paraRecord.optionFlag & create_flag_moves) {
            tagIdx_Moves = idx++;
            fieldOrderMap["Moves"] = tagIdx_Moves;
            extraFieldSet.insert("Moves");
        }
        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
            tagIdx_MovesBlob = idx++;
            std::string s = (paraRecord.optionFlag & create_flag_moves2) ? "Moves2" : "Moves1";
            fieldOrderMap[s] = tagIdx_MovesBlob;
            extraFieldSet.insert(s);
        }
        insertGameStatementIdxSz = idx;

        
        // Create database
        mDb = createDb(paraRecord.dbPath);
        if (!mDb) {
            return;
        }

        auto cpu = paraRecord.cpuNumber;
        if (cpu < 0) cpu = std::thread::hardware_concurrency();
        pool = new thread_pool(cpu);
        
        std::cout << "Thread count: " << pool->get_thread_count() << std::endl;
    }

    uint64_t cnt = 0;
    for(auto && path : paraRecord.pgnPaths) {
        cnt += processPgnFile(path);
    }
    
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

        if (playerInsertStatement) delete playerInsertStatement;
        playerInsertStatement = nullptr;
        if (eventInsertStatement) delete eventInsertStatement;
        eventInsertStatement = nullptr;
        if (siteInsertStatement) delete siteInsertStatement;
        siteInsertStatement = nullptr;
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

int Builder::addNewField(const char* fieldName)
{
    std::string s = fieldName, s0 = s;
    bslib::Funcs::toLower(s);
    
    if (extraFieldSet.find(s) == extraFieldSet.end()) {
        s = fieldName;
        try {
            mDb->exec("ALTER TABLE Games ADD COLUMN " + s + " TEXT");
        } catch (std::exception& e) {}

        auto order = -1;
        auto it2 = fieldOrderMap.find(s);
        if (it2 != fieldOrderMap.end()) {
            order = it2->second;
        } else {
            for(auto && fo : fieldOrderMap) {
                order = std::max(order, fo.second);
            }
            ++order;
            fieldOrderMap[s] = order;
            fieldOrderMap[s0] = order;
        }

        extraFieldSet.insert(s);
        extraFieldSet.insert(s0);
        insertGameStatementIdxSz++;

        assert(order > 0);
        return order;
    }
    return -1;
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
                            
                            if (paraRecord.task == Task::create) {
                                threadAddGame(tagMap, moves);
                            } else {
                                threadQueryGame(tagMap, moves);
                            }
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
        if (paraRecord.task == Task::create) {
            threadAddGame(tagMap, moves);
        } else {
            threadQueryGame(tagMap, moves);
        }
    }
}

uint64_t Builder::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;

    auto transactionCnt = 0;

    {
        char *buffer = (char*)malloc(blockSz + 16);

        FILE *stream = fopen(path.c_str(), "r");
        assert(stream != NULL);
        auto size = bslib::Funcs::getFileSize(stream);
        
        blockCnt = processedPgnSz = 0;
        for (size_t sz = 0, idx = 0; sz < size && gameCnt < paraRecord.gameNumberLimit; idx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (fread(buffer, k, 1, stream)) {
                if (mDb && transactionCnt <= 0) {
                    transactionCnt = 400;
                    mDb->exec("BEGIN");
                    std::cout << "BEGIN TRANSACTION" << std::endl;
                }

                blockCnt++;
                processedPgnSz += k;
                processDataBlock(buffer, k, true);
                pool->wait_for_tasks();

                transactionCnt--;
                if (mDb && transactionCnt <= 0) {
                    mDb->exec("COMMIT");
                    std::cout << "COMMIT TRANSACTION" << std::endl;
                }

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
    
    if (mDb && transactionCnt > 0) {
        mDb->exec("COMMIT");
    }

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
        
        std::string sqlstring0 =
            "CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, "\
           "Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, "\
        "Result INTEGER, TimeControl TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT";
        
        if (paraRecord.optionFlag & create_flag_moves) {
            sqlstring0 += ", Moves TEXT";
        }
        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
            auto sz = (paraRecord.optionFlag & create_flag_moves2) ? 2 : 1;
            sqlstring0 += ", Moves" + std::to_string(sz) + " BLOB DEFAULT NULL";
        }

        std::string sqlstring1 =
           ", FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players)";

        
        mDb->exec(sqlstring0 + sqlstring1);

        mDb->exec("DROP TABLE IF EXISTS Comments");
        mDb->exec("CREATE TABLE Comments (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INTEGER, Ply INTEGER, Comment TEXT)");


        mDb->exec("PRAGMA journal_mode=OFF");
//        mDb->exec("PRAGMA synchronous=OFF");
//        mDb->exec("PRAGMA cache_size=64000");

        // prepared statements
        playerInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
        eventInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
        siteInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");

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

IDInteger Builder::getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap)
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

void doAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(builder);
    builder->addGame(itemMap, moveText);
}

void doQueryGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(builder);
    builder->queryGame(itemMap, moveText);
}

void Builder::threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    pool->submit(doAddGame, itemMap, moveText);
}

void Builder::threadQueryGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    pool->submit(doQueryGame, itemMap, moveText);
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

static const std::string sourceFieldName = "Source";
static const char* lichessURL = "https://lichess.org/";
static const int lichessURLLength = 20;

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
    
    std::unordered_map<int, const char*> stringMap;
    std::unordered_map<int, int> intMap;

    auto whiteElo = 0, blackElo = 0, plyCount = 0;
    char* whiteName = nullptr, *blackName = nullptr, *source = nullptr;
    std::string fenString;

    for(auto && it : itemMap) {
        auto s = it.second;

        auto it2 = fieldOrderMap.find(it.first);
        if (it2 != fieldOrderMap.end()) {
            while(*s <= ' ' && *s > 0) s++; // trim left
            assert(strlen(s) < 1024);

            switch (it2->second) {
                case TagIdx_Event:
                {
                    intMap[TagIdx_Event] = getEventNameId(s);
                    break;
                }
                case TagIdx_Site:
                {
                    if (paraRecord.optionFlag & create_flag_discard_sites) {
                        intMap[TagIdx_Site] = 1; // empty
                        break;
                    }

                    // detect Lichess site, it is actually URL of the game, change to Source
                    if (memcmp(s, lichessURL, lichessURLLength) == 0 && strlen(s) > lichessURLLength + 5) {
                        source = s;
                        
                        intMap[TagIdx_Site] = 1; // empty for Site

                        // change content to column Source
                        if (extraFieldSet.find(sourceFieldName) == extraFieldSet.end()) {
                            std::lock_guard<std::mutex> dolock(tagFieldMutex);
                            auto order = addNewField(sourceFieldName.c_str());
                            if (order >= TagIdx_Max) {
                                stringMap[order] = s;
                                break;
                            }
                        }
                        auto it2 = fieldOrderMap.find(sourceFieldName);
                        if (it2 != fieldOrderMap.end()) {
                            auto order = it2->second;
                            assert(order > TagIdx_Max && order < fieldOrderMap.size());
                            stringMap[order] = s;
                        }

                        break;
                    }
                    intMap[TagIdx_Site] = getSiteNameId(s);
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

                case TagIdx_FEN:
                    fenString = s;
                    stringMap[TagIdx_FEN] = s;
                    break;

                case TagIdx_WhiteElo:
                {
                    whiteElo = std::atoi(s);
                    if (whiteElo > 0) {
                        intMap[TagIdx_WhiteElo] = whiteElo;
                    }
                    break;
                }
                case TagIdx_BlackElo:
                {
                    blackElo = std::atoi(s);
                    if (blackElo > 0) {
                        intMap[TagIdx_BlackElo] = blackElo;
                    }
                    break;
                }
                case TagIdx_PlyCount:
                {
                    plyCount = std::atoi(s);
                    if (paraRecord.limitLen > plyCount) {
                        return false;
                    }
                    break;
                }

                case TagIdx_Round:
                {
                    intMap[TagIdx_Round] = std::atoi(s);
                    break;
                }

                case TagIdx_Date:
                {
                    standardizeDate(s);
                }

                default:
//                case TagIdx_Result:
//                case TagIdx_Timer:
//                case TagIdx_ECO:
                {
                    stringMap[it2->second] = s;
                    break;
                }
            }

            continue;
        }
        
        if (strcmp(it.first, "Variant") == 0) {
            auto variant = bslib::Funcs::string2ChessVariant(it.second);
            // at this moment, support only the standard variant
            assert(variant != bslib::ChessVariant::standard);

            if (variant != bslib::ChessVariant::standard) {
                t->errCnt++;
                return false;
            }
        }
        
        if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
            return false;
        }
        
        if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
            return false;
        }
        
        if ((paraRecord.optionFlag & create_flag_accept_new_tags) && strcmp(it.first, "SetUp") != 0) {
            if (t->insertGameStatement) {
                delete t->insertGameStatement;
                t->insertGameStatement = nullptr;
            }

            std::lock_guard<std::mutex> dolock(tagFieldMutex);
            auto order = addNewField(it.first);
            assert(order > TagIdx_Max && order < fieldOrderMap.size());
            stringMap[order] = s;
        }
    }
    
    try {
        if (!t->insertGameStatement || t->insertGameStatementIdxSz != insertGameStatementIdxSz) {
            std::lock_guard<std::mutex> dolock(tagFieldMutex);
            t->createInsertGameStatement(mDb, fieldOrderMap);
        }

        t->insertGameStatement->reset();
        t->insertGameStatement->clearBindings();

        intMap[TagIdx_White] = getPlayerNameId(whiteName, whiteElo);
        intMap[TagIdx_Black] = getPlayerNameId(blackName, blackElo);

        IDInteger gameID;
        {
            std::lock_guard<std::mutex> dolock(gameMutex);
            ++gameCnt;
            gameID = gameCnt;
        }
        intMap[TagIdx_GameID] = gameID;

        if (tagIdx_Moves >= 0) {
            assert(paraRecord.optionFlag & create_flag_moves);

            // trim left
            while(*moveText <= ' ') moveText++;
            stringMap[tagIdx_Moves] = moveText;
        }

        // Parse moves
        if (tagIdx_MovesBlob >= 0) {
            assert(paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2));
            //assert(t->board);
            t->board->newGame(fenString);

            int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;
            
            if (paraRecord.optionFlag & create_flag_discard_comments) {
                flag |= bslib::BoardCore::ParseMoveListFlag_discardComment;
            }

            t->board->fromMoveList(gameID, &itemMap, moveText, bslib::Notation::san, flag);

            plyCount = t->board->getHistListSize();

            if (paraRecord.limitLen > plyCount) {
                return false;
            }

            if (plyCount > 0) {
                auto p = t->buf;
                for(auto i = 0; i < plyCount; i++) {
                    auto h = t->board->_getHistPointerAt(i);
                    auto move = h->move;
                    
                    if (paraRecord.optionFlag & create_flag_moves2) { // 2 bytes encoding
                        *(int16_t*)p = bslib::ChessBoard::encode2Bytes(move);
                        p += 2;
                    } else if (paraRecord.optionFlag & create_flag_moves1) {
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
                t->insertGameStatement->bind(tagIdx_MovesBlob + 1, t->buf, cnt);
            }
        }

        t->hdpLen += plyCount;
        if (plyCount > 0) {
            intMap[TagIdx_PlyCount] = plyCount;
        }

        for(auto && it : stringMap) {
            if (it.first >= 0) {
                t->insertGameStatement->bind(it.first + 1, it.second);
            }
        }
        for(auto && it : intMap) {
            if (it.first >= 0) {
                t->insertGameStatement->bind(it.first + 1, it.second);
            }
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

bool Builder::queryGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    auto threadId = std::this_thread::get_id();
    auto t = &threadMap[threadId];
    t->init(mDb);
    assert(t->board);

    std::string fenString;
    for(auto && it : itemMap) {
        if (strcmp(it.first, "FEN") == 0) {
            fenString = it.second;
            break;
        }
    }

    IDInteger gameID;
    {
        std::lock_guard<std::mutex> dolock(gameMutex);
        ++gameCnt;
        gameID = gameCnt;
    }

    // Parse moves
    {
        //assert(t->board);
        t->board->newGame(fenString);

        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                    | bslib::BoardCore::ParseMoveListFlag_discardComment
                    | bslib::BoardCore::ParseMoveListFlag_create_bitboard;

        if (paraRecord.optionFlag & query_flag_print_pgn) {
            flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
        }
        t->board->fromMoveList(gameID, &itemMap, moveText, bslib::Notation::san, flag, checkToStop);
    }

    return true;
}


int popCount(uint64_t x) {
   int count = 0;
   while (x) {
       count++;
       x &= x - 1; // reset LS1B
   }
   return count;
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
    assert(!moveText.empty() || !moveVec.empty());

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
    
    int flag = bslib::BoardCore::ParseMoveListFlag_create_bitboard;
    if (searchField == SearchField::moves) { // there is a text move only
        flag |= bslib::BoardCore::ParseMoveListFlag_quick_check;
        t->board->fromMoveList(gameID, nullptr, moveText, bslib::Notation::san, flag, checkToStop);

    } else {
        
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }
        
        t->board->fromMoveList(gameID, nullptr, moveVec, flag, checkToStop);
    }

    t->gameCnt++;
    t->hdpLen += t->board->getHistListSize();
}


void Builder::searchPosition(SQLite::Database* db, const std::vector<std::string>& pgnPaths, const std::string& query)
{
    assert(paraRecord.task != Task::create);
    auto parser = new Parser;
    if (!parser->parse(query.c_str())) {
        std::cerr << "Error: " << parser->getErrorString() << std::endl;
        delete parser;
        return;
    }
    
    auto startTime = getNow();
    
    // check if there at least a move fields (Moves, Moves1 or Moves2)
    if (db) {
        searchField = SearchField::none;

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
    

    checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board, const std::unordered_map<char*, char*>* itemMap) -> bool {
        assert(board && bitboardVec.size() >= 11);
        
        if (parser->evaluate(bitboardVec)) {
            succCount++;
            
            if (paraRecord.optionFlag & query_flag_print_all) {
                std::cout << succCount << ". gameId: " << gameId;
                if (paraRecord.optionFlag & query_flag_print_fen) {
                    std::cout << ", fen: " << board->getFen();
                }
                if (paraRecord.optionFlag & query_flag_print_pgn) {
                    std::cout << "\n\n" << board->toPgn(itemMap);
                }
                std::cout << std::endl;
            }
            return true;
        }

        return false;
    };

    
    std::string moveFieldName = "Moves";
    if (searchField == SearchField::moves1) {
        moveFieldName = "Moves1";
    } else if (searchField == SearchField::moves2) {
        moveFieldName = "Moves2";
    }

    std::cout << "Search with query " << query <<  "..."
    << ", searchField: " << moveFieldName
    << std::endl;

    succCount = 0; gameCnt = 0;

    if (db) {
        SQLite::Statement statement(*db, "SELECT * FROM Games");
        for (; statement.executeStep(); gameCnt++) {
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

            if (succCount >= paraRecord.resultNumberLimit) {
                break;
            }
        }
        pool->wait_for_tasks();

    } else {
        assert(!pgnPaths.empty());
        for(auto && path : pgnPaths) {
            processPgnFile(path);
        }
    }

    // Done, retrieve some last stats
    int64_t parsedGameCnt = 0, allHdpLen = 0;
    for(auto && t : threadMap) {
        parsedGameCnt += t.second.gameCnt;
        allHdpLen += t.second.hdpLen;
    }

    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    
    std::cout << std::endl << query << " DONE. Elapsed: " << elapsed << " ms, "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", total games: " << gameCnt
              << ", total results: " << succCount
              << ", time per results: " << elapsed / std::max<int64_t>(1, succCount)  << " ms"
              << std::endl << std::endl << std::endl;
    
    delete parser;
}

void Builder::bench(const ParaRecord& paraRecord)
{
    std::cout << "Benchmark position searching..." << std::endl;

    const std::vector<std::string> queries {
        "Q = 3",                            // three White Queens
        "r[e4, e5, d4,d5]= 2",              // two black Rooks in middle squares
        "P[d4, e5, f4, g4] = 4 and kb7",    // White Pawns in d4, e5, f4, g4 and black King in b7
        "B[c-f] + b[c-f] == 2",               // There are two Bishops (any side) from column c to f
        "white6 = 5",                        // There are 5 white pieces on row 6
    };

    query(paraRecord, queries);
}

void Builder::query(const ParaRecord& _paraRecord, const std::vector<std::string>& queries)
{
    assert(paraRecord.task != Task::create);
    
    paraRecord = _paraRecord;
    gameCnt = 0;
    eventCnt = playerCnt = siteCnt = 1;
    errCnt = 0;

    // Reading all data, parsing moves, multi threads
    auto cpu = paraRecord.cpuNumber;
    if (cpu < 0) cpu = std::thread::hardware_concurrency();
    pool = new thread_pool(cpu);
    std::cout << "Thread count: " << pool->get_thread_count() << std::endl;

    auto ok = false;
    if (paraRecord.dbPath.empty()) {
        if (!paraRecord.pgnPaths.empty()) {
            ok = true;
            for(auto && s : queries) {
                searchPosition(nullptr, paraRecord.pgnPaths, s);
            }
        }
    } else {
        ok = true;
        SQLite::Database db(paraRecord.dbPath, SQLite::OPEN_READWRITE);

        for(auto && s : queries) {
            searchPosition(&db, std::vector<std::string>(), s);
        }
    }

    delete pool;
    
    if (!ok) {
        std::cout << "Error: there is no path for database nor PGN files" << std::endl;
    }

    std::cout << "Completed! " << std::endl;
}

/*
 This function is just an example how to query and extract data from a record with a given game ID
 */
void Builder::queryGameDataByID(SQLite::Database& db, int gameID)
{
    std::string str =
                    "SELECT g.*, w.Name White, b.Name Black " \
                    "FROM Games g " \
                    "INNER JOIN Players w ON WhiteID = w.ID " \
                    "INNER JOIN Players b ON BlackID = b.ID " \
                    "WHERE g.ID = ?";

    SQLite::Statement query(db, str);
    query.bind(1, gameID);

    if (!query.executeStep()) {
        std::cerr << "Error: Cannot retrieve record with game ID: " << gameID << std::endl;
        return;
    }
    
    std::string fenString;
    std::set<std::string> moveNameSet;

    /*
       Warning: It is easier to work with map<std::string, std::string>.
       However, for speeding of the parser, we use map<char*, char*> instead.
       Thus, here is a bit more completed when we have to store string in an buffer first
     */
    std::unordered_map<char*, char*> itemMap;
    char * buf = (char*) malloc(1024 * 8), *p = buf;

    for(int i = 0, cnt = query.getColumnCount(); i < cnt; ++i) {
        auto c = query.getColumn(i);
        std::string name = c.getName();
        if (name == "Moves" || name == "Moves1" || name == "Moves2") {
            moveNameSet.insert(name);
            continue;
        }
        
        // Ignore all ID column such as ID, WhiteID, BlackID, EventID...
        auto nameLen = name.size();
        if (nameLen >= 2 && strcmp(name.c_str() + nameLen - 2, "ID") == 0) {
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
                if (name == "FEN") {
                    fenString = str;
                }
                break;
            }

            default:
                assert(0);
                break;
        }

        if (name != "Event" && str.empty()) {
            continue;
        }
        auto p0 = p;
        strcpy(p, name.c_str());
        p += nameLen + 1;
        itemMap[p0] = p;
        strcpy(p, str.c_str());
        p += str.size() + 1;
    }
    
    auto board = Builder::createBoard(bslib::ChessVariant::standard);
    board->newGame(fenString);
    
    if (moveNameSet.find("Moves1") != moveNameSet.end() || moveNameSet.find("Moves2") != moveNameSet.end()) {
        auto moveName = "Moves1";
        
        int flag = bslib::BoardCore::ParseMoveListFlag_create_san;
        
        if (moveNameSet.find("Moves2") == moveNameSet.end()) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        } else {
            moveName = "Moves2";
        }

        auto c = query.getColumn(moveName);
        auto moveBlob = static_cast<const int8_t*>(c.getBlob());
        
        if (moveBlob) {
            std::vector<int8_t> moveVec;
            auto sz = c.size();
            for(auto i = 0; i < sz; ++i) {
                moveVec.push_back(moveBlob[i]);
            }
            
            board->fromMoveList(gameID, nullptr, moveVec, flag, nullptr);
        }
    } else {
        assert(moveNameSet.find("Moves") != moveNameSet.end());
        auto moveText = query.getColumn("Moves").getString();
        
        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                    | bslib::BoardCore::ParseMoveListFlag_discardComment
                    | bslib::BoardCore::ParseMoveListFlag_create_san;
        board->fromMoveList(gameID, nullptr, moveText.c_str(), bslib::Notation::san, flag, nullptr);
    }
    
    std::cout << "Game retrieved successfully, PGN:\n" << board->toPgn(&itemMap) << std::endl;

    delete buf;
    delete board;
}


void Builder::getGame(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;
    SQLite::Database db(paraRecord.dbPath, SQLite::OPEN_READWRITE);
    queryGameDataByID(db, paraRecord.gameID);
}
