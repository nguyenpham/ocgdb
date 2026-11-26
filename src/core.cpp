/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>

#include "core.h"

using namespace ocgdb;

Core::Core()
{
    resetCnts();
    startTime = getNow();
}

Core::~Core()
{
    if (pool) {
        pool->wait_for_tasks();
    }

    /// the thread records hold statements of the task's connection: gone before it is closed
    threadMap.clear();

    delete pool;
    pool = nullptr;
}

bool Core::run(const ocgdb::ParaRecord& param)
{
    printOut.init(true, param.reportPath);

    paraRecord = param;
    variant = param.variant;
    if (needsPool()) {
        createPool();
    }

    startTime = getNow();
    auto ok = runTask();

    if (pool) {
        pool->wait_for_tasks();
    }
    printOut.close();
    std::cout << (ok ? "Completed!" : "Failed!") << std::endl;
    return ok;
}

std::chrono::steady_clock::time_point Core::getNow()
{
    return std::chrono::steady_clock::now();
}

int64_t Core::elapsedMs() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
}

void Core::createPool()
{
    int cpu = paraRecord.cpuNumber;
    if (cpu <= 0) {
        cpu = std::max(1u, std::thread::hardware_concurrency());
    }
    if (pool) {
        if (static_cast<int>(pool->get_thread_count()) == cpu) {
            return;
        }
        pool->wait_for_tasks();
        delete pool;
    }
    pool = new thread_pool(cpu);
    std::cout << "Threads: " << pool->get_thread_count() << std::endl;
}

void Core::printStats(bool completed) const
{
    auto elapsed = elapsedMs();

    std::lock_guard<std::mutex> dolock(printMutex);

    std::cout << "#games: " << gameCnt << ", elapsed: " << elapsed << " ms, "
              << banksia::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << gameCnt * 1000LL / elapsed << " games/s";
    if (errCnt > 0) {
        std::cout << ", #errors: " << errCnt;
    }
    if (fileSize > 0 && !completed) {
        std::cout << ", " << processedPgnSz * 100 / fileSize << "%";
    }
    std::cout << std::endl;
}

void Core::printProgress()
{
    {
        std::lock_guard<std::mutex> dolock(progressMutex);
        auto now = getNow();
        if (now - lastProgressTime < std::chrono::seconds(5) || now - startTime < std::chrono::seconds(5)) {
            return;
        }
        lastProgressTime = now;
    }
    printStats(false);
}

ThreadRecord* Core::getThreadRecord()
{
    auto threadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> dolock(threadMapMutex);
    return &threadMap[threadId];
}

void Core::resetCnts()
{
    gameCnt = processedCnt = errCnt = succCount = 0;
    eventCnt = playerCnt = siteCnt = 0;
    commentCnt = 0;
    blockCnt = processedPgnSz = fileSize = 0;
}
