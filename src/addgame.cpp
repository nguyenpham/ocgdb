/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "addgame.h"
#include "board/chess.h"

using namespace ocgdb;

AddGame::AddGame()
{
}

void AddGame::runTask()
{
    createMode = false;

    assert(!paraRecord.dbPaths.empty());
    auto dbPath = paraRecord.dbPaths.front();

    if (!openDb(dbPath)) {
        return;
    }
    

    // add games from PGN files
    processPgnFiles(paraRecord.pgnPaths);
    
    // add games from SQLite databases
    dbRead.addGameInstance = this;
    dbRead.setOptionFlag(paraRecord.optionFlag | query_flag_print_pgn); // for calling extractHeader
    for(size_t i = 1; i < paraRecord.dbPaths.size(); i++) {
        addDb(paraRecord.dbPaths.at(i));
    }

    updateInfoTable();
}

bool AddGame::openDb(const std::string& dbPath)
{
    mDb = new SQLite::Database(dbPath, SQLite::OPEN_READWRITE);
    if (!mDb) {
        createMode = true;
        Builder::runTask();
        return false;
    }


    {
        searchField = SearchField::none;
        paraRecord.optionFlag &= ~(create_flag_moves | create_flag_moves1 | create_flag_moves2);

        const std::set<std::string> idSet {
            "EventID", "SiteID", "WhiteID", "BlackID"
        };

        std::vector<std::string> tagVec;

        SQLite::Statement stmt(*mDb, "PRAGMA table_info(Games)");
        while (stmt.executeStep()) {
            std::string fieldName = stmt.getColumn(1).getText();
            
            if (fieldName == "Moves2") {
                searchField = SearchField::moves2;
                paraRecord.optionFlag |= create_flag_moves2;
                continue;
            }

            if (fieldName == "Moves1") {
                if (searchField < SearchField::moves1) {
                    searchField = SearchField::moves1;
                    paraRecord.optionFlag |= create_flag_moves1;
                }
                continue;
            }

            if (fieldName == "Moves") {
                if (searchField < SearchField::moves) {
                    searchField = SearchField::moves;
                    paraRecord.optionFlag |= create_flag_moves;
                }
                continue;
            }
            
            if (idSet.find(fieldName) != idSet.end()) {
                fieldName = fieldName.substr(0, fieldName.size() - 2);
            }
            
            tagVec.push_back(fieldName);
        }

        setupTagVec(tagVec, paraRecord.optionFlag);
    }
    {
        SQLite::Statement stmtGID(*mDb, "SELECT max(ID) FROM Games");
        if (stmtGID.executeStep()) {
            newGameID = stmtGID.getColumn(0).getInt();
        }
        
        queryInfo();
    }

    return true;
}

IDInteger AddGame::getNewGameID()
{
    if (createMode) {
        return Builder::getNewGameID();
    }

    std::lock_guard<std::mutex> dolock(gameMutex);
    ++newGameID;
    ++gameCnt;
    return newGameID;
}


// Query directly from the database, if not existent, add it
IDInteger AddGame::getNameId(const std::string& tableName, IDInteger& cnt, const std::string& name, int elo)
{
    auto theName = Builder::encodeString(name);
    std::string sQuery = "SELECT ID FROM " + tableName + " WHERE name=\"" + theName + "\"";
    SQLite::Statement query(*mDb, sQuery.c_str());
    if (query.executeStep()) {
        return query.getColumn(0);
    }

    ++cnt;
    std::string str0 = "INSERT INTO " + tableName + "(name";
    std::string str1 = ") VALUES (\"" + theName + "\"";
    std::string str2 = ") RETURNING ID";

    if (elo > 0) {
        str0 += ", Elo";
        str1 += ", " + std::to_string(elo);
    }
    return mDb->execAndGet(str0 + str1 + str2);
}

int AddGame::getPlayerNameId(char* name, int elo)
{
    if (createMode) {
        return Builder::getPlayerNameId(name, elo);
    }
    std::lock_guard<std::mutex> dolock(playerMutex);
    return getNameId("Players", playerCnt, name, elo);
}

int AddGame::getEventNameId(char* name)
{
    if (createMode) {
        return Builder::getEventNameId(name);
    }
    std::lock_guard<std::mutex> dolock(eventMutex);
    return getNameId("Events", eventCnt, name);
}



int AddGame::getSiteNameId(char* name)
{
    if (createMode) {
        return Builder::getSiteNameId(name);
    }
    std::lock_guard<std::mutex> dolock(siteMutex);
    return getNameId("Sites", siteCnt, name);
}


/////////////////////////////////////////////////////////////
// add games from SQLite databases
void AddGame::addDb(const std::string& dbPath)
{
    try
    {
        dbRead.readADb(dbPath, "SELECT * FROM Games");
    }
    catch (std::exception& e)
    {
        std::cout << "SQLite exception: " << e.what() << ", path: " << dbPath << std::endl;
    }
}

bool AddGame::createConvertingIDMaps(SQLite::Database* db)
{
    return
        createConvertingIDMap(mDb, "Players", gameCnt, playerConvertIDMap) &&
        createConvertingIDMap(mDb, "Events", eventCnt, eventConvertIDMap) &&
        createConvertingIDMap(mDb, "Sites", siteCnt, siteConvertIDMap);
}

bool AddGame::createConvertingIDMap(SQLite::Database* db, const std::string& tableName, IDInteger& cnt, std::map<IDInteger, IDInteger>& theMap)
{
    assert(db);
    theMap.clear();
    
    std::string sQuery = "SELECT * FROM " + tableName;

    SQLite::Statement query(*db, sQuery.c_str());
    while (query.executeStep()) {
        auto theID = query.getColumn("ID").getInt();
        auto name = query.getColumn("Name").getString();
        auto elo = -1;
        if (tableName == "Players") {
            elo = query.getColumn("Elo").getInt();
        }
                
        auto newID = getNameId(tableName, cnt, name, elo);
        theMap[theID] = newID;
    }

    return true;
}

ThreadRecord* AddGame::getThreadRecordAndInit()
{
    auto t = getThreadRecord(); assert(t);

    if (!t->board) {
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    }
    assert(t->board);
    
    if (!t->insertGameStatement) {
        std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
        t->createInsertGameStatement(mDb, create_tagVec);
    }
    
    return t;
}

void AddGame::addAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(!record.moveString.empty() || record.moveText || !moveVec.empty());
    assert(record.gameID > 0);

    auto t = getThreadRecordAndInit();
    assert(t && t->insertGameStatement);

    std::unordered_map<std::string, const char*> stringMap;
    std::unordered_map<std::string, int> intMap;
    std::string ecoString;
    int plyCount = 0;

    IDInteger gameID = getNewGameID();
    intMap["ID"] = gameID;
    
    if ((paraRecord.optionFlag & create_flag_moves) && !record.moveString.empty()) {
        stringMap["Moves"] = record.moveString.c_str();
    }

    {
        for(auto && it : record.tags) {
            if (create_tagMap.find(it.first) != create_tagMap.end()) {
                stringMap[it.first] = it.second.c_str();
            }
        }
        {
            auto it = eventConvertIDMap.find(record.eventID);
            if (it != eventConvertIDMap.end()) {
                intMap["EventID"] = it->second;
            }
        }
        if (create_tagMap.find("Site") != create_tagMap.end()) {
            auto it = siteConvertIDMap.find(record.siteID);
            if (it != siteConvertIDMap.end()) {
                intMap["SiteID"] = it->second;
            }
        }
        if (create_tagMap.find("White") != create_tagMap.end()) {
            auto it = playerConvertIDMap.find(record.whiteID);
            if (it != playerConvertIDMap.end()) {
                intMap["WhiteID"] = it->second;
            }
        }
        if (create_tagMap.find("Black") != create_tagMap.end()) {
            auto it = playerConvertIDMap.find(record.blackID);
            if (it != playerConvertIDMap.end()) {
                intMap["BlackID"] = it->second;
            }
        }
    }
    
    t->board->newGame(record.fenText);

    // Parse moves
    if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {

        int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;
        
        if (paraRecord.optionFlag & create_flag_discard_comments) {
            flag |= bslib::BoardCore::ParseMoveListFlag_discardComment;
        }

//        bslib::PgnRecord record;
//        record.moveText = moveText;
//        record.gameID = gameID;
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

    t->insertGameStatement->reset();
    t->insertGameStatement->clearBindings();

    for(auto && it : stringMap) {
        t->insertGameStatement->bind(":" + it.first, it.second);
    }
    for(auto && it : intMap) {
        t->insertGameStatement->bind(":" + it.first, it.second);
    }
    
    t->insertGameStatement->exec();

}

/////////////////////////////////////////////////////////////
void AddGameDbRead::setOptionFlag(int optionFlag)
{
    paraRecord.optionFlag = optionFlag;
}


bool AddGameDbRead::openDB(const std::string& dbPath)
{
    return DbRead::openDB(dbPath) && addGameInstance->createConvertingIDMaps(mDb);
}


void AddGameDbRead::processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    addGameInstance->addAGame(record, moveVec);
}
