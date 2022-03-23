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
