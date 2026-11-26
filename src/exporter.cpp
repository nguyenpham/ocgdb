/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <fstream>
#include <iostream>

#include "exporter.h"
#include "report.h"

using namespace ocgdb;

bool Exporter::runTask()
{
    auto dbPath = paraRecord.dbPaths.front();
    auto pgnPath = paraRecord.pgnPaths.front();

    if (!openOCGDB(dbPath, true)) {
        return false;
    }
    if (searchField == SearchField::none) {
        std::cerr << "Error: database " << dbPath << " has no column for moves" << std::endl;
        return false;
    }

    std::ofstream ofs(pgnPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Error: can't write " << pgnPath << std::endl;
        return false;
    }

    std::cout << "Exporting " << dbPath << " to " << pgnPath << std::endl;

    startTime = getNow();
    gameCnt = 0;
    try {
        forEachGameAsPgn([&](const std::string& pgn) {
            ofs << pgn << "\n";
            ++gameCnt;
            if ((gameCnt & 0xfff) == 0) {
                printProgress();
            }
            return paraRecord.gameNumberLimit <= 0 || gameCnt < paraRecord.gameNumberLimit;
        });
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    ofs.close();
    printStats(true);
    return true;
}

bool Extract::runTask()
{
    auto dbPath = paraRecord.dbPaths.front();
    if (!openOCGDB(dbPath, true)) {
        return false;
    }

    QueryGameRecord qgr(*mDb, searchField, fullGameQueryString, variant);
    for (auto&& gameID : paraRecord.gameIDVec) {
        banksia::PgnRecord record;
        record.gameID = gameID;
        auto pgn = qgr.queryAndCreatePGNByGameID(record);
        if (pgn.empty()) {
            std::cerr << "Error: there is no game with ID " << gameID << std::endl;
            continue;
        }

        std::string str = "; ID: " + std::to_string(gameID) + "\n" + pgn;
        if (paraRecord.optionFlag & query_flag_print_fen) {
            str += "; FEN of the last position: " + qgr.board->getFen() + "\n";
        }
        printOut.printOut(str);
    }
    return true;
}
