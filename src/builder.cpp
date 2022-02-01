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

bool debugMode = false;

using namespace ocgdb;

Builder* builder = nullptr;

static const std::string fullGameQueryString =
    "SELECT e.Name Event, s.Name Site, w.Name White, b.Name Black, g.* " \
    "FROM Games g " \
    "INNER JOIN Players w ON WhiteID = w.ID " \
    "INNER JOIN Players b ON BlackID = b.ID " \
    "INNER JOIN Events e ON EventID = e.ID " \
    "INNER JOIN Sites s ON SiteID = s.ID";

bool ParaRecord::isValid() const
{
    if (dbPaths.empty() && task != Task::query) {
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
            if (!hasPgn) {
                errorString = "Must have at least one PGN path. Mising or wrong parameter -pgn";
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
            if (!hasPgn || pgnPaths.size() != 1) {
                errorString = "Must have one PGN path. Mising or wrong parameter -pgn";
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
        case Task::bench:
        {
            ok = true;
            break;
        }
        case Task::getgame:
        {
            if (gameIDVec.empty()) {
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
static const std::string searchFieldNames[] = {
    "",
    "Moves",
    "Moves1",
    "Moves2",
    ""
};

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

    {"remove", 10},
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
            
            if (s == "printpgn" || s == "printfen") {
                auto it2 = optionNameMap.find("printall");
                assert(it2 != optionNameMap.end());
                optionFlag |= 1 << it2->second;
            }
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
    deleteAllStatements();
}

void ThreadRecord::deleteAllStatements()
{
    if (insertGameStatement) delete insertGameStatement;
    if (insertCommentStatement) delete insertCommentStatement;
    if (removeGameStatement) delete removeGameStatement;
    insertGameStatement = nullptr;
    insertCommentStatement = nullptr;
    removeGameStatement = nullptr;
}

void ThreadRecord::resetStats()
{
    errCnt = gameCnt = hdpLen = dupCnt = delCnt = 0;
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
              << ", speed: " << gameCnt * 1000ULL / elapsed
              << " games/s";
    
    
    switch (paraRecord.task) {
        case Task::create:
            std::cout << ", #blocks: " << blockCnt << ", processed size: " << processedPgnSz / (1024 * 1024ULL) << " MB";
            break;
        case Task::dup:
        {
            int64_t dupCnt = 0, delCnt = 0;
            for(auto && t : threadMap) {
                dupCnt += t.second.dupCnt;
                delCnt += t.second.delCnt;
            }
            std::cout << ", #duplicates: " << dupCnt << ", #removed: " << delCnt;
            break;
        }

        default:
            break;
    }

    std::cout << std::endl;
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
        case Task::merge:
            mergeDatabases(param);
            break;
        case Task::export_:
            convertSql2Pgn(param);
            break;
        case Task::dup:
            findDuplicatedGames(param);
            break;
        case Task::bench:
            bench(param);
            break;
        case Task::query:
            searchPostion(param, param.queries);
            break;
        case Task::getgame:
            getGame(param);
            break;

        default:
            break;
    }
}

void Builder::createPool()
{
    auto cpu = paraRecord.cpuNumber;
    if (cpu < 0) cpu = std::thread::hardware_concurrency();
    pool = new thread_pool(cpu);
    
    std::cout << "Thread count: " << pool->get_thread_count() << std::endl;

}

void Builder::convertPgn2Sql(const ParaRecord& _paraRecord)
{
    std::cout   << "Convert PGN files into a database..." << std::endl;

    paraRecord = _paraRecord;

    // Prepare
    assert(!paraRecord.dbPaths.empty());
    auto dbPath = paraRecord.dbPaths.front();
    setDatabasePath(dbPath);
    
    // remove old db file if existed
    std::remove(dbPath.c_str());

    startTime = getNow();

    // options
    {
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
        mDb = createDb(dbPath);
        if (!mDb) {
            return;
        }

        createPool();
    }

    uint64_t cnt = 0;
    for(auto && path : paraRecord.pgnPaths) {
        cnt += processPgnFile(path);
    }
    
    {
        delete pool;
        pool = nullptr;
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
                    // std::cout << "BEGIN TRANSACTION" << std::endl;
                }

                blockCnt++;
                processedPgnSz += k;
                processDataBlock(buffer, k, true);
                pool->wait_for_tasks();

                transactionCnt--;
                if (mDb && transactionCnt <= 0) {
                    mDb->exec("COMMIT");
                    // std::cout << "COMMIT TRANSACTION" << std::endl;
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
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Version', '0.9')");
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

void Builder::threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    pool->submit(doAddGame, itemMap, moveText);
}



void doQueryGame(const bslib::PgnRecord& record)
{
    assert(builder);
    builder->queryGame(record);
}

void Builder::threadQueryGame(const bslib::PgnRecord& record)
{
    pool->submit(doQueryGame, record);
}

void Builder::threadQueryGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    bslib::PgnRecord record;
    record.moveText = moveText;
    
    for(auto && it : itemMap) {
        auto name = std::string(it.first), s = std::string(it.second);
        if (name == "FEN") {
            record.fenText = s;
        }
        record.tags[name] = s;
    }
    
    threadQueryGame(record);
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
    ThreadRecord* t;
    {
        std::lock_guard<std::mutex> dolock(threadMapMutex);
        t = &threadMap[threadId];
    }
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

            bslib::PgnRecord record;
            record.moveText = moveText;
            record.gameID = gameID;
            t->board->fromMoveList(&record, bslib::Notation::san, flag);

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

bool Builder::queryGame(const bslib::PgnRecord& record)
{
    auto threadId = std::this_thread::get_id();
    auto t = &threadMap[threadId];
    t->init(mDb);
    assert(t->board);

    IDInteger gameID;
    {
        std::lock_guard<std::mutex> dolock(gameMutex);
        ++gameCnt;
        gameID = gameCnt;
    }

    // Parse moves
    {
        //assert(t->board);
        t->board->newGame(record.fenText);

        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                    | bslib::BoardCore::ParseMoveListFlag_discardComment
                    | bslib::BoardCore::ParseMoveListFlag_create_bitboard;

        if (paraRecord.optionFlag & query_flag_print_pgn) {
            flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
        }
        t->board->fromMoveList(&record, bslib::Notation::san, flag, checkToStop);
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


void doSearchPostion(const bslib::PgnRecord& record,
                     const std::vector<int8_t>& moveVec)
{
    assert(builder);
    builder->searchPosition(record, moveVec);
}

void Builder::threadSearchPosition(const bslib::PgnRecord& record,
                                   const std::vector<int8_t>& moveVec)
{
    pool->submit(doSearchPostion, record, moveVec);
}

void Builder::searchPosition(const bslib::PgnRecord& record,
                             const std::vector<int8_t>& moveVec)
{
    assert(!record.moveString.empty() || record.moveText || !moveVec.empty());

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
    
    t->board->newGame(record.fenText);
    
    int flag = bslib::BoardCore::ParseMoveListFlag_create_bitboard;
    if (searchField == SearchField::moves) { // there is a text move only
        flag |= bslib::BoardCore::ParseMoveListFlag_quick_check;
        t->board->fromMoveList(&record, bslib::Notation::san, flag, checkToStop);

    } else {
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }
        
        if (paraRecord.optionFlag & query_flag_print_pgn) {
            flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
        }
        t->board->fromMoveList(&record, moveVec, flag, checkToStop);
    }

    t->gameCnt++;
    t->hdpLen += t->board->getHistListSize();
}

Builder::SearchField Builder::getMoveField(SQLite::Database* db)
{
    assert(db);
    auto searchField = SearchField::none;

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

    return searchField;
}

void extractHeader(SQLite::Statement& query, bslib::PgnRecord& record)
{
    for(int i = 0, cnt = query.getColumnCount(); i < cnt; ++i) {
        auto c = query.getColumn(i);
        std::string name = c.getName();
        if (name == "ID") {
            record.gameID = c.getInt();
            continue;
        }

        // Ignore all ID columns and Moves, Moves1, Moves2
        if (name == "EventID" || name == "SiteID"
            || name == "WhiteID" || name == "BlackID"
            || name == "Moves" || name == "Moves1" || name == "Moves2") {
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
                if (name == "FEN") record.fenText = str;
                break;
            }

            default:
                assert(0);
                break;
        }

        if (name != "Event" && str.empty()) {
            continue;
        }
        record.tags[name] = str;
    }
}

void Builder::searchPosition(SQLite::Database* db, const std::vector<std::string>& pgnPaths, std::string query)
{
    // remove comments by //
    if (query.find("//") != std::string::npos) {
        while(true) {
            auto p = query.find("//");
            if (p == std::string::npos) {
                break;
            }
            
            auto q = p + 2;
            for(; q < query.size(); q++) {
                auto ch = query.at(q);
                if (ch == '\n') {
                    q++;
                    break;
                }
            }
            
            auto s = query.substr(0, p);
            if (q >= query.size()) {
                query = s;
                break;
            } else {
                auto s2 = query.substr(q);
                query = s + s2;
            }
        }
    }
    
    bslib::Funcs::trim(query);

    if (query.empty()) {
        return;
    }

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
        searchField = getMoveField(db);

        if (searchField <= SearchField::none) {
            std::cerr << "FATAL ERROR: missing move field (Moves or Moves1 or Moves2)" << std::endl;
            return;
        }
    }
    

    checkToStop = [=](const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board, const bslib::PgnRecord* record) -> bool {
        assert(board && bitboardVec.size() >= 11);
        
        if (parser->evaluate(bitboardVec)) {
            succCount++;
            
            if (paraRecord.optionFlag & query_flag_print_all) {
                std::cout << succCount << ". gameId: " << (record ? record->gameID : -1);
                if (paraRecord.optionFlag & query_flag_print_fen) {
                    std::cout << ", fen: " << board->getFen();
                }
                if (paraRecord.optionFlag & query_flag_print_pgn) {
                    std::cout << "\n\n" << board->toPgn(record);
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
        
        auto str = (paraRecord.optionFlag & query_flag_print_pgn) ?
        fullGameQueryString : "SELECT * FROM Games";
        
        SQLite::Statement statement(*db, str);
        for (; statement.executeStep(); gameCnt++) {
            bslib::PgnRecord record;
            record.gameID = statement.getColumn("ID").getInt();
            record.fenText = statement.getColumn("FEN").getText();
            
            std::vector<int8_t> moveVec;

            if (searchField == SearchField::moves) {
                // Don't use moveText because when a thread starts working, the statement maybe destroyed
                record.moveString = statement.getColumn("Moves").getText();
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
            
            if (paraRecord.optionFlag & query_flag_print_pgn) {
                extractHeader(statement, record);
            }
            threadSearchPosition(record, moveVec);

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

    searchPostion(paraRecord, queries);
}

void Builder::searchPostion(const ParaRecord& _paraRecord, const std::vector<std::string>& queries)
{
    std::cout   << "Querying..." << std::endl;
    
    paraRecord = _paraRecord; assert(paraRecord.task != Task::create);
    gameCnt = 0;
    eventCnt = playerCnt = siteCnt = 1;
    errCnt = 0;

    // Reading all data, parsing moves, multi threads
    createPool();

    auto ok = false;
    if (paraRecord.dbPaths.empty()) {
        if (!paraRecord.pgnPaths.empty()) {
            ok = true;
            for(auto && s : queries) {
                searchPosition(nullptr, paraRecord.pgnPaths, s);
            }
        }
    } else {
        ok = true;
        SQLite::Database db(paraRecord.dbPaths.front(), SQLite::OPEN_READWRITE);

        for(auto && s : queries) {
            searchPosition(&db, std::vector<std::string>(), s);
        }
    }

    if (!ok) {
        std::cout << "Error: there is no path for database nor PGN files" << std::endl;
    }

    std::cout << "Completed! " << std::endl;
}


bool Builder::queryGameData(SQLite::Statement& query, SQLite::Statement* queryComments, std::string* toPgnString, bslib::BoardCore* board, char* tmpBuf, SearchField searchField)
{

    if (!query.executeStep()) {
        return false;
    }
    
    bslib::PgnRecord record;
    extractHeader(query, record);
    board->newGame(record.fenText);
    
    // Tables games may have 0-2 columns for moves
    if (searchField == SearchField::moves1 || searchField == SearchField::moves2) {
        auto moveName = "Moves1";
        
        int flag = bslib::BoardCore::ParseMoveListFlag_create_san;
        
        if (searchField == SearchField::moves1) {
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
            
            board->fromMoveList(&record, moveVec, flag, nullptr);
            
            if (queryComments) {
                queryComments->reset();
                queryComments->bind(1, record.gameID);
                while (queryComments->executeStep()) {
                    auto ply = queryComments->getColumn("Ply").getInt();
                    auto comment = queryComments->getColumn("Comment").getString();
                    if (!comment.empty() && ply >= 0 && ply < board->getHistListSize()) {
                        board->_getHistPointerAt(ply)->comment = comment;
                    }
                }
            }
        }
    } else if (searchField == SearchField::moves) {
        record.moveString = query.getColumn("Moves").getString();
        
        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                    | bslib::BoardCore::ParseMoveListFlag_discardComment
                    | bslib::BoardCore::ParseMoveListFlag_create_san;
        
        if (toPgnString) {
            flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
        }
        board->fromMoveList(&record, bslib::Notation::san, flag, nullptr);
    }
    
    if (toPgnString) {
        *toPgnString = board->toPgn(&record);
    }

    return true;
}

/*
 This function is just an example how to query and extract data from a record with a given game ID
 */
void Builder::getGameDataByID(SQLite::Database& db, const std::vector<int> gameIDVec, SearchField searchField)
{

    std::string str = fullGameQueryString + " WHERE g.ID = ?";

    SQLite::Statement query(db, str);

    SQLite::Statement* queryComments = nullptr;
    if (searchField == SearchField::moves1 || searchField == SearchField::moves1) {
        queryComments = new SQLite::Statement(db, "SELECT * FROM Comments WHERE GameID = ?");
    }

    char * buf = (char*) malloc(1024 * 8);
    auto board = Builder::createBoard(bslib::ChessVariant::standard);
    std::string toPgnString;
    
    for(auto && gameID : gameIDVec) {
        std::cout   << "Get PGN game with ID: " << gameID << std::endl;
        query.bind(1, gameID);
        if (queryGameData(query, queryComments, &toPgnString, board, buf, searchField)) {
            std::cout << "PGN:\n" << toPgnString << std::endl;
        } else {
            std::cerr << "Error: Cannot retrieve record with the game ID: " << gameID << std::endl;
        }
        query.reset();
    }

    delete buf;
    delete board;
    
    if (queryComments) {
        delete queryComments;
    }
}


void Builder::getGame(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;
    SQLite::Database db(paraRecord.dbPaths.front(), SQLite::OPEN_READWRITE);
    auto searchField = getMoveField(&db);
    getGameDataByID(db, paraRecord.gameIDVec, searchField);
}


void Builder::convertSql2Pgn(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;
    auto pgnPath = paraRecord.pgnPaths.front();
    auto dbPath = paraRecord.dbPaths.empty() ? "" : paraRecord.dbPaths.front();

    std::cout   << "Convert a database into a PGN file...\n"
                << "DB path : " << dbPath
                << "\nPGN path: " << pgnPath
                << std::endl;


    startTime = getNow();

    char * buf = (char*) malloc(1024 * 8);
    auto board = Builder::createBoard(bslib::ChessVariant::standard);
    std::string toPgnString;

    SQLite::Database db(dbPath, SQLite::OPEN_READWRITE);

    assert(!paraRecord.pgnPaths.empty());
    
#ifdef _WIN32
    std::ofstream ofs(std::filesystem::u8path(pgnPath.c_str()), std::ios_base::out | std::ios_base::app);
#else
    std::ofstream ofs(pgnPath, std::ios_base::out | std::ios_base::app);
#endif

    auto searchField = getMoveField(&db);
    SQLite::Statement query(db, fullGameQueryString);

    SQLite::Statement* queryComments = nullptr;
    if (searchField == SearchField::moves1 || searchField == SearchField::moves1) {
        queryComments = new SQLite::Statement(db, "SELECT * FROM Comments WHERE GameID = ?");
    }
    
    for (gameCnt = 0; queryGameData(query, queryComments, &toPgnString, board, buf, searchField); ++gameCnt) {
        ofs << toPgnString << "\n" << std::endl;

        if (gameCnt && (gameCnt & 0xffff) == 0) {
            printStats();
        }
    }

    ofs.close();

    if (queryComments) delete queryComments;
    delete buf;
    delete board;

    printStats();
    std::cout << "Completed! " << std::endl;
}


void Builder::mergeDatabases(const ParaRecord& _paraRecord)
{
}


void doCheckDupplication(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(builder);
    builder->checkDuplicates(record, moveVec);
}

void Builder::threadCheckDupplication(const bslib::PgnRecord& record,
                                   const std::vector<int8_t>& moveVec)
{
    pool->submit(doCheckDupplication, record, moveVec);
}

void Builder::checkDuplicates(const bslib::PgnRecord& record,
                             const std::vector<int8_t>& moveVec)
{
    assert(!record.moveString.empty() || record.moveText || !moveVec.empty());
    assert(record.gameID > 0);

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
    
    t->board->newGame(record.fenText);
    
    int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;
    if (searchField == SearchField::moves) { // there is a text move only
        t->board->fromMoveList(&record, bslib::Notation::san, flag, nullptr);
    } else {
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }
        
        t->board->fromMoveList(&record, moveVec, flag, nullptr);
    }

    t->gameCnt++;
    
    auto lastHashKey = t->board->hashKey;
    {
        std::lock_guard<std::mutex> dolock(dupHashKeyMutex);

        if (paraRecord.optionFlag & query_flag_print_all) {
            auto it = hashGameIDMap.find(lastHashKey);
            if (it == hashGameIDMap.end()) {
                hashGameIDMap[lastHashKey] = record.gameID;
                return;
            }
            std::cerr << "Duplicate games detected between IDs " << it->second << " and " << record.gameID << std::endl;
        } else {
            auto it = hashSet.find(lastHashKey);
            if (it == hashSet.end()) {
                hashSet.insert(lastHashKey);
                return;
            }
        }
    }
    
    t->dupCnt++;
    if (paraRecord.optionFlag & dup_flag_remove) {
        if (!t->removeGameStatement) {
            assert(mDb);
            t->removeGameStatement = new SQLite::Statement(*mDb, "DELETE FROM Games WHERE ID = ?");
        }
        
        t->removeGameStatement->reset();
        t->removeGameStatement->bind(1, record.gameID);
        t->removeGameStatement->exec();
        t->delCnt++;
    }
}

void Builder::findDuplicatedGames(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;
    createPool();
    
    for(auto && dbPath : paraRecord.dbPaths) {
        std::cout   << "Detect duplicate games...\n"
                    << "DB path : " << dbPath
                    << std::endl;

        startTime = getNow();

        mDb = new SQLite::Database(dbPath, SQLite::OPEN_READWRITE);
        if (!mDb) {
            std::cerr << "Error: can't open database " << dbPath << std::endl;
            continue;
        }

        if (paraRecord.optionFlag & dup_flag_remove) {
            mDb->exec("PRAGMA journal_mode=OFF");
            mDb->exec("BEGIN");
        }
        
        for(auto && t : threadMap) {
            t.second.resetStats();
        }

        searchField = getMoveField(mDb);
        if (searchField == SearchField::none) {
            continue;
        }
        
        std::string moveName = searchFieldNames[static_cast<int>(searchField)];
        assert(!moveName.empty());
        
        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check | bslib::BoardCore::ParseMoveListFlag_discardComment;
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }

        std::string sqlString = "SELECT ID, FEN, " + moveName + " FROM Games";
        auto statement = new SQLite::Statement(*mDb, sqlString);
        
        for (gameCnt = 0; statement->executeStep(); ++gameCnt) {
            bslib::PgnRecord record;
            record.gameID = statement->getColumn("ID").getInt();
            record.fenText = statement->getColumn("FEN").getText();
            std::vector<int8_t> moveVec;

            if (searchField == SearchField::moves) {
                record.moveString = statement->getColumn("Moves").getText();
                if (record.moveString.empty()) {
                    continue;
                }
            } else {
                auto c = statement->getColumn(moveName.c_str());
                auto moveBlob = static_cast<const int8_t*>(c.getBlob());
                
                if (moveBlob) {
                    auto sz = c.size();
                    for(auto i = 0; i < sz; ++i) {
                        moveVec.push_back(moveBlob[i]);
                    }
                }
                
                if (moveVec.empty()) {
                    continue;
                }
            }

            threadCheckDupplication(record, moveVec);
            
            if (gameCnt && (gameCnt & 0xffff) == 0) {
                printStats();
            }
        }

        pool->wait_for_tasks();
        
        delete statement;
        statement = nullptr;
        
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
        
        std::cout << std::endl << " DONE. Elapsed: " << elapsed << " ms, "
                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                  << ", total games: " << gameCnt
                  << ", total results: " << succCount
                  << ", time per results: " << elapsed / std::max<int64_t>(1, succCount)  << " ms"
                  << std::endl << std::endl << std::endl;

        int64_t delCnt = 0;
        {
            for(auto && t : threadMap) {
                t.second.deleteAllStatements();
                delCnt += t.second.delCnt;
            }
            
            // Update table Info
            if (delCnt > 0) {
                int64_t gCnt = gameCnt - delCnt;
                
                std::string sqlstr = "UPDATE Info SET Value = '" + std::to_string(gCnt) + "' WHERE Name = 'GameCount'";
//                std::cout << "sqlstr: " << sqlstr << std::endl;
                mDb->exec(sqlstr);

                mDb->exec("COMMIT");
            }

            if (mDb) delete mDb;
            mDb = nullptr;
        }
    }


    printStats();
    std::cout << "Completed! " << std::endl;
}
