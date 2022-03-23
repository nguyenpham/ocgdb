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

void AddGame::runTask()
{
}


// Query directly from databases
IDInteger AddGame::getNameId(const std::string& tableName, const std::string& name, int elo)
{
    auto theName = Builder::encodeString(name);
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

bool AddGame::addGame(const std::string& dbPath, const std::string& pgnString)
{
    if (dbPath.empty() || pgnString.empty()) return false;
    
    std::unordered_map<std::string, std::string> itemMap;
    bslib::ChessBoard board;
//    board.BoardCore::moveFromString_san(<#const std::string &#>);
    
    return addGame(dbPath, itemMap, &board);
}

bool AddGame::addGame(const std::string& dbPath, const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board)
{
    return false;
//    mDb = new SQLite::Database(dbPath, SQLite::OPEN_READWRITE);
//
//    if (mDb) {
//    } else {
//        paraRecord.optionFlag = create_flag_moves2;
//        mDb = createDb(dbPath, paraRecord.optionFlag, create_tagVec);
//        createInsertStatements(*mDb);
//    }
//    if (!mDb) {
//        std::cerr << "Error: Can't open nor create database file '" << mDb->getFilename() << std::endl;
//        return false;
//    }
//    std::cout << "SQLite database file '" << mDb->getFilename() << "' opened successfully" << std::endl;;
//    
//    bool hashMoves;
//    searchField = getMoveField(mDb, &hashMoves);
//
//    auto r = addGame(itemMap, board);
//    
//    return r;
}

bool AddGame::addGame(const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board)
{
//    assert(board);
//    assert(board->variant == bslib::ChessVariant::standard);
//
//    auto threadId = std::this_thread::get_id();
//    ThreadRecord* t;
//    {
//        std::lock_guard<std::mutex> dolock(threadMapMutex);
//        t = &threadMap[threadId];
//    }
//    t->init(mDb);
//
//    if (itemMap.size() < 3) {
//        return false;
//    }
//
//    std::unordered_map<std::string, std::string> stringMap;
//    std::unordered_map<std::string, int> intMap;
//
//    auto whiteElo = 0, blackElo = 0;
//    std::string whiteName, blackName, eventName, siteName, fenString;
//
//    auto plyCount = board->getHistListSize();
//
//    if (paraRecord.limitLen > plyCount) {
//        return false;
//    }
//    if (plyCount > 0) {
//        intMap["PlyCount"] = plyCount;
//    }
//
//
//    for(auto && it : itemMap) {
//        auto s = it.second;
//
//        auto it2 = create_tagMap.find(it.first);
//        if (it2 != create_tagMap.end()) {
//
//            switch (it2->second) {
//                case TagIdx_Event:
//                {
//                    eventName = s;
//                    break;
//                }
//                case TagIdx_Site:
//                {
//                    if (paraRecord.optionFlag & create_flag_discard_sites) {
//                        intMap["SiteID"] = 1; // empty
//                        break;
//                    }
//
//                    // detect Lichess site, it is actually URL of the game, change to Source
//                    if (memcmp(s.c_str(), lichessURL, lichessURLLength) == 0 && s.size() > lichessURLLength + 5) {
//                        intMap["SiteID"] = 1; // empty for Site
//
//                        // change content to column Source
//                        if (create_tagMap.find(sourceFieldName) == create_tagMap.end()) {
//                            create_addNewField(sourceFieldName);
//                            stringMap[sourceFieldName] = s;
//                            break;
//                        }
//                        auto it2 = create_tagMap.find(sourceFieldName);
//                        if (it2 != create_tagMap.end()) {
////                            auto order = it2->second;
////                            assert(order > TagIdx_Max && order < fieldOrderMap.size());
//                            stringMap[sourceFieldName] = s;
//                        }
//
//                        break;
//                    }
//                    siteName = s;
//                    break;
//                }
//                case TagIdx_White:
//                {
//                    whiteName = s;
//                    break;
//                }
//                case TagIdx_Black:
//                {
//                    blackName = s;
//                    break;
//                }
//
//                case TagIdx_FEN:
//                {
//                    fenString = s;
//                    stringMap["FEN"] = s;
//                    break;
//                }
//                case TagIdx_WhiteElo:
//                {
//                    whiteElo = std::stoi(s);
//                    if (whiteElo > 0) {
//                        intMap["WhiteElo"] = whiteElo;
//                    }
//                    break;
//                }
//                case TagIdx_BlackElo:
//                {
//                    blackElo = std::stoi(s);
//                    if (blackElo > 0) {
//                        intMap["BlackElo"] = blackElo;
//                    }
//                    break;
//                }
//                case TagIdx_PlyCount:
//                {
//                    plyCount = std::stoi(s);
//                    if (paraRecord.limitLen > plyCount) {
//                        return false;
//                    }
//                    break;
//                }
//
//                default:
//                {
//                    // ignore empty string or one started with *, ?
//                    auto ch = s[0];
//                    if (ch == 0 || ch == '*' || ch == '?') {
//                        break;
//                    }
//
//                    if (it.first.find("Date") != std::string::npos) {
//                        s = SqlLib::standardizeDate(s);
//                    }
//                    stringMap[it.first] = s;
//                    break;
//                }
//            }
//
//            continue;
//        }
//
//        if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
//            return false;
//        }
//
//        if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
//            return false;
//        }
//    }
//
//    try {
//        if (!t->insertGameStatement || t->insertGameStatementIdxSz != create_tagVec.size()) {
//            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
//            t->createInsertGameStatement(mDb, create_tagVec);
//        }
//
//        t->insertGameStatement->reset();
//        t->insertGameStatement->clearBindings();
//
//        intMap["EventID"] = getNameId("Events", eventName);
//        intMap["SiteID"] = getNameId("Sites", siteName);
//
//        intMap["WhiteID"] = getNameId("Players", whiteName, whiteElo);
//        intMap["BlackID"] = getNameId("Players", blackName, blackElo);
//
//        IDInteger gameID;
//        {
//            std::lock_guard<std::mutex> dolock(gameMutex);
//            ++gameCnt;
//            gameID = gameCnt;
//        }
//        intMap["ID"] = gameID;
//        std::string moveText;
//
//        if (paraRecord.optionFlag & create_flag_moves) {
//            // trim left
////            while(*moveText <= ' ') moveText++;
//
//            moveText = board->toMoveListString(bslib::Notation::san);
//            stringMap["Moves"] = moveText;
//        }
//
//        // Parse moves
//        if (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) {
//            int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;
//
//            if (paraRecord.optionFlag & create_flag_discard_comments) {
//                flag |= bslib::BoardCore::ParseMoveListFlag_discardComment;
//            }
//
//            bslib::PgnRecord record;
//            record.moveText = moveText.c_str();
//            record.gameID = gameID;
//
//            if (plyCount > 0) {
//                auto p = t->buf;
//                for(auto i = 0; i < plyCount; i++) {
//                    auto h = t->board->_getHistPointerAt(i);
//                    auto move = h->move;
//
//                    if (paraRecord.optionFlag & create_flag_moves2) { // 2 bytes encoding
//                        *(int16_t*)p = bslib::ChessBoard::encode2Bytes(move);
//                        p += 2;
//                    } else if (paraRecord.optionFlag & create_flag_moves1) {
//                        auto pair = bslib::ChessBoard::encode1Byte(move);
//                        assert(pair.second == 1 || pair.second == 2);
//                        if (pair.second == 1) {
//                            *p = static_cast<int8_t>(pair.first);
//                            p++;
//                        } else {
//                            *(int16_t*)p = pair.first;
//                            assert(*p == static_cast<int8_t>(pair.first));
//                            assert(*(p + 1) == static_cast<int8_t>(pair.first >> 8));
//                            p += 2;
//                        }
//                    }
//
//                    if (!h->comment.empty()) {
//                        t->insertCommentStatement->reset();
//                        t->insertCommentStatement->bind(1, gameID);
//                        t->insertCommentStatement->bind(2, i);
//                        t->insertCommentStatement->bind(3, h->comment);
//                        t->insertCommentStatement->exec();
//                        std::lock_guard<std::mutex> dolock(commentMutex);
//                        commentCnt++;
//                    }
//                }
//
//                auto cnt = static_cast<int>(p - t->buf);
//                assert(cnt >= plyCount);
//                auto bindMoves = (paraRecord.optionFlag & create_flag_moves1) ? ":Moves1" : ":Moves2";
//                t->insertGameStatement->bind(bindMoves, t->buf, cnt);
//            }
//        }
//
//        // first comment
//        if (!t->board->getFirstComment().empty()) {
//            t->insertCommentStatement->reset();
//            t->insertCommentStatement->bind(1, gameID);
//            t->insertCommentStatement->bind(2, -1);
//            t->insertCommentStatement->bind(3, t->board->getFirstComment());
//            t->insertCommentStatement->exec();
//            std::lock_guard<std::mutex> dolock(commentMutex);
//            commentCnt++;
//        }
//
//        for(auto && it : stringMap) {
//            t->insertGameStatement->bind(":" + it.first, it.second);
//        }
//        for(auto && it : intMap) {
//            t->insertGameStatement->bind(":" + it.first, it.second);
//        }
//
//        t->insertGameStatement->exec();
//    }
//    catch (std::exception& e)
//    {
//        std::cout << "SQLite exception: " << e.what() << std::endl;
//        t->errCnt++;
//        return false;
//    }

    return true;
}
