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


using namespace ocgdb;


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


void Builder::standardizeDate(char* date)
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


std::string Builder::standardizeDate(const std::string& date)
{
    std::string s;
    auto c = 0;
    for(char p : date) {
        if (p == '.') {
            p = '-';
            c++;
        } else if (p == '?') {
            p = '1';
        }
        
        s += p;
    }
    
    if (c != 2) {
        s.clear();
    }
    
    return s;
}


std::string Builder::encodeString(const std::string& str)
{
    return bslib::Funcs::replaceString(str, "\"", "\\\"");
}


void Builder::runTask()
{
    std::cout   << "Convert PGN files into a database..." << std::endl;

    startTime = getNow();

    // Prepare
    assert(!paraRecord.dbPaths.empty());
    auto dbPath = paraRecord.dbPaths.front();
    
    // remove old db file if existed
    std::remove(dbPath.c_str());
    
    create();
}

void Builder::create()
{
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
        playerIdMap.reserve(8 * 1024 * 1024);
        eventIdMap.reserve(128 * 1024);
        siteIdMap.reserve(128 * 1024);

        auto dbPath = paraRecord.dbPaths.front();
        if (!createDb(dbPath) || !mDb) {
            return;
        }
        createInsertStatements(*mDb);
    }

    processPgnFiles(paraRecord.pgnPaths);

    // completing
    {
        updateInfoTable();
        
        if (playerInsertStatement) delete playerInsertStatement;
        playerInsertStatement = nullptr;
        if (eventInsertStatement) delete eventInsertStatement;
        eventInsertStatement = nullptr;
        if (siteInsertStatement) delete siteInsertStatement;
        siteInsertStatement = nullptr;
    }
}

void Builder::updateInfoTable()
{
    std::string str = "DELETE FROM Info WHERE Name='GameCount' OR Name='PlayerCount' OR Name='EventCount' OR Name='SiteCount' OR Name='CommentCount'";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('GameCount', '") + std::to_string(gameCnt) + "')";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('PlayerCount', '") + std::to_string(playerCnt) + "')";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('EventCount', '") + std::to_string(eventCnt) + "')";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('SiteCount', '") + std::to_string(siteCnt) + "')";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('CommentCount', '") + std::to_string(commentCnt) + "')";
    mDb->exec(str);
}

void Builder::printStats() const
{
    DbCore::printStats();
    std::cout << std::endl;
}


bool Builder::addNewField(const std::string& fieldName)
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
    }
    return true;
}

bool Builder::createDb(const std::string& dbPath)
{
    gameCnt = commentCnt = 0;
    eventCnt = playerCnt = siteCnt = 1;
    errCnt = 0;
    
    // ID, FEN, Moves, Moves1, Moves2 are special columns
    setupTagVec({
        "ID",
        "Event", "Site", "Date", "Round",
        "White", "WhiteElo", "Black", "BlackElo",
        "Result", "TimeControl", "ECO", "PlyCount"
    }, paraRecord.optionFlag);
        
    // Create database
    mDb = createDb(dbPath, paraRecord.optionFlag, create_tagVec);
    return mDb != nullptr;
}

void Builder::setupTagVec(const std::vector<std::string>& tagVec, int optionFlag)
{
    create_tagVec = tagVec;
    
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
    
    if (optionFlag & create_flag_moves) {
        create_tagVec.push_back("Moves");
    }
    if (optionFlag & (create_flag_moves1 | create_flag_moves2)) {
        create_tagVec.push_back((optionFlag & create_flag_moves2) ? "Moves2" : "Moves1");
    }
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


int Builder::getSiteNameId(char* name)
{
    std::lock_guard<std::mutex> dolock(siteMutex);
    return getNameId(name, -1, siteCnt, siteInsertStatement, siteIdMap);
}



static const std::string sourceFieldName = "Source";
static const char* lichessURL = "https://lichess.org/";
static const int lichessURLLength = 20;


// Add games when creating database
void Builder::processPGNGameWithAThread(ThreadRecord* t, const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(t);

    t->init(mDb);
    assert(t->board);

    if (itemMap.size() < 3) {
        t->errCnt++;
        return;
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
                return;
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
                    intMap["EventID"] = getEventNameId(s);
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
                            if (addNewField(sourceFieldName)) {
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
                    intMap["SiteID"] = getSiteNameId(s);
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
                        return;
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
                        return;
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
                        standardizeDate(s);
                    }

                    stringMap[it.first] = s;
                    break;
                }
            } // switch

            continue;
        } // if (it2 != create_fieldMap.end())
        
        
        if ((paraRecord.optionFlag & create_flag_accept_new_tags) &&
            addNewField(it.first)) {
            if (t->insertGameStatement) {
                delete t->insertGameStatement;
                t->insertGameStatement = nullptr;
            }
            stringMap[it.first] = s;
        }
    }

    if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
        return;
    }
    
    if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
        return;
    }
    
    try {
        if (!t->insertGameStatement || t->insertGameStatementIdxSz != create_tagVec.size()) {
            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
            t->createInsertGameStatement(mDb, create_tagVec);
        }

        t->insertGameStatement->reset();
        t->insertGameStatement->clearBindings();

        intMap["WhiteID"] = getPlayerNameId(whiteName, whiteElo);
        intMap["BlackID"] = getPlayerNameId(blackName, blackElo);

        IDInteger gameID = getNewGameID();
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
                return;
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
        return;
    }
}


IDInteger Builder::getNewGameID()
{
    std::lock_guard<std::mutex> dolock(gameMutex);
    ++gameCnt;
    return gameCnt;
}
