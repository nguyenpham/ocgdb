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
#include "builder.h"
#include "board/chess.h"

void print_usage();
extern bool debugMode;

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
        auto str = std::string(argv[i]);
        if (str == "-bench") {
            paraRecord.task = ocgdb::Task::bench;
            continue;
        }
        if (str == "-export") {
            paraRecord.task = ocgdb::Task::export_;
            continue;
        }
        if (str == "-debug") {
            debugMode = true;
            continue;
        }
//        if (str == "-merge") {
//            paraRecord.task = ocgdb::Task::merge;
//            continue;
//        }
        if (str == "-ags") {
            paraRecord.task = ocgdb::Task::aggressivegamesearch;
            continue;
        }
        if (str == "-dup") {
            paraRecord.task = ocgdb::Task::dup;
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
        if (str == "-q") {
            paraRecord.task = ocgdb::Task::query;
            auto query = std::string(argv[++i]);
            paraRecord.queries.push_back(query);
            continue;
        }
        if (str == "-g") {
            paraRecord.task = ocgdb::Task::getgame;
            paraRecord.gameIDVec.push_back(std::atoi(argv[++i]));
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
            ocgdb::Builder oc;
            oc.runTask(paraRecord);
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
    std::cerr << "Usage:" << std::endl;
    std::cerr << " ocgdb [<parameters>]" << std::endl;
    std::cerr << std::endl;
    std::cerr << " -pgn <file>           PGN game database file, repeat to add multi files" << std::endl;
    std::cerr << " -db <file>            database file, extension should be .ocgdb.db3, repeat to add multi files" << std::endl;
    std::cerr << " -r <file>             report file, works with -g, -q, -dup" << std::endl;
    std::cerr << "                       use :memory: to create in-memory database" << std::endl;
//    std::cerr << " -merge                merge databases into the first one, works with -db, -cpu" << std::endl;
    std::cerr << " -dup                  check duplicate games in databases, works with -db, -cpu, -plycount, -o printall;remove" << std::endl;
    std::cerr << " -ags                  aggressive games search, works with -db, -pgn, -cpu, -r, -o printall;printfen;printpgn" << std::endl;
    std::cerr << " -export               export from a database into a PGN file, works with -db and -pgn" << std::endl;
    std::cerr << " -bench                benchmarch querying games speed, works with -db and -cpu" << std::endl;
    std::cerr << " -q <query>            querying positions, repeat to add multi queries, works with -db and -cpu" << std::endl;
    std::cerr << " -g <id>               get game with game ID number, works with -db, repeat to add multi IDs" << std::endl;

    std::cerr << " -elo <n>              discard games with Elo under n (for creating)" << std::endl;
    std::cerr << " -plycount <n>         discard games with ply-count under n (for creating)" << std::endl;
    std::cerr << " -resultcount <n>      stop querying if the number of results above n (for querying)" << std::endl;
    std::cerr << " -cpu <n>              number of threads, should <= total physical cores, omit it for using all cores" << std::endl;
    std::cerr << " -o [<options>;]       options" << std::endl;
    std::cerr << "    moves              create text move field Moves" << std::endl;
    std::cerr << "    moves1             create binary move field Moves, 1-byte encoding" << std::endl;
    std::cerr << "    moves2             create binary move field Moves, 2-byte encoding" << std::endl;
    std::cerr << "    acceptnewtag       create a new field for a new PGN tag (for creating)" << std::endl;
    std::cerr << "    discardcomments    discard all comments (for creating)" << std::endl;
    std::cerr << "    discardsites       discard all Site tag (for creating)" << std::endl;
    std::cerr << "    discardnoelo       discard games without player Elos (for creating)" << std::endl;
    std::cerr << "    discardfen         discard games with FENs (not started from origin; for creating)" << std::endl;
    std::cerr << "    reseteco           re-create all ECO (for creating)" << std::endl;
    std::cerr << "    printall           print all results (for querying, checking duplications)" << std::endl;
    std::cerr << "    printfen           print FENs of results (for querying)" << std::endl;
    std::cerr << "    printpgn           print simple PGNs of results (for querying)" << std::endl;
    std::cerr << "    embededgames       duplicate included games inside other games" << std::endl;
    std::cerr << "    remove             remove duplicate games (for checking duplicates)" << std::endl;
    std::cerr << "    filter1            filter for advancing pieces to last 3 ranks (for aggressive games search)" << std::endl;
    std::cerr << "    filter2            filter for sacrifices (for aggressive games search)" << std::endl;

    
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << " ocgdb -pgn big.png -db big.ocgdb.db3 -cpu 4 -o moves" << std::endl;
    std::cerr << " ocgdb -pgn big.png -db :memory: -elo 2100 -o moves;moves1;discardsites" << std::endl;
    std::cerr << " ocgdb -bench -db big.ocgdb.db3 -cpu 4" << std::endl;
    std::cerr << " ocgdb -db big.ocgdb.db3 -cpu 4 -q \"Q=3\" -q\"P[d4, e5, f4, g4] = 4 and kb7\"" << std::endl;
    std::cerr << " ocgdb -db big.ocgdb.db3 -cpu 4 -q \"fen[K7/N7/k7/8/3p4/8/N7/8 w - - 0 1]\"" << std::endl;
    std::cerr << " ocgdb -db big.ocgdb.db3 -g 423 -g 4432" << std::endl;
    std::cerr << " ocgdb -db big.ocgdb.db3 -dup -o remove;printall" << std::endl;
    std::cerr << " ocgdb -db big.ocgdb.db3 -dup -o remove -r report.txt" << std::endl;
}
