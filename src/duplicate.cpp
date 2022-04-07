/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "duplicate.h"

using namespace ocgdb;

void Duplicate::runTask()
{
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

        searchField = DbRead::getMoveField(mDb);
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

        std::string moveName = searchFieldNames[static_cast<int>(searchField)];
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
        
        readADb(dbPath, sqlString);
    }
}

void Duplicate::printStats() const
{
    int64_t dupCnt = 0, delCnt = 0;
    for(auto && t : threadMap) {
        dupCnt += t.second.dupCnt;
        delCnt += t.second.delCnt;
    }

    DbCore::printStats();
    std::cout << ", #duplicates: " << dupCnt << ", #removed: " << delCnt;
    std::cout << std::endl;
}


void Duplicate::processAGameWithAThread(ThreadRecord* t, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(t);

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
    auto moveName = searchFieldNames[static_cast<int>(searchField)];

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

void Duplicate::printDuplicates(ThreadRecord* t, const bslib::PgnRecord& record, int theDupID, uint64_t hashKey)
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
