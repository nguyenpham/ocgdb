/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 Developers
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

    ocgdb::Builder oc;
    auto cpu = 0;
    std::string pgnPath;

    for(auto i = 1; i < argc; i++) {
        auto str = std::string(argv[i]);
        if (i + 1 >= argc) continue;;

        if (str == "-pgn") {
            pgnPath = std::string(argv[++i]);
            continue;
        }
        if (str == "-cpu") {
            cpu = std::atoi(argv[++i]);
            continue;
        }
    }
    
    if (!pgnPath.empty()) {
        oc.bench(pgnPath, cpu);
    } else {
        std::cerr << "Usage:" << std::endl;
        std::cerr << " ocgdb [<options>]" << std::endl;
        std::cerr << std::endl;
        std::cerr << " -pgn <file>           PGN game database file" << std::endl;
        std::cerr << " -cpu <n>              number of threads, omit for all cores, works with -pgn" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << " ocgdb -pgn c:\\games\\big.png -cpu 4" << std::endl;
    }
    return 1;
}
