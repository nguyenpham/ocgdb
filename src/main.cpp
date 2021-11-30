/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>
#include "builder.h"

int main(int argc, const char * argv[]) {
    std::cout << "Open Chess Game Database Standard, (C) 2021\n";
    ocgdb::Builder oc;

    auto benchMode = 0;
    std::string pgnPath, dbPath;
    for(auto i = 1; i < argc; i++) {
        auto str = std::string(argv[i]);
        if (str == "-bench") {
            benchMode = 1;
            continue;
        }
        if (str == "-benchmatch") {
            benchMode = 2;
            continue;
        }

        if (i + 1 >= argc) continue;;

        if (str == "-pgn") {
            pgnPath = std::string(argv[++i]);
            continue;
        }
        if (str == "-db") {
            dbPath = std::string(argv[++i]);
            continue;
        }
    }
    
    auto ok = true;
    
    if (benchMode) {
        if (dbPath.empty()) {
            ok = false;
        } else {
            if (benchMode == 1) {
                oc.bench(dbPath);
            } else {
                oc.benchMatchMoves(dbPath);
            }
        }
    } else {
        if (pgnPath.empty() || dbPath.empty()) {
            ok = false;
        } else {
            oc.convertPgn2Sql(pgnPath, dbPath);
        }
    }

    if (!ok) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << " ocgdb [<options>]" << std::endl;
        std::cerr << std::endl;
        std::cerr << " -pgn <file>           read PGN games from file" << std::endl;
        std::cerr << " -db <file>            create database, extension should be .ocgdb.db3" << std::endl;
        std::cerr << "                       use :memory: to create in memory database" << std::endl;
        std::cerr << " -bench                benchmarch querying games speed" << std::endl;
        std::cerr << " -benchmatch           benchmarch querying game moves speed" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << " ocgdb -pgn c:\\games\\big.png -db c:\\db\\big.ocgdb.db3" << std::endl;
        std::cerr << " ocgdb -pgn c:\\games\\big.png -db :memory:" << std::endl;
        std::cerr << " ocgdb -bench -db c:\\db\\big.ocgdb.db3" << std::endl;
    }
    return 1;
}
