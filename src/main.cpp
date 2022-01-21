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

int main(int argc, const char * argv[]) {
    std::cout << "Open Chess Game Database Standard, Database Builder, (C) 2021\n";
    
    // init
    {
        bslib::ChessBoard::staticInit();
    }

    ocgdb::ParaRecord paraRecord;
    
    // for creating
    paraRecord.columnMovesMode = ocgdb::ColumnMovesMode::moves2;
//    paraRecord.optionSet.insert(ocgdb::Option::comment_discard);
//    paraRecord.optionSet.insert(ocgdb::Option::site_discard);
//    paraRecord.gameNumberLimit = 0xff;
    
    // for querying
//    paraRecord.optionSet.insert(ocgdb::Option::query_print_all);
    paraRecord.optionSet.insert(ocgdb::Option::query_print_fen);

    auto keepMovesField = false, setupMovesFields = false;
    auto encodeSize = 0;
    for(auto i = 1; i < argc; i++) {
        auto str = std::string(argv[i]);
        if (str == "-bench") {
            paraRecord.task = ocgdb::Task::bench;
            continue;
        }
        if (str == "-keepmoves") {
            keepMovesField = true;
            setupMovesFields = true;
            continue;
        }

        if (i + 1 >= argc) continue;

        if (str == "-pgn") {
            auto pgnPath = std::string(argv[++i]);
            paraRecord.pgnPaths.push_back(pgnPath);
            continue;
        }
        if (str == "-db") {
            paraRecord.dbPath = std::string(argv[++i]);
            continue;
        }
        if (str == "-cpu") {
            paraRecord.cpuNumber = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-elo") {
            paraRecord.lowElo = std::atoi(argv[++i]);
            continue;
        }
        if (str == "-encode") {
            encodeSize = std::atoi(argv[++i]);
            setupMovesFields = true;
            continue;
        }
        if (str == "-q") {
            paraRecord.task = ocgdb::Task::query;
            auto query = std::string(argv[++i]);
            paraRecord.queries.push_back(query);
            continue;
        }
    }
    
    if (setupMovesFields) {
        paraRecord.columnMovesMode = ocgdb::ColumnMovesMode::none;

        if (encodeSize == 1) {
            paraRecord.columnMovesMode = keepMovesField ? ocgdb::ColumnMovesMode::moves_moves1 : ocgdb::ColumnMovesMode::moves1;
        } else if (encodeSize == 2) {
            paraRecord.columnMovesMode = keepMovesField ? ocgdb::ColumnMovesMode::moves_moves2 : ocgdb::ColumnMovesMode::moves2;
        } else if (keepMovesField) {
            paraRecord.columnMovesMode = ocgdb::ColumnMovesMode::moves;
        }
    }

    std::cout << "Paras:\n" << paraRecord.toString() << std::endl;

    if (paraRecord.isValid()) {
        ocgdb::Builder oc;
        oc.runTask(paraRecord);
        return 0;
    }
    
    auto errorString = paraRecord.getErrorString();
    if (!errorString.empty()) {
        std::cerr << "Error: " << errorString << "\n" << std::endl;
    }

    {
        std::cerr << "Usage:" << std::endl;
        std::cerr << " ocgdb [<options>]" << std::endl;
        std::cerr << std::endl;
        std::cerr << " -pgn <file>           PGN game database file" << std::endl;
        std::cerr << " -db <file>            create database, extension should be .ocgdb.db3" << std::endl;
        std::cerr << "                       use :memory: to create in-memory database" << std::endl;
        std::cerr << " -cpu <n>              number of threads, omit for all cores, works with -pgn, -bench" << std::endl;
        std::cerr << " -elo <n>              low limit of Elo" << std::endl;
        std::cerr << " -keepmoves            keep field Moves when creating db, works with -pgn, -db" << std::endl;
        std::cerr << " -encode <n>           n must be 0 (not binary moves), 1 or 2 for field Moves1 or Moves2 when creating db" << std::endl;

        std::cerr << " -bench                benchmarch querying games speed, works with -db and -cpu" << std::endl;
        std::cerr << " -q <query>            querying positions, works with -db and -cpu" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << " ocgdb -pgn c:\\games\\big.png -db c:\\db\\big.ocgdb.db3 -cpu 4 -encode 2" << std::endl;
        std::cerr << " ocgdb -pgn c:\\games\\big.png -db :memory: -keepmoves" << std::endl;
        std::cerr << " ocgdb -bench -db c:\\db\\big.ocgdb.db3 -cpu 4" << std::endl;
        std::cerr << " ocgdb -db c:\\db\\big.ocgdb.db3 -cpu 4 -q \"Q=3\" -q\"P[d4, e5, f4, g4] = 4 and kb7\"" << std::endl;
    }
    return 1;
}
