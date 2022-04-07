/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "dbcore.h"

using namespace ocgdb;

thread_pool* DbCore::pool = nullptr;

DbCore::DbCore()
{
}

DbCore::~DbCore()
{
    threadMap.clear();
    if (mDb) delete mDb;

    if (pool) {
        delete pool;
        pool = nullptr;
    }
}

void DbCore::run(const ocgdb::ParaRecord& param)
{
    printOut.init((param.optionFlag & query_flag_print_all) && (param.optionFlag & query_flag_print_pgn), param.reportPath);

    paraRecord = param;
    createPool();

    runTask();

    ocgdb::printOut.close();
    std::cout << "Completed! " << std::endl;
}

std::chrono::steady_clock::time_point DbCore::getNow()
{
    return std::chrono::steady_clock::now();
}

void DbCore::createPool()
{
    auto cpu = paraRecord.cpuNumber;
    if (cpu < 0) cpu = std::thread::hardware_concurrency();
    if (pool) {
        if (pool->get_thread_count() == cpu) {
            return;
        }
        delete pool;
    }
    pool = new thread_pool(cpu);
    std::cout << "Thread count: " << pool->get_thread_count() << std::endl;
}

void DbCore::printStats() const
{
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
    
    std::lock_guard<std::mutex> dolock(printMutex);

    std::cout << "#games: " << gameCnt
              << ", elapsed: " << elapsed << "ms "
              << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << gameCnt * 1000ULL / elapsed
              << " games/s";
}

ThreadRecord* DbCore::getThreadRecord()
{
    auto threadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> dolock(threadMapMutex);
    return &threadMap[threadId];
}

void DbCore::queryInfo()
{
    playerCnt = eventCnt = gameCnt = siteCnt = -1;
    
    if (!mDb) return;

    SQLite::Statement query(*mDb, "SELECT * FROM Info");
    
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
