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
Report printOut;


//////////////////////////////////
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
    
    std::lock_guard<std::mutex> dolock(printMutex);

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

void Builder::runTask(const ParaRecord& param)
{
    paraRecord = param;
    printOut.init((paraRecord.optionFlag & query_flag_print_all) && (paraRecord.optionFlag & query_flag_print_pgn), param.reportPath);
    
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
            searchPostion(param);
            break;
        case Task::getgame:
            getGame(param);
            break;

        default:
            break;
    }
    
    printOut.close();
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
        gameCnt = commentCnt = 0;
        eventCnt = playerCnt = siteCnt = 1;
        errCnt = 0;
        
        playerIdMap.reserve(8 * 1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);

        // ID, FEN, Moves, Moves1, Moves2 are special columns
        create_tagVec = {
            "ID",
            "Event", "Site", "Date", "Round",
            "White", "WhiteElo", "Black", "BlackElo",
            "Result", "TimeControl", "ECO", "PlyCount"
        };
        
        assert(TagIdx_Max == knownPgnTagVec.size());
        std::unordered_map<std::string, int> create_knownTagMap;
        for(int i = 0; i < TagIdx_Max; i++) {
            create_knownTagMap[knownPgnTagVec[i]] = i;
        }
        
        auto hasFEN = false;
        for(auto && s : create_tagVec) {
            if (s == "FEN") hasFEN = true;
            auto it = create_knownTagMap.find(s);
            if (it != create_knownTagMap.end()) {
                create_tagMap[s] = it->second;
            }
        }
        
        if (!hasFEN) {
            create_tagVec.push_back("FEN");
            create_tagMap["FEN"] = TagIdx_FEN;
        }

        auto sz = static_cast<int>(create_knownTagMap.size());
        for(auto && s : create_tagVec) {
            auto it = create_knownTagMap.find(s);
            if (it == create_knownTagMap.end()) {
                create_tagMap[s] = sz++;
            }
        }
        
        if (paraRecord.optionFlag & create_flag_moves) {
            create_tagVec.push_back("Moves");
        }
        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
            create_tagVec.push_back((paraRecord.optionFlag & create_flag_moves2) ? "Moves2" : "Moves1");
        }
        
        // Create database
        mDb = createDb(dbPath, paraRecord.optionFlag, create_tagVec);
        if (!mDb) {
            return;
        }
        createInsertStatements(*mDb);

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

        str = std::string("INSERT INTO Info (Name, Value) VALUES ('SiteCount', '") + std::to_string(siteCnt) + "')";
        mDb->exec(str);

        str = std::string("INSERT INTO Info (Name, Value) VALUES ('CommentCount', '") + std::to_string(commentCnt) + "')";
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

bool Builder::create_addNewField(const std::string& fieldName)
{
    if (create_tagMap.find(fieldName) == create_tagMap.end()) {
        std::lock_guard<std::mutex> dolock(create_tagFieldMutex);

        try {
            mDb->exec("ALTER TABLE Games ADD COLUMN " + fieldName + " TEXT");
        } catch (std::exception& e) {
            return false;
        }
        
        auto sz = static_cast<int>(create_tagVec.size());
        create_tagVec.push_back(fieldName);
        create_tagMap[fieldName] = sz;
        

//        auto order = -1;
//        auto it2 = fieldOrderMap.find(s);
//        if (it2 != fieldOrderMap.end()) {
//            order = it2->second;
//        } else {
//            for(auto && fo : fieldOrderMap) {
//                order = std::max(order, fo.second);
//            }
//            ++order;
//            fieldOrderMap[s] = order;
//            fieldOrderMap[s0] = order;
//        }
//
//        extraFieldSet.insert(s);
//        extraFieldSet.insert(s0);
//        insertGameStatementIdxSz++;
//
//        assert(order > 0);
//        return order;
    }
    return true;
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
                    // Check carefully to avoid [ in middle of a line or without tag name
                    if (*p < 'A' || *p > 'Z' || (p > buffer && *(p - 2) >= ' ')) { // if (!isalpha(*p)) {
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
        auto size = bslib::Funcs::getFileSize(path);

        std::ifstream file(path, std::ios::binary);

        if (!file || size == 0) {
            std::cerr << "Error: Can't open file: '" << path << "'" << std::endl;
            return 0;
        }
        
        blockCnt = processedPgnSz = 0;
        for (size_t sz = 0, idx = 0; sz < size && gameCnt < paraRecord.gameNumberLimit; idx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (file.read(buffer, k)) {
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

        file.close();
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


SQLite::Database* Builder::createDb(const std::string& path, int optionFlag, const std::vector<std::string>& tagVec)
{
    assert(!path.empty());

    try
    {
        // Open a database file in create/write mode
        auto mDb = new SQLite::Database(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
        std::cout << "SQLite database file '" << mDb->getFilename() << "' opened successfully\n";

        mDb->exec("DROP TABLE IF EXISTS Info");
        mDb->exec("CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT)");
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Data Structure Version', '" + VersionDatabaseString + "')");
        
        // User Data version
        mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Version', '" + VersionUserDatabaseString + "')");
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

        // Table Games
        {
            mDb->exec("DROP TABLE IF EXISTS Games");
            
            std::string sql0 = "CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT";
            std::string sql1;
            for(auto && str : tagVec) {
                if (str == "ID") {
                    continue;;
                }

                sql0 += ", " + str;

                std::string stype = "TEXT";
                
                // Special fields, using ID
                if (str == "Event" || str == "Site" || str == "White" || str == "Black") {
                    if (str == "Event") {
                        sql1 += ", FOREIGN KEY(EventID) REFERENCES Events";
                    }
                    else if (str == "Site") {
                        sql1 += ", FOREIGN KEY(SiteID) REFERENCES Sites";
                    }
                    else if (str == "White") {
                        sql1 += ", FOREIGN KEY(WhiteID) REFERENCES Players";
                    }
                    else if (str == "Black") {
                        sql1 += ", FOREIGN KEY(BlackID) REFERENCES Players";
                    }
                    sql0 += "ID";
                    stype = "INTEGER";
                } else if (str == "WhiteElo" || str == "BlackElo" || str == "PlyCount") {
                    stype = "INTEGER";
                } else if (str == "Moves1" || str == "Moves2") {
                    stype = "BLOB DEFAULT NULL";
                }
                
                sql0 += " " + stype;
            }
            
            mDb->exec(sql0 + sql1 + ")");
        }

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

bool Builder::createInsertStatements(SQLite::Database& db)
{
    try
    {
        // prepared statements
        playerInsertStatement = new SQLite::Statement(db, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
        eventInsertStatement = new SQLite::Statement(db, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
        siteInsertStatement = new SQLite::Statement(db, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");
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

int Builder::create_getPlayerNameId(char* name, int elo)
{
    std::lock_guard<std::mutex> dolock(playerMutex);
    return create_getNameId(name, elo, playerCnt, playerInsertStatement, playerIdMap);
}

int Builder::create_getEventNameId(char* name)
{
    std::lock_guard<std::mutex> dolock(eventMutex);
    return create_getNameId(name, -1, eventCnt, eventInsertStatement, eventIdMap);
}

IDInteger Builder::create_getNameId(char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement, std::unordered_map<std::string, IDInteger>& idMap)
{
    name = bslib::Funcs::trim(name);

    // null, empty, * or ?
    if (!name || *name == 0 || *name == '*' || *name == '?') return 1;

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


int Builder::create_getSiteNameId(char* name)
{
    std::lock_guard<std::mutex> dolock(siteMutex);
    return create_getNameId(name, -1, siteCnt, siteInsertStatement, siteIdMap);
}

void doAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(builder);
    builder->create_addGame(itemMap, moveText);
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


static const std::string sourceFieldName = "Source";
static const char* lichessURL = "https://lichess.org/";
static const int lichessURLLength = 20;


// This function for adding games when creating database
bool Builder::create_addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
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
    
    std::unordered_map<std::string, const char*> stringMap;
    std::unordered_map<std::string, int> intMap;

    auto whiteElo = 0, blackElo = 0, plyCount = 0;
    char* whiteName = nullptr, *blackName = nullptr;
    std::string fenString, ecoString;

    for(auto && it : itemMap) {
        auto s = it.second;

        if (strcmp(it.first, "Variant") == 0) {
            auto variant = bslib::Funcs::string2ChessVariant(it.second);
            // at this moment, support only the standard variant
            assert(variant != bslib::ChessVariant::standard);

            if (variant != bslib::ChessVariant::standard) {
                t->errCnt++;
                return false;
            }
            continue;
        }
        if (strcmp(it.first, "SetUp") == 0) {
            continue;
        }

        auto it2 = create_tagMap.find(it.first);
        if (it2 != create_tagMap.end()) {
            while(*s <= ' ' && *s > 0) s++; // trim left
            assert(strlen(s) < 1024);

            switch (it2->second) {
                case TagIdx_Event:
                {
                    intMap["EventID"] = create_getEventNameId(s);
                    break;
                }
                case TagIdx_Site:
                {
                    if (paraRecord.optionFlag & create_flag_discard_sites) {
                        intMap["SiteID"] = 1; // empty
                        break;
                    }

                    // detect Lichess site, it is actually URL of the game, change to Source
                    if (memcmp(s, lichessURL, lichessURLLength) == 0 && strlen(s) > lichessURLLength + 5) {
                        intMap["SiteID"] = 1; // empty for Site

                        // change content to column Source
                        if (create_tagMap.find(sourceFieldName) == create_tagMap.end()) {
                            if (create_addNewField(sourceFieldName)) {
                                stringMap[sourceFieldName] = s;
                            }
                            break;
                        }

                        auto it2 = create_tagMap.find(sourceFieldName);
                        if (it2 != create_tagMap.end()) {
                            stringMap[sourceFieldName] = s;
                        }

                        break;
                    }
                    intMap["SiteID"] = create_getSiteNameId(s);
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
                    if (paraRecord.optionFlag & create_flag_discard_fen) {
                        return false;
                    }
                    fenString = s;
                    stringMap["FEN"] = s;
                    break;

                case TagIdx_WhiteElo:
                {
                    whiteElo = std::atoi(s);
                    if (whiteElo > 0) {
                        intMap["WhiteElo"] = whiteElo;
                    }
                    break;
                }
                case TagIdx_BlackElo:
                {
                    blackElo = std::atoi(s);
                    if (blackElo > 0) {
                        intMap["BlackElo"] = blackElo;
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

                case TagIdx_ECO:
                    ecoString = s;

                default:
                {
                    // ignore empty string or one started with *, ?
                    auto ch = s[0];
                    if (ch == 0 || ch == '*' || ch == '?') {
                        break;
                    }

                    if (strstr(it.first, "Date")) {
                        SqlLib::standardizeDate(s);
                    }

                    stringMap[it.first] = s;
                    break;
                }
            } // switch

            continue;
        } // if (it2 != create_fieldMap.end())
        
        
        if ((paraRecord.optionFlag & create_flag_accept_new_tags) &&
            create_addNewField(it.first)) {
            if (t->insertGameStatement) {
                delete t->insertGameStatement;
                t->insertGameStatement = nullptr;
            }
            stringMap[it.first] = s;
        }
    }

    if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
        return false;
    }
    
    if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
        return false;
    }
    
//    if ((paraRecord.optionFlag & create_flag_accept_new_tags) && strcmp(it.first, "SetUp") != 0) {
//        if (t->insertGameStatement) {
//            delete t->insertGameStatement;
//            t->insertGameStatement = nullptr;
//        }
//
//        create_addNewField(it.first);
//        stringMap[it.first] = s;
//    }

    try {
        if (!t->insertGameStatement || t->insertGameStatementIdxSz != create_tagVec.size()) {
            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
            t->createInsertGameStatement(mDb, create_tagVec);
        }

        t->insertGameStatement->reset();
        t->insertGameStatement->clearBindings();

        intMap["WhiteID"] = create_getPlayerNameId(whiteName, whiteElo);
        intMap["BlackID"] = create_getPlayerNameId(blackName, blackElo);

        IDInteger gameID;
        {
            std::lock_guard<std::mutex> dolock(gameMutex);
            ++gameCnt;
            gameID = gameCnt;
        }
        intMap["ID"] = gameID;

        if (paraRecord.optionFlag & create_flag_moves) {
            // trim left
            while(*moveText <= ' ') moveText++;
            stringMap["Moves"] = moveText;
        }

        // Parse moves
        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
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
                        std::lock_guard<std::mutex> dolock(commentMutex);
                        commentCnt++;
                    }
                }
                
                auto cnt = static_cast<int>(p - t->buf);
                assert(cnt >= plyCount);
                auto bindMoves = (paraRecord.optionFlag & create_flag_moves1) ? ":Moves1" : ":Moves2";
                t->insertGameStatement->bind(bindMoves, t->buf, cnt);
                
                
                if (ecoString.empty() || (paraRecord.optionFlag & create_flag_reset_eco)) {
                    ecoString = t->board->getLastEcoString();
                    if (!ecoString.empty()) {
                        stringMap["ECO"] = ecoString.c_str();
                    }
                }

            }
        }
        
        // first comment
        if (!t->board->getFirstComment().empty()) {
            t->insertCommentStatement->reset();
            t->insertCommentStatement->bind(1, gameID);
            t->insertCommentStatement->bind(2, -1);
            t->insertCommentStatement->bind(3, t->board->getFirstComment());
            t->insertCommentStatement->exec();
            std::lock_guard<std::mutex> dolock(commentMutex);
            commentCnt++;
        }


        t->hdpLen += plyCount;
        if (plyCount > 0) {
            intMap["PlyCount"] = plyCount;
        }

        for(auto && it : stringMap) {
            t->insertGameStatement->bind(":" + it.first, it.second);
        }
        for(auto && it : intMap) {
            t->insertGameStatement->bind(":" + it.first, it.second);
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

// Query directly from databases
IDInteger Builder::getNameId(const std::string& tableName, const std::string& name, int elo)
{
    auto theName = SqlLib::encodeString(name);
    std::string sQuery = "SELECT ID FROM " + tableName + " WHERE name=\"" + theName + "\"";
    SQLite::Statement query(*mDb, sQuery.c_str());
    if (query.executeStep()) {
        return query.getColumn(0);
    }

    std::string str0 = "INSERT INTO " + tableName + "(name";
    std::string str1 = ") VALUES (\"" + theName + "\"";
    std::string str2 = ") RETURNING ID";
    
    if (elo > 0) {
        str0 += ", Elo";
        str1 += ", " + std::to_string(elo);
    }
    return mDb->execAndGet(str0 + str1 + str2);
}


// This function for adding games after creating database (existent database)
// the database maybe not loaded yet, not any information

bool Builder::addGame(const std::string& dbPath, const std::string& pgnString)
{
    if (dbPath.empty() || pgnString.empty()) return false;
    
    std::unordered_map<std::string, std::string> itemMap;
    bslib::ChessBoard board;
//    board.BoardCore::moveFromString_san(<#const std::string &#>);
    
    return addGame(dbPath, itemMap, &board);
}

bool Builder::addGame(const std::string& dbPath, const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board)
{
    mDb = new SQLite::Database(dbPath, SQLite::OPEN_READWRITE);

    if (mDb) {
    } else {
        paraRecord.optionFlag = create_flag_moves2;
        mDb = createDb(dbPath, paraRecord.optionFlag, create_tagVec);
        createInsertStatements(*mDb);
    }
    if (!mDb) {
        std::cerr << "Error: Can't open nor create database file '" << mDb->getFilename() << std::endl;
        return false;
    }
    std::cout << "SQLite database file '" << mDb->getFilename() << "' opened successfully" << std::endl;;
    
    bool hashMoves;
    searchField = SqlLib::getMoveField(mDb, &hashMoves);

    auto r = addGame(itemMap, board);
    
    return r;
}

// Single adding game
bool Builder::addGame(const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board)
{
    assert(board);
    assert(board->variant == bslib::ChessVariant::standard);

    auto threadId = std::this_thread::get_id();
    ThreadRecord* t;
    {
        std::lock_guard<std::mutex> dolock(threadMapMutex);
        t = &threadMap[threadId];
    }
    t->init(mDb);

    if (itemMap.size() < 3) {
        return false;
    }
    
    std::unordered_map<std::string, std::string> stringMap;
    std::unordered_map<std::string, int> intMap;

    auto whiteElo = 0, blackElo = 0;
    std::string whiteName, blackName, eventName, siteName, fenString;

    auto plyCount = board->getHistListSize();

    if (paraRecord.limitLen > plyCount) {
        return false;
    }
    if (plyCount > 0) {
        intMap["PlyCount"] = plyCount;
    }


    for(auto && it : itemMap) {
        auto s = it.second;

        auto it2 = create_tagMap.find(it.first);
        if (it2 != create_tagMap.end()) {
//            while(*s <= ' ' && *s > 0) s++; // trim left
//            assert(strlen(s) < 1024);

            switch (it2->second) {
                case TagIdx_Event:
                {
                    eventName = s;
                    break;
                }
                case TagIdx_Site:
                {
                    if (paraRecord.optionFlag & create_flag_discard_sites) {
                        intMap["SiteID"] = 1; // empty
                        break;
                    }

                    // detect Lichess site, it is actually URL of the game, change to Source
                    if (memcmp(s.c_str(), lichessURL, lichessURLLength) == 0 && s.size() > lichessURLLength + 5) {
                        intMap["SiteID"] = 1; // empty for Site

                        // change content to column Source
                        if (create_tagMap.find(sourceFieldName) == create_tagMap.end()) {
                            create_addNewField(sourceFieldName);
                            stringMap[sourceFieldName] = s;
                            break;
                        }
                        auto it2 = create_tagMap.find(sourceFieldName);
                        if (it2 != create_tagMap.end()) {
//                            auto order = it2->second;
//                            assert(order > TagIdx_Max && order < fieldOrderMap.size());
                            stringMap[sourceFieldName] = s;
                        }

                        break;
                    }
                    siteName = s;
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
                {
                    fenString = s;
                    stringMap["FEN"] = s;
                    break;
                }
                case TagIdx_WhiteElo:
                {
                    whiteElo = std::stoi(s);
                    if (whiteElo > 0) {
                        intMap["WhiteElo"] = whiteElo;
                    }
                    break;
                }
                case TagIdx_BlackElo:
                {
                    blackElo = std::stoi(s);
                    if (blackElo > 0) {
                        intMap["BlackElo"] = blackElo;
                    }
                    break;
                }
                case TagIdx_PlyCount:
                {
                    plyCount = std::stoi(s);
                    if (paraRecord.limitLen > plyCount) {
                        return false;
                    }
                    break;
                }

                default:
                {
                    // ignore empty string or one started with *, ?
                    auto ch = s[0];
                    if (ch == 0 || ch == '*' || ch == '?') {
                        break;
                    }

                    if (it.first.find("Date") != std::string::npos) {
                        s = SqlLib::standardizeDate(s);
                    }
                    stringMap[it.first] = s;
                    break;
                }
            }

            continue;
        }
        
        if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
            return false;
        }
        
        if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
            return false;
        }
    }

    try {
        if (!t->insertGameStatement || t->insertGameStatementIdxSz != create_tagVec.size()) {
            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
            t->createInsertGameStatement(mDb, create_tagVec);
        }

        t->insertGameStatement->reset();
        t->insertGameStatement->clearBindings();

        intMap["EventID"] = getNameId("Events", eventName);
        intMap["SiteID"] = getNameId("Sites", siteName);

        intMap["WhiteID"] = getNameId("Players", whiteName, whiteElo);
        intMap["BlackID"] = getNameId("Players", blackName, blackElo);

        IDInteger gameID;
        {
            std::lock_guard<std::mutex> dolock(gameMutex);
            ++gameCnt;
            gameID = gameCnt;
        }
        intMap["ID"] = gameID;
        std::string moveText;

        if (paraRecord.optionFlag & create_flag_moves) {
            // trim left
//            while(*moveText <= ' ') moveText++;
            
            moveText = board->toMoveListString(bslib::Notation::san);
            stringMap["Moves"] = moveText;
        }

        // Parse moves
        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
            int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;

            if (paraRecord.optionFlag & create_flag_discard_comments) {
                flag |= bslib::BoardCore::ParseMoveListFlag_discardComment;
            }

            bslib::PgnRecord record;
            record.moveText = moveText.c_str();
            record.gameID = gameID;

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
                        std::lock_guard<std::mutex> dolock(commentMutex);
                        commentCnt++;
                    }
                }

                auto cnt = static_cast<int>(p - t->buf);
                assert(cnt >= plyCount);
                auto bindMoves = (paraRecord.optionFlag & create_flag_moves1) ? ":Moves1" : ":Moves2";
                t->insertGameStatement->bind(bindMoves, t->buf, cnt);
            }
        }
        
        // first comment
        if (!t->board->getFirstComment().empty()) {
            t->insertCommentStatement->reset();
            t->insertCommentStatement->bind(1, gameID);
            t->insertCommentStatement->bind(2, -1);
            t->insertCommentStatement->bind(3, t->board->getFirstComment());
            t->insertCommentStatement->exec();
            std::lock_guard<std::mutex> dolock(commentMutex);
            commentCnt++;
        }

        for(auto && it : stringMap) {
            t->insertGameStatement->bind(":" + it.first, it.second);
        }
        for(auto && it : intMap) {
            t->insertGameStatement->bind(":" + it.first, it.second);
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
        
        if (boardCallback) {
            boardCallback(t->board, &record);
        }
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
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
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

    t->hdpLen += t->board->getHistListSize();

    if (boardCallback) {
        boardCallback(t->board, &record);
    }
    t->gameCnt++;
}


void Builder::searchPosition(SQLite::Database* db,
                             const std::vector<std::string>& pgnPaths,
                             std::string query)
{
    mDb = db;

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

    std::cout << "Search with query " << query <<  "..." << std::endl;
    
    assert(paraRecord.task != Task::create);
    auto parser = new Parser;
    if (!parser->parse(query.c_str())) {
        std::cerr << "Error: " << parser->getErrorString() << std::endl;
        delete parser;
        return;
    }
    
    // check if there at least a move fields (Moves, Moves1 or Moves2)
    if (mDb) {
        searchField = SqlLib::getMoveField(mDb);

        if (searchField <= SearchField::none) {
            std::cerr << "FATAL ERROR: missing move field (Moves or Moves1 or Moves2)" << std::endl;
            return;
        }
        qgr = new QueryGameRecord(*mDb, searchField);
    }
    

    checkToStop = nullptr;
    
    boardCallback = [=](const bslib::BoardCore* board, const bslib::PgnRecord* record) -> bool {
        assert(board);
        
        for(int i = 1, n = board->getHistListSize(); i <= n; i++) {
            std::vector<uint64_t> bitboardVec;

            if (i < n) {
                auto hist = board->_getHistPointerAt(i);
                assert(hist && !hist->bitboardVec.empty());
                bitboardVec = hist->bitboardVec;
            } else {
                // last position
                bitboardVec = board->posToBitboards();
            }

            if (!parser->evaluate(bitboardVec)) {
                continue;
            }

            succCount++;

            if (paraRecord.optionFlag & query_flag_print_all) {
                std::lock_guard<std::mutex> dolock(printMutex);

                std::cout << succCount << ". gameId: " << (record ? record->gameID : -1) << std::endl;
            }

            if (printOut.isOn()) {
                if (paraRecord.optionFlag & query_flag_print_fen) {
                    std::string str = std::to_string(succCount) + ". gameId: " + std::to_string(record ? record->gameID : -1) +
                                ", fen: " + board->getFen() + "\n";
                    printOut.printOut(str);
                }

                static std::string printOutQuery;

                if (query != printOutQuery) {
                    printOutQuery = query;
                    printOut.printOut("; >>>>>> Query: " + query + "\n");
                }
                if (qgr) {
                    printGamePGNByIDs(*qgr, std::vector<int>{record->gameID});
                } else {
                    printOut.printOut(record->moveText);
                }
            }

            return true;
        }

        return false;
    };

    auto startTime = getNow();

    searchPosition_basic(pgnPaths);
    
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    
    std::cout << std::endl << query << " DONE. Elapsed: " << elapsed << " ms, "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", total games: " << gameCnt
              << ", total results: " << succCount
              << ", time per results: " << elapsed / std::max<int64_t>(1, succCount)  << " ms"
              << std::endl << std::endl << std::endl;
    
    mDb = nullptr;
    delete parser;
}

void Builder::searchPosition_basic(const std::vector<std::string>& pgnPaths)
{
    
    std::string moveFieldName = "Moves";
    if (searchField == SearchField::moves1) {
        moveFieldName = "Moves1";
    } else if (searchField == SearchField::moves2) {
        moveFieldName = "Moves2";
    }


    succCount = 0; gameCnt = 0;

    if (mDb) {
        
        auto str = (paraRecord.optionFlag & query_flag_print_pgn) ?
        SqlLib::fullGameQueryString : "SELECT * FROM Games";
        
        SQLite::Statement statement(*mDb, str);
        for (; statement.executeStep(); gameCnt++) {
            if (paraRecord.limitLen) {
                auto plyCount = statement.getColumn("PlyCount").getInt();

                if (plyCount < paraRecord.limitLen) {
                    continue;
                }
            }

            bslib::PgnRecord record;
            record.gameID = statement.getColumn("ID").getInt();
            record.result = statement.getColumn("Result").getText();
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
                SqlLib::extractHeader(statement, record);
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
    
    if (qgr) {
        delete qgr;
        qgr = nullptr;
    }

    // Done, retrieve some last stats
    int64_t parsedGameCnt = 0, allHdpLen = 0;
    for(auto && t : threadMap) {
        parsedGameCnt += t.second.gameCnt;
        allHdpLen += t.second.hdpLen;
    }
}

void Builder::bench(ParaRecord paraRecord)
{
    std::cout << "Benchmark position searching..." << std::endl;

    paraRecord.queries = std::vector<std::string> {
        "Q = 3",                            // three White Queens
        "r[e4, e5, d4,d5]= 2",              // two black Rooks in middle squares
        "P[d4, e5, f4, g4] = 4 and kb7",    // White Pawns in d4, e5, f4, g4 and black King in b7
        "B[c-f] + b[c-f] == 2",               // There are two Bishops (any side) from column c to f
        "white6 = 5",                        // There are 5 white pieces on row 6
    };

    searchPostion(paraRecord);
}

void Builder::searchPostion(const ParaRecord& _paraRecord)
{
    std::cout   << "Querying..." << std::endl;
    
    paraRecord = _paraRecord; assert(paraRecord.task != Task::create);

    gameCnt = commentCnt = 0;
    eventCnt = playerCnt = siteCnt = 1;
    errCnt = 0;

    // Reading all data, parsing moves, multi threads
    createPool();

    auto ok = false;
    if (paraRecord.dbPaths.empty()) {
        if (!paraRecord.pgnPaths.empty()) {
            ok = true;
            for(auto && s : paraRecord.queries) {
                searchPosition(nullptr, paraRecord.pgnPaths, s);
            }
        }
    } else {
        ok = true;
        
        for(auto && dbPath : paraRecord.dbPaths) {
            gameCnt = commentCnt = 0;
            eventCnt = playerCnt = siteCnt = 1;
            errCnt = 0;

            SQLite::Database db(dbPath, SQLite::OPEN_READONLY);

            for(auto && s : paraRecord.queries) {
                searchPosition(&db, std::vector<std::string>(), s);
            }
        }
    }

    if (!ok) {
        std::cout << "Error: there is no path for database nor PGN files" << std::endl;
    }

    std::cout << "Completed! " << std::endl;
}


/*
 This function is just an example how to query and extract data from a record with a given game ID
 */
void Builder::printGamePGNByIDs(SQLite::Database& db, const std::vector<int>& gameIDVec, SearchField searchField)
{
    QueryGameRecord qgr(db, searchField);
    printGamePGNByIDs(qgr, gameIDVec);
}


void Builder::printGamePGNByIDs(QueryGameRecord& qgr, const std::vector<int>& gameIDVec)
{
    for(auto && gameID : gameIDVec) {
        bslib::PgnRecord record;
        record.gameID = gameID;
        
        std::string str = "\n\n;ID: " + std::to_string(gameID) + "\n"
                + qgr.queryAndCreatePGNByGameID(record);
        printOut.printOut(str);
    }
}

void Builder::getGame(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;

    SQLite::Database db(paraRecord.dbPaths.front(), SQLite::OPEN_READONLY);
    auto searchField = SqlLib::getMoveField(&db);

    printGamePGNByIDs(db, paraRecord.gameIDVec, searchField);
}

void Builder::queryInfo()
{
    playerCnt = eventCnt = gameCnt = siteCnt = -1;
    
    if (!mDb) return;

    SQLite::Statement query(*mDb, "SELECT * FROM Info WHERE");
    
    while (query.executeStep()) {
        auto name = query.getColumn(0).getString();
        auto v = query.getColumn(1);
        if (name == "GameCount") {
            gameCnt = v.getInt();
        } else if (name == "PlayerCount") {
            playerCnt = v.getInt();
        } else if (name == "EventCount") {
            eventCnt = v.getInt();
        } else if (name == "SiteCount") {
            siteCnt = v.getInt();
        } else if (name == "CommentCount") {
            commentCnt = v.getInt();
        }
    }

    if (gameCnt < 0) {
        gameCnt = 0;
        SQLite::Statement query(*mDb, "SELECT COUNT(*) FROM Games");
        if (query.executeStep()) {
            gameCnt = query.getColumn(0).getInt();
        }
    }

    if (playerCnt < 0) {
        playerCnt = 0;
        SQLite::Statement query(*mDb, "SELECT COUNT(*) FROM Players");
        if (query.executeStep()) {
            playerCnt = query.getColumn(0).getInt();
        }
    }
}


void doConvertSql2Pgn(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec, int flag)
{
    assert(builder);
    builder->convertSql2PgnByAThread(record, moveVec, flag);
}

void Builder::threadConvertSql2Pgn(const bslib::PgnRecord& record,
                                   const std::vector<int8_t>& moveVec, int flag)
{
    pool->submit(doConvertSql2Pgn, record, moveVec, flag);
}

void Builder::convertSql2PgnByAThread(const bslib::PgnRecord& record,
                             const std::vector<int8_t>& moveVec, int flag)
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
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
        t->queryComments = new SQLite::Statement(*mDb, "SELECT * FROM Comments WHERE GameID = ?");
    }
    assert(t->board);
    
    t->board->newGame(record.fenText);
    t->board->fromMoveList(&record, moveVec, flag, nullptr);

    if (t->queryComments) {
        t->queryComments->reset();
        t->queryComments->bind(1, record.gameID);
        while (t->queryComments->executeStep()) {
            auto comment = t->queryComments->getColumn("Comment").getString();
            if (comment.empty()) continue;

            auto ply = t->queryComments->getColumn("Ply").getInt();
            if (ply >= 0 && ply < t->board->getHistListSize()) {
                t->board->_getHistPointerAt(ply)->comment = comment;
            } else {
                t->board->setFirstComment(comment);
            }
        }
    }

    auto toPgnString = t->board->toPgn(&record);
    if (!toPgnString.empty()) {
        std::lock_guard<std::mutex> dolock(pgnOfsMutex);
        pgnOfs << toPgnString << "\n" << std::endl;
    }

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

    SQLite::Database db(dbPath, SQLite::OPEN_READONLY);
    mDb = &db;

    assert(!paraRecord.pgnPaths.empty());
    
    pgnOfs = bslib::Funcs::openOfstream2write(pgnPath);

    bool hashMoves;
    searchField = SqlLib::getMoveField(&db, &hashMoves);
    if (hashMoves) searchField = SearchField::moves;

    SQLite::Statement queryGame(db, SqlLib::fullGameQueryString);

    auto board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    if (searchField == SearchField::moves) {
        SQLite::Statement queryComments(db, "SELECT * FROM Comments WHERE GameID = ?");
        
        for (gameCnt = 0; queryGame.executeStep(); gameCnt++) {
            bslib::PgnRecord record;
            record.gameID = queryGame.getColumn("ID").getInt();

            SqlLib::queryForABoard(record, searchField, &queryGame, &queryComments, board);
                
            auto toPgnString = board->toPgn(&record, false);
            if (!toPgnString.empty()) {
                pgnOfs << toPgnString << "\n" << std::endl;
            }

            if (gameCnt && (gameCnt & 0xffff) == 0) {
                printStats();
            }
        }

    } else {
        createPool();

        for (gameCnt = 0; queryGame.executeStep(); gameCnt++) {
            bslib::PgnRecord record;
            record.gameID = queryGame.getColumn("ID").getInt();
            SqlLib::extractHeader(queryGame, record);
            
            auto moveName = "Moves2";

            int flag = bslib::BoardCore::ParseMoveListFlag_create_san;

            if (searchField == SearchField::moves1) {
                flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
                moveName = "Moves1";
            }

            auto c = queryGame.getColumn(moveName);
            auto sz = c.size();
            auto array = c.getBlob();

            if (sz > 0 && array) {
                auto moveBlob = static_cast<const int8_t*>(array);
                std::vector<int8_t> moveVec;
                for(auto i = 0; i < sz; ++i) {
                    moveVec.push_back(moveBlob[i]);
                }
                threadConvertSql2Pgn(record, moveVec, flag);

                // reduce congestion because of having too many tasks,
                // it could improve speed a bit
                // if (pool->get_tasks_total() > 30000) {
                //    std::this_thread::sleep_for (std::chrono::seconds(1));
                // }
            } else {
                auto toPgnString = board->toPgn(&record, false);
                if (!toPgnString.empty()) {
                    std::lock_guard<std::mutex> dolock(pgnOfsMutex);
                    pgnOfs << toPgnString << "\n" << std::endl;
                }
            }

            if (gameCnt && (gameCnt & 0xffff) == 0) {
                printStats();
            }

        }
        
        printStats();
        std::cout   << "wait_for_tasks!"
                    << ", get_tasks_total: " << pool->get_tasks_total()
                    << ", get_tasks_queued: " << pool->get_tasks_queued()
                    << std::endl;
        pool->wait_for_tasks();
        
        for(auto && t : threadMap) {
            t.second.deleteAllStatements();
            t.second.delCnt;
        }

    }

    pgnOfs.close();
    mDb = nullptr;
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


void Builder::checkDuplicates(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
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
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
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

    // Check again game length
    auto plyCount = t->board->getHistListSize();
    if (plyCount < paraRecord.limitLen) {
        return;
    }

    auto embeded = paraRecord.optionFlag & dup_flag_embededgames;

    std::vector<int> gameIDVec;
    auto hashKey = t->board->getHashKeyForCheckingDuplicates();

    {
        std::lock_guard<std::mutex> dolock(dupHashKeyMutex);

        // last position
        auto it = hashGameIDMap.find(hashKey);
        if (it == hashGameIDMap.end()) {
            hashGameIDMap[hashKey] = std::vector<int>{ record.gameID };
            if (!embeded) {
                return;
            }
        } else {
            gameIDVec = it->second;
            // Have to add the gameID here thus other threads can check it too
            it->second.push_back(record.gameID);
        }

        if (embeded) {
            for(int i = std::max(paraRecord.limitLen, 1); i < plyCount; ++i) {
                auto hk = t->board->getHashKeyForCheckingDuplicates(i);
                assert(hk != hashKey);
                auto it = hashGameIDMap.find(hk);
                if (it != hashGameIDMap.end()) {
                    gameIDVec.insert(gameIDVec.end(), it->second.begin(), it->second.end());
                }
            }

            if (gameIDVec.empty()) {
                return;
            }
        }
    }

    assert(gameIDVec.size() > 0);

    auto moveName = SqlLib::searchFieldNames[static_cast<int>(searchField)];

    // Prepare for next check
    if (!t->getGameStatement) {
        std::string sqlString = "SELECT FEN, " + moveName + " FROM Games WHERE ID = ?";
        t->getGameStatement = new SQLite::Statement(*mDb, sqlString);
    }

    if (!t->board2) {
        t->board2 = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    }

    std::set<int> deletingSet;
    
    // Next check, match all moves of all games in the list
    for(auto && dupID : gameIDVec) {
        if (dupID == record.gameID) {
            continue;
        }
        t->getGameStatement->reset();
        t->getGameStatement->bind(1, dupID);

        // that game may be deleted by other threads
        if (!t->getGameStatement->executeStep()) {
            continue;
        }
        {
            bslib::PgnRecord record2;
            record2.fenText = t->getGameStatement->getColumn("FEN").getText();

            if (record2.fenText != record.fenText) {
                continue;
            }

            record2.gameID = dupID;
            t->board2->newGame(record2.fenText);

            if (searchField == SearchField::moves) {
                record2.moveString = t->getGameStatement->getColumn("Moves").getText();
                if (record2.moveString.empty()) {
                    continue;
                }

                t->board2->fromMoveList(&record2, bslib::Notation::san, flag, nullptr);
            } else {
                std::vector<int8_t> moveVec;

                auto c = t->getGameStatement->getColumn(moveName.c_str());
                auto moveBlob = static_cast<const int8_t*>(c.getBlob());

                if (moveBlob) {
                    auto sz = c.size();
                    for(auto i = 0; i < sz; ++i) {
                        moveVec.push_back(moveBlob[i]);
                    }
                }

                t->board2->fromMoveList(&record2, moveVec, flag, nullptr);
            }
        }

        auto plyCount2 = t->board2->getHistListSize();
        if (plyCount2 < paraRecord.limitLen || (!embeded && plyCount != plyCount2)) {
            continue;
        }

        if (t->board->equalMoveLists(t->board2, embeded)) {
            if (paraRecord.optionFlag & dup_flag_remove) {
                auto theID = plyCount2 <= plyCount ? dupID : record.gameID;
                deletingSet.insert(theID);
            }

            t->dupCnt++;
            printDuplicates(t, record, dupID, hashKey);
        }
    }
    
    if (deletingSet.empty()) {
        return;
    }
    
    if (!t->removeGameStatement) {
        assert(mDb);
        t->removeGameStatement = new SQLite::Statement(*mDb, "DELETE FROM Games WHERE ID = ?");
    }

    for(auto && removingGameID : deletingSet) {
        {
            std::lock_guard<std::mutex> dolock(dupHashKeyMutex);
    
            auto it = hashGameIDMap.find(hashKey);
            assert(it != hashGameIDMap.end());
            for(size_t i = 0; i < it->second.size(); ++i) {
                if (it->second.at(i) == removingGameID) {
                    it->second.erase(it->second.begin() + i);
                    break;
                }
            }
        }
    
        t->removeGameStatement->reset();
        t->removeGameStatement->bind(1, removingGameID);
    
        try {
            if (t->removeGameStatement->exec()) {
                t->delCnt++;
                
                if (printOut.isOn()) {
                    std::string str = ";>>>>> Deleted ID: " + std::to_string(removingGameID) + "\n\n";
                    printOut.printOut(str);
                }
            }
        } catch (std::exception& e) {
            std::cout << "SQLite exception: " << e.what() << std::endl;
            t->errCnt++;
        }
    }
}

void Builder::printDuplicates(ThreadRecord* t, const bslib::PgnRecord& record, int theDupID, uint64_t hashKey)
{
    if (theDupID < 0) {
        return;
    }

    auto plyCount = t->board->getHistListSize();
    
    if (paraRecord.optionFlag & query_flag_print_all) {
        std::lock_guard<std::mutex> dolock(printMutex);

        std::cerr << "Duplicate games detected between IDs " << theDupID << " and " << record.gameID
        << ", game length: " << plyCount
        << std::endl;
    }

    if (printOut.isOn()) {
        if (!t->qgr) {
            t->qgr = new QueryGameRecord(*mDb, searchField);
        }

        bslib::PgnRecord record2;
        record2.gameID = theDupID;
        auto toPgnString0 = t->qgr->queryAndCreatePGNByGameID(record2);
        record2.gameID = record.gameID;
        auto toPgnString1 = t->qgr->queryAndCreatePGNByGameID(record2);

        {
            std::string str = ";>>>>> Duplicate: " + std::to_string(theDupID) + " vs " + std::to_string(record.gameID)
                + "\n\n;ID: " + std::to_string(theDupID) + "\n" + toPgnString0
                + "\n\n;ID: " + std::to_string(record.gameID) + "\n" + toPgnString1
                + "\n\n";

            printOut.printOut(str);
        }
    }
}

void Builder::findDuplicatedGames(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;
    createPool();
    
    for(auto && dbPath : paraRecord.dbPaths) {
        std::cout   << "Finding duplicate games...\n"
                    << "DB path : " << dbPath
                    << std::endl;

        startTime = getNow();

        mDb = new SQLite::Database(dbPath, (paraRecord.optionFlag & dup_flag_remove) ? SQLite::OPEN_READWRITE : SQLite::OPEN_READONLY);
        if (!mDb) {
            std::cerr << "Error: can't open database " << dbPath << std::endl;
            continue;
        }

        searchField = SqlLib::getMoveField(mDb);
        if (searchField == SearchField::none) {
            std::cerr << "Error: database " << dbPath << " has not any move field" << std::endl;
            continue;
        }

        hashGameIDMap.clear();

        // query for GameCount for setting reserve
        if (paraRecord.optionFlag & query_flag_print_all) {
            SQLite::Statement statement(*mDb, "SELECT * FROM Info WHERE Name = 'GameCount'");
            
            if (statement.executeStep()) {
                auto s = statement.getColumn("Value").getText();
                auto gameCount = std::stoi(s);
                
                // Just a large number to avoid rubish data
                if (gameCount > 0 && gameCount < 1024 * 1024 * 1024) {
                    hashGameIDMap.reserve(gameCount + 16);
                }
            }
        }

        if (paraRecord.optionFlag & dup_flag_remove) {
            mDb->exec("PRAGMA journal_mode=OFF");
            mDb->exec("BEGIN");
        }
        
        for(auto && t : threadMap) {
            t.second.resetStats();
        }

        std::string moveName = SqlLib::searchFieldNames[static_cast<int>(searchField)];
        assert(!moveName.empty());
        
        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check | bslib::BoardCore::ParseMoveListFlag_discardComment;
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }

        std::string sqlString = "SELECT ID, FEN, PlyCount, " + moveName + " FROM Games";

        // sort games by lengths thus the longer games can check back sorter ones for embeded games
        if (paraRecord.optionFlag & dup_flag_embededgames) {
            sqlString += " ORDER BY PlyCount ASC";
        }

        auto statement = new SQLite::Statement(*mDb, sqlString);
        
        for (gameCnt = 0; statement->executeStep(); ++gameCnt) {
            auto c = statement->getColumn("PlyCount");
            if (!c.isNull()) {
                auto plyCount = c.getInt();
                if (plyCount < paraRecord.limitLen) {
                    continue;
                }
            }

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
        
        int64_t delCnt = 0;
        for(auto && t : threadMap) {
            t.second.deleteAllStatements();
            delCnt += t.second.delCnt;
        }

        {
            
            // Update table Info
            if (delCnt > 0) {
                int64_t gCnt = gameCnt - delCnt;
                
                std::string sqlstr = "UPDATE Info SET Value = '" + std::to_string(gCnt) + "' WHERE Name = 'GameCount'";
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
