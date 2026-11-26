/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_REPORT_H
#define OCGDB_REPORT_H

#include <fstream>
#include <mutex>
#include <string>

namespace ocgdb {

    /// Where results go: the console, and the report file of -r when there is one.
    class Report
    {
    public:
        Report() { }
        ~Report() { close(); }

        bool isOn() const { return printConsole || openingstream; }

        void init(bool print, const std::string& path);
        void printOut(const std::string& str);

        void close();

    public:
        bool printConsole = true, openingstream = false;
        mutable std::mutex printMutex;
        std::ofstream ofs;
    };

    extern Report printOut;

} // namespace ocgdb

#endif /* OCGDB_REPORT_H */
