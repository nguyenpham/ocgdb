/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "exporter.h"

using namespace ocgdb;

bool Exporter::openDB(const std::string& dbPath)
{
    if (!DbRead::openDB(dbPath)) {
        return false;
    }
    
    flag = bslib::BoardCore::ParseMoveListFlag_create_san;

    if (searchField == SearchField::moves1) {
        flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
    }
    
    if (searchField != SearchField::moves) {
        for(auto && t : threadMap) {
            t.second.queryComments = new SQLite::Statement(*mDb, "SELECT * FROM Comments WHERE GameID = ?");
        }
    }
    return true;
}

void Exporter::runTask()
{
    startTime = getNow();

    auto pgnPath = paraRecord.pgnPaths.front();
    pgnOfs = bslib::Funcs::openOfstream2write(pgnPath);

    paraRecord.optionFlag |= query_flag_print_pgn;
    
    for(auto && dbPath : paraRecord.dbPaths) {
        std::cout   << "Convert a database into a PGN file...\n"
                    << "DB path : " << dbPath
                    << "\nPGN path: " << pgnPath
                    << std::endl;

        readADb(dbPath, DbRead::fullGameQueryString);
    }
    
    pgnOfs.close();
}

void Exporter::processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(!record.moveString.empty() || record.moveText || !moveVec.empty());
    assert(record.gameID > 0);

    auto threadId = std::this_thread::get_id();
    ThreadRecord* t;
    
    {
        std::lock_guard<std::mutex> dolock(threadMapMutex);
        t = &threadMap[threadId];
    }

    if (!t->board) {
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
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
