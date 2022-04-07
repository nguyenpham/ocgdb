/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>

#include "records.h"
#include "report.h"
#include "search.h"
#include "exporter.h"
#include "duplicate.h"
#include "builder.h"
#include "extract.h"
#include "addgame.h"

#include "board/chess.h"

void print_usage();
extern bool debugMode;

void runTask(ocgdb::ParaRecord& param)
{
    ocgdb::DbCore* dbCore = nullptr;

    switch (param.task) {
        case ocgdb::Task::create:
        {
            dbCore = new ocgdb::Builder;
            break;
        }
        case ocgdb::Task::export_:
        {
            dbCore = new ocgdb::Exporter;
            break;
        }
        case ocgdb::Task::dup:
        {
            dbCore = new ocgdb::Duplicate;
            break;
        }
        case ocgdb::Task::bench:
        {
            auto search = new ocgdb::Search;
            search->setupForBench(param);
            dbCore = search;
            break;
        }
        case ocgdb::Task::query:
        {
            dbCore = new ocgdb::Search;
            break;
        }
        case ocgdb::Task::getgame:
        {
            dbCore = new ocgdb::Extract;
            break;
        }
        case ocgdb::Task::merge:
        {
            dbCore = new ocgdb::AddGame;
            break;
        }

        default:
            break;
    }
    
    if (dbCore) {
        dbCore->run(param);
        delete dbCore;
    }
}

void printConflictedTasks(ocgdb::Task task0, ocgdb::Task task1)
{
    std::cerr << "Error: multi/conflicted tasks: " << ocgdb::ParaRecord::toString(task0) << " vs "  << ocgdb::ParaRecord::toString(task1) << std::endl;
}

int main(int argc, const char * argv[]) {
    std::cout << "Open Chess Game Database Standard (OCGDB), (C) 2022 - version: " << ocgdb::VersionString << "\n" << std::endl;
    
    if (argc < 2) {
        print_usage();
        return 0;
    }

    // init
    {
        bslib::ChessBoard::staticInit();
    }

    auto errCnt = 0;
    ocgdb::ParaRecord paraRecord;
    
    for(auto i = 1; i < argc; i++) {
        auto oldTask = paraRecord.task;
        auto str = std::string(argv[i]);
        if (str == "-bench") {
            paraRecord.task = ocgdb::Task::bench;
            continue;
        }
        if (str == "-debug") {
            debugMode = true;
            continue;
        }
        if (str == "-create" || str == "-merge" || str == "-export" || str == "-dup") {
            if (str == "-create") {
                paraRecord.task = ocgdb::Task::create;
            } else if (str == "-merge") {
                paraRecord.task = ocgdb::Task::merge;
            } else if (str == "-export") {
                paraRecord.task = ocgdb::Task::export_;
            } else if (str == "-dup") {
                paraRecord.task = ocgdb::Task::dup;
            }
            if (oldTask != ocgdb::Task::none) {
                errCnt++;
                printConflictedTasks(oldTask, paraRecord.task);
            }
            continue;
        }

        if (i + 1 >= argc) continue;

        if (str == "-pgn") {
            paraRecord.pgnPaths.push_back(std::string(argv[++i]));
            continue;
        }
        if (str == "-db") {
            paraRecord.dbPaths.push_back(std::string(argv[++i]));
            continue;
        }
        if (str == "-r") {
            paraRecord.reportPath = std::string(argv[++i]);
            continue;
        }
        if (str == "-cpu") {
            paraRecord.cpuNumber = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-elo") {
            paraRecord.limitElo = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-o") {
            auto optionString = std::string(argv[++i]);
            paraRecord.setupOptions(optionString);
            continue;
        }
        if (str == "-plycount") {
            paraRecord.limitLen = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-resultcount") {
            paraRecord.resultNumberLimit = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-q" || str == "-g") {
            if (str == "-q") {
                paraRecord.task = ocgdb::Task::query;
                auto query = std::string(argv[++i]);
                paraRecord.queries.push_back(query);
            } else {
                paraRecord.task = ocgdb::Task::getgame;
                paraRecord.gameIDVec.push_back(std::atoi(argv[++i]));
            }
            if (oldTask != ocgdb::Task::none) {
                errCnt++;
                printConflictedTasks(oldTask, paraRecord.task);
                break;
            }
            continue;
        }
        errCnt++;
        std::cerr << "Error: unknown parameter: " << str << "\n" << std::endl;
        break;
    }
    
    if (errCnt == 0) {
        if (debugMode) {
            std::cout << "All parameters:\n" << paraRecord.toString() << std::endl;
        }
        
        if (paraRecord.isValid()) {
            runTask(paraRecord);
            return 0;
        }
        
        auto errorString = paraRecord.getErrorString();
        if (!errorString.empty()) {
            std::cerr << "Error: " << errorString << "\n" << std::endl;
        }
    }

    print_usage();
    return 1;
}

void print_usage()
{
    
    const std::string str =
    "Usage:\n" \
    " ocgdb [<parameters>]\n" \
    "\n" \
    " -create               create a new database from multi PGN files, works with -db, -pgn\n" \
    " -merge                merge multi PGN files or databases into the first database, works with -db, -pgn\n" \
    " -dup                  check duplicate games in databases, works with -db\n" \
    " -export               export from a database into a PGN file, works with -db, -pgn\n" \
    " -bench                benchmarch querying games speed, works with -db\n" \
    " -q <query>            querying positions, repeat to add multi queries, works with -db, -pgn\n" \
    " -g <id>               get game with game ID numbers (repeat to add multi IDs), works with -db, -pgn\n" \
    " -pgn <file>           PGN game database file, repeat to add multi files\n" \
    " -db <file>            database file, extension should be .ocgdb.db3, repeat to add multi files\n" \
    " -r <file>             report file, works with -g, -q, -dup\n" \
    "                       use :memory: to create in-memory database\n" \
    " -elo <n>              discard games with Elo under n (for creating)\n" \
    " -plycount <n>         discard games with ply-count under n (for creating)\n" \
    " -resultcount <n>      stop querying if the number of results above n (for querying)\n" \
    " -cpu <n>              number of threads, should <= total physical cores, omit it for using all cores\n" \
    " -o [<options>,]       options, separated by commas\n" \
    "    moves              create text move field Moves\n" \
    "    moves1             create binary move field Moves, 1-byte encoding\n" \
    "    moves2             create binary move field Moves, 2-byte encoding\n" \
    "    acceptnewtags      create a new field for a new PGN tag (for creating)\n" \
    "    discardcomments    discard all comments (for creating)\n" \
    "    discardsites       discard all Site tag (for creating)\n" \
    "    discardnoelo       discard games without player Elos (for creating)\n" \
    "    discardfen         discard games with FENs (not started from origin; for creating)\n" \
    "    reseteco           re-create all ECO (for creating)\n" \
    "    printall           print all results (for querying, checking duplications)\n" \
    "    printfen           print FENs of results (for querying)\n" \
    "    printpgn           print simple PGNs of results (for querying)\n" \
    "    embededgames       duplicate included games inside other games\n" \
    "    remove             remove duplicate games (for checking duplicates)\n" \
    "    nobot              Lichess: ignore BOT games (for creating a database)\n" \
    "    bot                Lichess: count games with BOT (for creating a database)\n" \
    "\n" \
    "Examples:\n" \
    " ocgdb -create -pgn big.png -db big.ocgdb.db3 -cpu 4 -o moves\n" \
    " ocgdb -create -pgn big1.png -pgn big2.png -db :memory: -elo 2100 -o moves,moves1,discardsites\n" \
    " ocgdb -bench -db big.ocgdb.db3 -cpu 4\n" \
    " ocgdb -db big.ocgdb.db3 -cpu 4 -q \"Q=3\" -q\"P[d4, e5, f4, g4] = 4 and kb7\"\n" \
    " ocgdb -db big.ocgdb.db3 -cpu 4 -q \"fen[K7/N7/k7/8/3p4/8/N7/8 w - - 0 1]\"\n" \
    " ocgdb -db big.ocgdb.db3 -g 423 -g 4432\n" \
    " ocgdb -db big.ocgdb.db3 -dup -o remove,printall\n" \
    " ocgdb -db big.ocgdb.db3 -dup -o remove -r report.txt\n"
    "\n" \
    "Main functions/features:\n" \
    "1. create a SQLite database from multi PGN files\n" \
    "2. merge/add games from some PGN files/databases into a SQLite database\n" \
    "3. export multi SQLite databases to a PGN file\n" \
    "4. get/display PGN games/FEN strings with game IDs from a SQLite database\n" \
    "5. find duplicates/embeded games from multi SQLite databases\n" \
    "6. query games from multi SQLite databases or PGN files, using PQL (Position Query Language)\n" \
    ;

    std::cerr << str << std::endl;
}
