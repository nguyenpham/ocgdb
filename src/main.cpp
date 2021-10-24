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

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/SQLiteCpp/VariadicBind.h"

int main(int argc, const char * argv[]) {
    std::cout << "Open Chess Game Database Standard, (C) 2021\n";

    if (argc >= 5 && std::string(argv[1]) == "-pgn" && std::string(argv[3]) == "-db") {
        auto pgnPath = std::string(argv[2]), dbPath = std::string(argv[4]);
        ocgdb::Builder oc;
        oc.convertPgn2Sql(pgnPath, dbPath);
        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "-bench" && std::string(argv[2]) == "-db") {
        auto dbPath = std::string(argv[3]);
        ocgdb::Builder oc;
        oc.bench(dbPath);
        return 0;
    }


    std::cerr << "Usage: ocgdb -pgn PGNPATH -db DBPATH" << std::endl;
    std::cerr << "       ocgdb -bench -db DBPATH" << std::endl;
    std::cerr << " e.g.: ocgdb -pgn c:\\games\\big.png -db c:\\db\\big.ocgdb.db3" << std::endl;
    return 1;
}
