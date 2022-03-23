/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "report.h"

namespace ocgdb {
    Report printOut;
};

using namespace ocgdb;

void Report::init(bool print, const std::string& path)
{
    printConsole = print;
    if (!path.empty()) {
        ofs = bslib::Funcs::openOfstream2write(path);
        openingstream = ofs.is_open();
    }
}

void Report::printOut(const std::string& str)
{
    if (str.empty()) return;

    if (openingstream) {
        std::lock_guard<std::mutex> dolock(ofsMutex);
        ofs << str << std::endl;
    }
    
    if (printConsole) {
        std::lock_guard<std::mutex> dolock(printMutex);
        std::cout << str << std::endl;
    }
}

void Report::printOutPgn(const bslib::PgnRecord& record)
{
    std::string str;
    for(auto && it : record.tags) {
        std::string s = "[" + it.first + " \"" + it.second + "\"\n";

        if (it.first == "Event") {
            str = s + str;
        } else {
            str += s;
        }
    }
    if (str.empty()) return;
    str += "\n";
    
    if (!record.moveString.empty()) {
        str += record.moveString;
    } else if (record.moveText && record.moveText[0]) {
        str += record.moveText;
    }
    
    printOut(str);
}

void Report::close()
{
    if (openingstream && ofs.is_open()) {
        ofs.close();
    }
    openingstream = false;
}
