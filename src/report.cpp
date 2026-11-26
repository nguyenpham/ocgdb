/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>

#include "report.h"

namespace ocgdb {
    Report printOut;
}

using namespace ocgdb;

void Report::init(bool print, const std::string& path)
{
    close();
    printConsole = print;
    if (!path.empty()) {
        ofs.open(path, std::ios::out | std::ios::trunc);
        openingstream = ofs.is_open();
        if (!openingstream) {
            std::cerr << "Error: can't open the report file " << path << std::endl;
        }
    }
}

void Report::printOut(const std::string& str)
{
    if (str.empty()) return;

    std::lock_guard<std::mutex> dolock(printMutex);
    if (openingstream) {
        ofs << str << "\n";
    }

    if (printConsole) {
        std::cout << str << std::endl;
    }
}

void Report::close()
{
    std::lock_guard<std::mutex> dolock(printMutex);
    if (openingstream && ofs.is_open()) {
        ofs.close();
    }
    openingstream = false;
}
