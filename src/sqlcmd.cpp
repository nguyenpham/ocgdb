/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <filesystem>
#include <iostream>

#include "sqlcmd.h"
#include "report.h"

using namespace ocgdb;

/// a value of a row as text; BLOBs as their size (the move columns)
static std::string columnText(const SQLite::Column& c)
{
    switch (c.getType()) {
        case SQLITE_NULL:
            return "NULL";
        case SQLITE_INTEGER:
            return std::to_string(c.getInt64());
        case SQLITE_FLOAT: {
            auto s = std::to_string(c.getDouble());
            return s;
        }
        case SQLITE_BLOB:
            return "<blob " + std::to_string(c.getBytes()) + " bytes>";
        default:
            return c.getString();
    }
}

bool SqlCmd::runTask()
{
    auto dbPath = paraRecord.dbPaths.front();

    /// read-write when it can: the statements may change the database
    mDb = openDB(dbPath, SQLite::OPEN_READWRITE, paraRecord.password);
    if (!mDb) {
        mDb = openDB(dbPath, SQLite::OPEN_READONLY, paraRecord.password);
        if (!mDb) {
            return false;
        }
    }
    databasePath = dbPath;

    auto ok = true;
    auto changesBefore = sqlite3_total_changes(mDb->getHandle());
    for (auto&& sql : paraRecord.sqls) {
        ok = runStatement(sql) && ok;
    }

    /// the counts of the Info table follow what the statements changed
    if (sqlite3_total_changes(mDb->getHandle()) != changesBefore && hasTable(*mDb, "Info")) {
        try {
            refreshInfoCounts(*mDb);
        } catch (const std::exception&) {
        }
    }

    closeDb();
    return ok;
}

bool SqlCmd::runStatement(const std::string& sql)
{
    std::cout << "SQL: " << sql << std::endl;
    try {
        SQLite::Statement stmt(*mDb, sql);

        auto colCnt = stmt.getColumnCount();
        if (colCnt == 0) {
            auto n = stmt.exec();
            printOut.printOut("; " + std::to_string(n) + " rows changed");
            return true;
        }

        std::string header;
        for (int i = 0; i < colCnt; i++) {
            if (i) header += "|";
            header += stmt.getColumnName(i);
        }
        printOut.printOut(header);

        int64_t rowCnt = 0;
        while (stmt.executeStep()) {
            std::string row;
            for (int i = 0; i < colCnt; i++) {
                if (i) row += "|";
                row += columnText(stmt.getColumn(i));
            }
            printOut.printOut(row);
            rowCnt++;
            if (paraRecord.resultNumberLimit > 0 && rowCnt >= paraRecord.resultNumberLimit) {
                break;
            }
        }
        printOut.printOut("; " + std::to_string(rowCnt) + " rows");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DbInfo::runTask()
{
    auto ok = true;
    for (auto&& dbPath : paraRecord.dbPaths) {
        mDb = openDB(dbPath, SQLite::OPEN_READONLY, paraRecord.password);
        if (!mDb) {
            ok = false;
            continue;
        }
        databasePath = dbPath;

        std::error_code ec;
        auto sz = std::filesystem::file_size(dbPath, ec);

        std::string str = "Database: " + dbPath + "\nFile size: " + std::to_string(ec ? 0 : sz) + " bytes\n";

        if (hasTable(*mDb, "Info")) {
            str += "\nInfo table:\n";
            SQLite::Statement q(*mDb, "SELECT Name, Value FROM Info");
            while (q.executeStep()) {
                str += "  " + q.getColumn(0).getString() + ": " + columnText(q.getColumn(1)) + "\n";
            }
        }

        str += "\nTables (rows):\n";
        {
            std::vector<std::string> tables;
            SQLite::Statement q(*mDb, "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
            while (q.executeStep()) {
                tables.push_back(q.getColumn(0).getString());
            }
            for (auto&& t : tables) {
                SQLite::Statement c(*mDb, "SELECT COUNT(*) FROM \"" + t + "\"");
                auto n = c.executeStep() ? c.getColumn(0).getInt64() : 0;
                str += "  " + t + ": " + std::to_string(n) + "\n";
            }
        }

        if (hasTable(*mDb, "Games")) {
            str += "\nColumns of Games:\n";
            SQLite::Statement q(*mDb, "PRAGMA table_info(Games)");
            while (q.executeStep()) {
                str += "  " + q.getColumn(1).getString() + " " + q.getColumn(2).getString() + "\n";
            }

            gameFieldSet.clear();
            extractGameTableInfo();
            str += "\nMoves are read from: " + (searchField == SearchField::none ? std::string("(no move column)")
                                                                                 : searchFieldNames[static_cast<int>(searchField)])
                   + "\n";
        }

        str += "\nIndexes:\n";
        {
            SQLite::Statement q(*mDb, "SELECT name, tbl_name FROM sqlite_schema WHERE type = 'index' AND sql IS NOT NULL ORDER BY name");
            while (q.executeStep()) {
                str += "  " + q.getColumn(0).getString() + " (" + q.getColumn(1).getString() + ")\n";
            }
        }

        printOut.printOut(str);
        closeDb();
    }
    return ok;
}
