/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "search.h"

using namespace ocgdb;

Search::~Search()
{
    if (qgr) {
        delete qgr;
        qgr = nullptr;
    }
}

void Search::runTask()
{
    std::cout   << "Querying..." << std::endl;

    if (paraRecord.dbPaths.empty() && paraRecord.pgnPaths.empty()) {
        std::cout << "Error: there is no path for database nor PGN files" << std::endl;
        return;
    }
    
    if (paraRecord.queries.empty()) {
        std::cout << "Error: there is no query" << std::endl;
        return;
    }
    
    assert(paraRecord.task != Task::create);

    gameCnt = commentCnt = 0;
    eventCnt = playerCnt = siteCnt = 1;
    errCnt = 0;
    succCount = 0;

    checkToStop = nullptr;
    
    boardCallback = [=](const bslib::BoardCore* board, const bslib::PgnRecord* record) -> bool {
        assert(board);
        
        for(int i = 1, n = board->getHistListSize(); i <= n; i++) {
            std::vector<uint64_t> bitboardVec;

            if (i < n) {
                auto hist = board->_getHistPointerAt(i);
                assert(hist && !hist->bitboardVec.empty());
                bitboardVec = hist->bitboardVec;
            } else {
                // last position
                bitboardVec = board->posToBitboards();
            }

            if (!parser.evaluate(bitboardVec)) {
                continue;
            }

            succCount++;

            if (paraRecord.optionFlag & query_flag_print_all) {
                std::lock_guard<std::mutex> dolock(printMutex);

                std::cout << succCount << ". gameId: " << (record ? record->gameID : -1) << std::endl;
            }

            if (printOut.isOn()) {
                if (paraRecord.optionFlag & query_flag_print_fen) {
                    std::string str = std::to_string(succCount) + ". gameId: " + std::to_string(record ? record->gameID : -1) +
                                ", fen: " + board->getFen() + "\n";
                    printOut.printOut(str);
                }

                static std::string printOutQuery;

                if (query != printOutQuery) {
                    printOutQuery = query;
                    printOut.printOut("; >>>>>> Query: " + query + "\n");
                }
                if (qgr) {
                    printGamePGNByIDs(*qgr, std::vector<int>{record->gameID});
                } else {
                    printOut.printOutPgn(*record);
                }
            }

            return true;
        }

        return false;
    };

    
    for(auto && _query : paraRecord.queries) {
        query = _query;

        // remove comments by //
        if (query.find("//") != std::string::npos) {
            while(true) {
                auto p = query.find("//");
                if (p == std::string::npos) {
                    break;
                }
                
                auto q = p + 2;
                for(; q < query.size(); q++) {
                    auto ch = query.at(q);
                    if (ch == '\n') {
                        q++;
                        break;
                    }
                }
                
                auto s = query.substr(0, p);
                if (q >= query.size()) {
                    query = s;
                    break;
                } else {
                    auto s2 = query.substr(q);
                    query = s + s2;
                }
            }
        }
        
        bslib::Funcs::trim(query);

        if (query.empty()) {
            continue;;
        }

        std::cout << "Search with query " << query <<  "..." << std::endl;
        
        assert(paraRecord.task != Task::create);
        if (!parser.parse(chessVariant, query.c_str())) {
            std::cerr << "Error: " << parser.getErrorString() << std::endl;
            continue;;
        }

        // Query PGN files
        for(auto && path : paraRecord.pgnPaths) {
            startTime = getNow();
            processPgnFile(path);
        }

        // Query databases
        if (!paraRecord.dbPaths.empty()) {
            auto queryString = (paraRecord.optionFlag & query_flag_print_pgn) ? DbRead::fullGameQueryString : "SELECT * FROM Games";
            for(auto && dbPath : paraRecord.dbPaths) {
                gameCnt = commentCnt = 0;
                eventCnt = playerCnt = siteCnt = 1;
                errCnt = 0;
                readADb(dbPath, queryString);
            }
        }
    }
}


void Search::processAGameWithAThread(ThreadRecord* t, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    assert(t);

    if (!t->board) {
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    }
    assert(t->board);
    
    t->board->newGame(record.fenText);
    
    int flag = bslib::BoardCore::ParseMoveListFlag_create_bitboard;
    if (searchField == SearchField::moves) { // there is a text move only
        flag |= bslib::BoardCore::ParseMoveListFlag_quick_check;
        t->board->fromMoveList(&record, bslib::Notation::san, flag, checkToStop);
    } else {
        if (searchField == SearchField::moves1) {
            flag |= bslib::BoardCore::ParseMoveListFlag_move_size_1_byte;
        }
        
        if (paraRecord.optionFlag & query_flag_print_pgn) {
            flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
        }
        t->board->fromMoveList(&record, moveVec, flag, checkToStop);
    }

    t->hdpLen += t->board->getHistListSize();

    if (boardCallback) {
        boardCallback(t->board, &record);
    }
    t->gameCnt++;
}

void Search::printStats() const
{
    DbCore::printStats();
    std::cout << " #succ: " << succCount << std::endl;

}

void Search::processPGNGameWithAThread(ThreadRecord* t, const std::unordered_map<char*, char*>& tagMap, const char* moveText)
{
    assert(t);

    if (!t->board) {
        t->board = bslib::Funcs::createBoard(bslib::ChessVariant::standard);
    }
    assert(t->board);
    
    bslib::PgnRecord record;
    {
        std::lock_guard<std::mutex> dolock(gameIDMutex);
        ++gameCnt;
        record.gameID = gameCnt;
    }

    record.moveText = moveText;
    
    for(auto && it : tagMap) {
        auto name = std::string(it.first), s = std::string(it.second);
        if (name == "FEN") {
            record.fenText = s;
        }
        record.tags[name] = s;
    }

    // Parse moves
    t->board->newGame(record.fenText);

    int flag = bslib::BoardCore::ParseMoveListFlag_quick_check
                | bslib::BoardCore::ParseMoveListFlag_discardComment
                | bslib::BoardCore::ParseMoveListFlag_create_bitboard;

    if (paraRecord.optionFlag & query_flag_print_pgn) {
        flag |= bslib::BoardCore::ParseMoveListFlag_create_san;
    }
    t->board->fromMoveList(&record, bslib::Notation::san, flag, checkToStop);
    
    if (boardCallback) {
        boardCallback(t->board, &record);
    }

}

bool Search::openDB(const std::string& dbPath)
{
    if (DbRead::openDB(dbPath)) {
        if (qgr) {
            delete qgr;
        }

        startTime = getNow();
        
        qgr = new QueryGameRecord(*mDb, searchField);
        return true;
    }
    return false;
}

void Search::closeDb()
{
    if (qgr) {
        delete qgr;
        qgr = nullptr;
    }
    DbRead::closeDb();
}


void Search::setupForBench(ParaRecord& paraRecord)
{
    std::cout << "Benchmark position searching..." << std::endl;

    paraRecord.queries = std::vector<std::string> {
        "Q = 3",                            // three White Queens
        "r[e4, e5, d4,d5]= 2",              // two black Rooks in middle squares
        "P[d4, e5, f4, g4] = 4 and kb7",    // White Pawns in d4, e5, f4, g4 and black King in b7
        "B[c-f] + b[c-f] == 2",               // There are two Bishops (any side) from column c to f
        "white6 = 5",                        // There are 5 white pieces on row 6
    };

//    searchPostion(paraRecord);
}
