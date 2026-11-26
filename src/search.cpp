/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <algorithm>
#include <iostream>
#include <sstream>

#include "search.h"
#include "report.h"

using namespace ocgdb;

std::vector<std::string> Search::benchQueries()
{
#ifdef _CHESS_
    return std::vector<std::string> {
        "Q = 3",                         // three White Queens
        "r[e4, e5, d4, d5] = 2",         // two black Rooks in middle squares
        "P[d4, e5, f4, g4] = 4 and kb7", // White Pawns in d4, e5, f4, g4 and black King in b7
        "B[c-f] + b[c-f] == 2",          // There are two Bishops (any side) from column c to f
        "white6 = 5",                    // There are 5 white pieces on row 6
        "mate and move[N, n]",           // mate by a knight move
    };
#else
    return std::vector<std::string> {
        "R = 2 and C = 2",               // both Red rooks and cannons on the board
        "c[e] = 2",                      // two Black cannons on file e
        "H[c-g] >= 2 and h[c-g] >= 2",   // two horses of each side in the middle files
        "P[4] = 5",                      // five Red pawns on rank 4
        "mate and move[H, h]",           // mate by a horse move
    };
#endif
}

std::string Search::trimQuery(std::string query)
{
    // remove comments by //
    for (;;) {
        auto p = query.find("//");
        if (p == std::string::npos) {
            break;
        }
        auto q = query.find('\n', p);
        query = query.substr(0, p) + (q == std::string::npos ? "" : query.substr(q + 1));
    }

    banksia::trim(query);
    return query;
}

bool Search::runTask()
{
    if (paraRecord.task == Task::bench) {
        std::cout << "Benchmark: searching positions with " << benchQueries().size() << " queries" << std::endl;
        paraRecord.queries = benchQueries();
        paraRecord.pgnPaths.clear();
    }

    auto ok = true;
    for (auto&& q : paraRecord.queries) {
        if (!working) break;
        ok = searchAQuery(q) && ok;
    }
    return ok;
}

void Search::printStats(bool completed) const
{
    auto elapsed = elapsedMs();
    std::lock_guard<std::mutex> dolock(printMutex);
    std::cout << "#games searched: " << processedCnt << ", found: " << succCount << ", elapsed: " << elapsed << " ms "
              << banksia::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << processedCnt * 1000LL / elapsed << " games/s";
    if (fileSize > 0 && !completed) {
        std::cout << ", " << processedPgnSz * 100 / fileSize << "%";
    }
    std::cout << std::endl;
}

bool Search::searchAQuery(const std::string& _query)
{
    query = trimQuery(_query);
    if (query.empty()) {
        return true;
    }

    std::cout << "Query: " << query << std::endl;
    parsedVariant = banksia::ChessVariant::none;

    auto ok = true;
    for (auto&& path : paraRecord.pgnPaths) {
        if (!working) break;
        ok = searchAPgnFile(path, query) && ok;
    }

    for (auto&& dbPath : paraRecord.dbPaths) {
        if (!working) break;
        ok = searchADatabase(dbPath, query) && ok;
    }
    return ok;
}

/// parses the query for the variant (the letters of the pieces and the squares are the variant's)
static bool parseFor(Parser& parser, banksia::ChessVariant variant, banksia::ChessVariant& parsedVariant,
                     const std::string& query)
{
    if (parsedVariant == variant) {
        return true;
    }
    if (!parser.parse(variant, query.c_str())) {
        std::cerr << "Error: " << parser.getErrorString() << std::endl;
        parsedVariant = banksia::ChessVariant::none;
        return false;
    }
    parsedVariant = variant;
    if (debugMode) {
        parser.printTree();
    }
    return true;
}

void Search::setupCallback()
{
    const int serial = ++searchSerial;
    const auto wantFen = (paraRecord.optionFlag & query_flag_print_fen) != 0;
    const auto wantPgn = (paraRecord.optionFlag & query_flag_print_pgn) != 0;
    const auto benchMode = paraRecord.task == Task::bench;

    checkToStop = [=, this](const banksia::BitboardBase* bb, const banksia::BoardCore* board,
                            const banksia::PgnRecord* record) -> bool {
        if (!bb || !board || !record || !working) {
            return false;
        }

        /// the game's progress through "A then B": per thread, which reads one game at a time
        thread_local int stateSerial = -1, stage = 0;
        thread_local int64_t stateGameId = -1;
        if (stateSerial != serial || stateGameId != record->gameID) {
            stateSerial = serial;
            stateGameId = record->gameID;
            stage = 0;
        }

        PqlContext ctx;
        ctx.bb = bb;
        ctx.board = board;
        if (!parser.match(ctx, stage)) {
            return false;
        }

        auto n = ++succCount;
        if (paraRecord.resultNumberLimit > 0 && n > paraRecord.resultNumberLimit) {
            working = false;
            return true;
        }

        if (benchMode) {
            return true; // counted only
        }

        Found found;
        found.id = record->gameID;
        found.ply = board->getHistListSize();
        if (wantFen) {
            found.fen = board->getFen();
        }
        if (wantPgn && fromPgn) {
            /// the game as it is in the file
            std::ostringstream ss;
            for (auto&& it : record->tags) {
                if (it.first != "Event") continue;
                ss << "[Event \"" << it.second << "\"]\n";
            }
            for (auto&& it : record->tags) {
                if (it.first == "Event") continue;
                ss << "[" << it.first << " \"" << it.second << "\"]\n";
            }
            ss << "\n" << (record->moveText ? record->moveText : record->moveString.c_str()) << "\n";
            found.pgn = ss.str();
        }

        std::lock_guard<std::mutex> dolock(foundMutex);
        foundVec.push_back(std::move(found));
        return true;
    };
}

void Search::printResults(const std::string& source)
{
    /// -bench: the counts only
    if (paraRecord.task == Task::bench) {
        totalFound += static_cast<int64_t>(foundVec.size());
        foundVec.clear();
        return;
    }

    std::sort(foundVec.begin(), foundVec.end(), [](const Found& a, const Found& b) { return a.id < b.id; });

    if (paraRecord.resultNumberLimit > 0 && static_cast<int64_t>(foundVec.size()) > paraRecord.resultNumberLimit) {
        foundVec.resize(paraRecord.resultNumberLimit);
    }

    QueryGameRecord* qgr = nullptr;
    if (!fromPgn && (paraRecord.optionFlag & query_flag_print_pgn) && mDb) {
        qgr = new QueryGameRecord(*mDb, searchField, fullGameQueryString, variant);
    }

    printOut.printOut("; Query: " + query + "\n; Source: " + source + "\n; Found: " + std::to_string(foundVec.size()));

    int64_t cnt = 0;
    for (auto&& f : foundVec) {
        ++cnt;
        std::string str = std::to_string(cnt) + ". " + (fromPgn ? "game #" : "ID ") + std::to_string(f.id) + ", ply "
                          + std::to_string(f.ply);
        if (!f.fen.empty()) {
            str += ", FEN: " + f.fen;
        }
        if (qgr) {
            banksia::PgnRecord record;
            record.gameID = static_cast<int>(f.id);
            str += "\n" + qgr->queryAndCreatePGNByGameID(record);
        } else if (!f.pgn.empty()) {
            str += "\n" + f.pgn;
        }
        printOut.printOut(str);
    }

    delete qgr;
    totalFound += static_cast<int64_t>(foundVec.size());
    foundVec.clear();
}

bool Search::searchADatabase(const std::string& dbPath, const std::string& query)
{
    std::cout << "Searching " << dbPath << std::endl;

    if (!openOCGDB(dbPath, true)) {
        return false;
    }

    if (searchField == SearchField::none) {
        std::cerr << "Error: database " << dbPath << " has no column for moves (Moves, Moves1 or Moves2)" << std::endl;
        closeDb();
        return false;
    }

    if (!parseFor(parser, variant, parsedVariant, query)) {
        closeDb();
        return false;
    }

    startTime = getNow();
    processedCnt = succCount = 0;
    foundVec.clear();
    fromPgn = false;

    setupCallback();
    boardCallback = nullptr;
    wantHeaders = false;
    wantBitboards = true;
    /// The main line only: with variations the history is not the path to the position, so
    /// "ply", the last move and "then" would be wrong
    mainLineOnly = true;

    auto ok = true;
    try {
        auto sql = gamesQuery(false);
        if (debugMode) {
            std::cout << "SQL: " << sql << std::endl;
        }
        ok = readGames(sql);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        ok = false;
    }

    auto stoppedByLimit = !working && paraRecord.resultNumberLimit > 0;
    working = true;

    printStats(true);
    if (stoppedByLimit) {
        std::cout << "Stopped: " << paraRecord.resultNumberLimit << " results reached (-resultcount)" << std::endl;
    }
    printResults(dbPath);
    closeDb();
    return ok;
}

bool Search::searchAPgnFile(const std::string& path, const std::string& query)
{
    std::cout << "Searching " << path << std::endl;

    variant = paraRecord.variant == banksia::ChessVariant::none ? defaultVariant() : paraRecord.variant;
    if (!parseFor(parser, variant, parsedVariant, query)) {
        return false;
    }

    startTime = getNow();
    processedCnt = succCount = 0;
    foundVec.clear();
    fromPgn = true;

    setupCallback();

    processPgnFile(path);

    auto stoppedByLimit = !working && paraRecord.resultNumberLimit > 0;
    working = true;

    printStats(true);
    if (stoppedByLimit) {
        std::cout << "Stopped: " << paraRecord.resultNumberLimit << " results reached (-resultcount)" << std::endl;
    }
    printResults(path);
    return true;
}

void Search::processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                     const std::vector<int8_t>& moveVec)
{
    assert(t);
    t->initForBoards(variant);
    extractBoard(t->board, record, moveVec, true);
    t->gameCnt++;
}

void Search::processPGNGameWithAThread(ThreadRecord* t, const std::unordered_map<const char*, char*>& tagMap,
                                       const char* moveText)
{
    assert(t);
    t->initForBoards(variant);

    banksia::PgnRecord record;
    record.gameID = static_cast<int32_t>(currentGameIndex);
    record.moveText = moveText;
    record.variant = variant;

    for (auto&& it : tagMap) {
        std::string name = it.first, s = it.second;
        if (name == "FEN") {
            record.fenText = s;
        }
        record.tags[name] = s;
    }

    /// a game of another variant is not searched
    auto it = record.tags.find("Variant");
    if (it != record.tags.end()) {
        auto v = banksia::string2ChessVariant(it->second);
        if (v != banksia::ChessVariant::none && v != variant) {
            return;
        }
    }

    if (!t->board->newGame(record.fenText)) {
        errCnt++;
        return;
    }

    int flag = banksia::ParseMoveListFlag_quick_check | banksia::ParseMoveListFlag_discardComment
               | banksia::ParseMoveListFlag_create_bitboard;

    /// the main line only, as for databases
    t->board->fromMoveList(record, banksia::Notation::san, banksia::ParseVariation::discard, banksia::moveFormat, flag,
                           checkToStop);
    t->gameCnt++;
}
