/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_REPORT_H
#define OCGDB_REPORT_H

#include <stdio.h>
#include "records.h"

namespace ocgdb {

class Report
{
public:
    Report() {}
    ~Report() {
        close();
    }
    
    bool isOn() const {
        return printConsole || openingstream;
    }

    void init(bool print, const std::string& path);
    void printOut(const std::string& str);
    void printOutPgn(const bslib::PgnRecord& record);

    void close();

public:
    bool printConsole = true, openingstream = false;
    mutable std::mutex printMutex, ofsMutex;
    std::ofstream ofs;
};

extern Report printOut;

}


#endif /* REPORT_H */
