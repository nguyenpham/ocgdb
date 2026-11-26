/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>
#include <functional>
#include <map>
#include <set>

#include "duplicate.h"
#include "report.h"

using namespace ocgdb;

bool Duplicate::runTask()
{
    auto ok = true;
    for (auto&& dbPath : paraRecord.dbPaths) {
        if (!working) break;
        ok = checkADatabase(dbPath) && ok;
    }
    return ok;
}

void Duplicate::printStats(bool completed) const
{
    auto elapsed = elapsedMs();
    std::lock_guard<std::mutex> dolock(printMutex);
    std::cout << "#games checked: " << processedCnt << ", elapsed: " << elapsed << " ms, speed: "
              << processedCnt * 1000LL / elapsed << " games/s";
    if (completed) {
        std::cout << ", duplicates: " << dupPairCnt << ", removed: " << delCnt;
    }
    std::cout << std::endl;
}

bool Duplicate::checkADatabase(const std::string& dbPath)
{
    std::cout << "Finding duplicate games: " << dbPath << std::endl;

    startTime = getNow();
    processedCnt = 0;
    delCnt = dupPairCnt = 0;
    samePairVec.clear();
    embededPairVec.clear();
    hashGameIDMap.clear();

    if (!openOCGDB(dbPath, (paraRecord.optionFlag & dup_flag_remove) == 0)) {
        return false;
    }

    if (searchField == SearchField::none) {
        std::cerr << "Error: database " << dbPath << " has no column for moves" << std::endl;
        return false;
    }

    moveName = searchFieldNames[static_cast<int>(searchField)];

    auto sqlString = "SELECT g.ID, g.FEN, g." + moveName + " FROM Games g";
    std::string cond;
    if (!paraRecord.where.empty()) {
        /// the condition may name the players, the event and the site (w.Name, e.Name...)
        sqlString = fullGameQueryString;
        cond = "(" + paraRecord.where + ")";
    }
    if (paraRecord.limitLen > 0 && gameFieldSet.count("PlyCount")) {
        if (!cond.empty()) cond += " AND ";
        cond += "g.PlyCount >= " + std::to_string(paraRecord.limitLen);
    }
    if (!cond.empty()) {
        sqlString += " WHERE " + cond;
    }

    // sort games by lengths thus the longer games can check back the shorter ones for embedded games
    if ((paraRecord.optionFlag & dup_flag_embededgames) && gameFieldSet.count("PlyCount")) {
        sqlString += " ORDER BY g.PlyCount ASC";
    }

    if (debugMode) {
        std::cout << "SQL: " << sqlString << std::endl;
    }

    mainLineOnly = true;
    try {
        readGames(sqlString);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    reportRemove();
    printStats(true);
    closeDb();
    return true;
}

void Duplicate::processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                        const std::vector<int8_t>& moveVec)
{
    assert(t);

    t->initGetGameStatement(mDb, variant, moveName);
    assert(t->board && t->board2);

    extractBoard(t->board, record, moveVec, true);
    t->gameCnt++;

    auto plyCount = t->board->getMainLineSize();
    if (plyCount < paraRecord.limitLen) {
        return;
    }

    const bool embeded = paraRecord.optionFlag & dup_flag_embededgames;

    /// Check hashKeys
    std::vector<IDInteger> gameIDVec;
    auto dupHashKey = t->board->getHashKeyForCheckingDuplicates();

    {
        std::lock_guard<std::mutex> dolock(dupHashKeyMutex);

        // last position
        auto it = hashGameIDMap.find(dupHashKey);
        if (it == hashGameIDMap.end()) {
            hashGameIDMap[dupHashKey] = std::vector<IDInteger> { record.gameID };
        } else {
            gameIDVec = it->second;
            /// added here so that other threads check against it too
            it->second.push_back(record.gameID);
        }

        if (embeded) {
            for (int i = std::max(paraRecord.limitLen, 1); i < plyCount; ++i) {
                auto hk = t->board->getHashKeyForCheckingDuplicates(i);
                auto it = hashGameIDMap.find(hk);
                if (it != hashGameIDMap.end()) {
                    gameIDVec.insert(gameIDVec.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }

    for (auto&& checkingID : gameIDVec) {
        if (checkingID == record.gameID) {
            continue;
        }

        if (t->isBoardDuplication(checkingID, embeded)) {
            t->dupCnt++;
            auto sameLength = t->board2->getMainLineSize() == plyCount;

            std::lock_guard<std::mutex> dolock(dupVecMutex);
            if (sameLength) {
                samePairVec.emplace_back(record.gameID, checkingID);
            } else {
                embededPairVec.emplace_back(record.gameID, checkingID);
            }
            break;
        }
    }
}

/// The pairs found, reported and/or removed: each duplicate once; deleted in one transaction,
/// with their comments; the Info counts recounted; Banksia GUI's opening tree (table Tree), whose
/// counts include the duplicates, dropped (as Banksia GUI's Duplicate::reportRemove).
void Duplicate::reportRemove()
{
    /// Copies of one game make a group, whichever pairs of them the threads compared: of each
    /// group the game with the smallest ID stays -- it was in the database first. (Pairs taken one
    /// by one, as (smaller, larger), could name as the copy kept a game already marked as a copy,
    /// and one of four copies stayed, depending on how the threads ran.)
    std::map<IDInteger, IDInteger> parent;
    std::function<IDInteger(IDInteger)> find = [&](IDInteger x) {
        auto it = parent.find(x);
        if (it == parent.end() || it->second == x) {
            return x;
        }
        auto r = find(it->second);
        parent[x] = r;
        return r;
    };
    for (auto&& [a, b] : samePairVec) {
        auto ra = find(a), rb = find(b);
        if (ra != rb) {
            /// the smaller ID is the root
            parent[std::max(ra, rb)] = std::min(ra, rb);
            parent.emplace(std::min(ra, rb), std::min(ra, rb));
        }
    }

    std::map<IDInteger, IDInteger> dupOf; // duplicate -> kept
    for (auto&& it : parent) {
        auto root = find(it.first);
        if (it.first != root) {
            dupOf[it.first] = root;
        }
    }
    /// a game that is the start of a longer one goes, the longer one stays
    for (auto&& [longer, shorter] : embededPairVec) {
        if (!dupOf.count(shorter)) {
            dupOf[shorter] = longer;
        }
    }
    dupPairCnt = static_cast<int64_t>(dupOf.size());

    if (dupOf.empty() || !working) {
        return;
    }

    if (paraRecord.optionFlag & query_flag_print_all) {
        QueryGameRecord* qgr = nullptr;
        if (paraRecord.optionFlag & query_flag_print_pgn) {
            qgr = new QueryGameRecord(*mDb, searchField, fullGameQueryString, variant);
        }

        for (auto&& [dup, kept] : dupOf) {
            std::string str = ">>>>> Duplicate: " + std::to_string(dup) + " of " + std::to_string(kept);
            if (qgr) {
                banksia::PgnRecord record0, record1;
                record0.gameID = kept;
                record1.gameID = dup;
                str += "\n\n" + qgr->queryAndCreatePGNByGameID(record0) + "\n" + qgr->queryAndCreatePGNByGameID(record1);
            }
            printOut.printOut(str);
        }
        delete qgr;
    }

    if ((paraRecord.optionFlag & dup_flag_remove) == 0 || !working) {
        return;
    }

    threadMap.clear(); // their statements

    const auto hasComments = hasTable(*mDb, "Comments");

    SQLite::Statement removeGame(*mDb, "DELETE FROM Games WHERE ID = ?");
    SQLite::Statement* removeComments = hasComments ? new SQLite::Statement(*mDb, "DELETE FROM Comments WHERE GameID = ?") : nullptr;

    mDb->exec("BEGIN");
    for (auto&& it : dupOf) {
        removeGame.reset();
        removeGame.bind(1, it.first);
        delCnt += removeGame.exec();
        if (removeComments) {
            removeComments->reset();
            removeComments->bind(1, it.first);
            removeComments->exec();
        }
    }
    delete removeComments;

    if (delCnt > 0 && hasTable(*mDb, "Tree")) {
        mDb->exec("DROP TABLE Tree");
        mDb->exec("DELETE FROM Info WHERE Name = 'TreeNodeCount'");
        std::cout << "The opening tree (table Tree) included the duplicates and was dropped" << std::endl;
    }
    refreshInfoCounts(*mDb);
    mDb->exec("COMMIT");
}
