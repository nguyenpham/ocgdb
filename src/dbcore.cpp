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

#include "dbcore.h"

using namespace ocgdb;

DbCore::~DbCore()
{
    closeDb();
}

std::string DbCore::sqlQuote(const std::string& s)
{
    std::string r = "'";
    for (auto ch : s) {
        if (ch == '\'') r += '\'';
        r += ch;
    }
    return r + "'";
}

namespace {
    /// The ways a database can be encrypted, in the order they are tried when one is opened -- the
    /// same list and order as Banksia GUI's DbCore::applyKey (ui/ocgdb/dbcore.cpp): SQLCipher 4
    /// first (what Banksia GUI and other tools write), then SQLCipher 3, then the other schemes
    /// SQLite3 Multiple Ciphers knows. https://utelle.github.io/SQLite3MultipleCiphers/docs/ciphers/
    struct CipherScheme {
        const char* cipher;
        int legacy; // SQLCipher's format version; 0: not set
    };
    const CipherScheme cipherSchemes[] = {
        { "sqlcipher", 4 }, { "sqlcipher", 3 }, { "chacha20", 0 }, { "aes256cbc", 0 }, { "aes128cbc", 0 },
        { "sqlcipher", 2 }, { "sqlcipher", 1 }, { "ascon128", 0 },   { "aegis", 0 },     { "rc4", 0 },
    };

    bool tryKey(SQLite::Database& db, const CipherScheme& scheme, const std::string& pass)
    {
        try {
            db.exec(std::string("PRAGMA cipher = '") + scheme.cipher + "'");
            if (scheme.legacy > 0) {
                db.exec("PRAGMA legacy = " + std::to_string(scheme.legacy));
            }
            db.exec("PRAGMA key = " + DbCore::sqlQuote(pass));
            /// reading the schema is what fails with the wrong key or cipher ("file is not a database")
            SQLite::Statement q(db, "SELECT count(*) FROM sqlite_schema");
            return q.executeStep();
        } catch (const std::exception&) {
            return false;
        }
    }

    bool isReadable(SQLite::Database& db)
    {
        try {
            SQLite::Statement q(db, "SELECT count(*) FROM sqlite_schema");
            return q.executeStep();
        } catch (const std::exception&) {
            return false;
        }
    }
} // namespace

SQLite::Database* DbCore::openDB(const std::string& dbPath, int openFlags, const std::string& password)
{
    auto isNew = dbPath == ":memory:" || !std::filesystem::exists(dbPath) || std::filesystem::file_size(dbPath) == 0;

    SQLite::Database* db = nullptr;
    try {
        db = new SQLite::Database(dbPath, openFlags);

        if (!password.empty()) {
            auto ok = false;
            if (isNew) {
                ok = tryKey(*db, cipherSchemes[0], password);
            } else {
                for (auto&& scheme : cipherSchemes) {
                    if (tryKey(*db, scheme, password)) {
                        ok = true;
                        break;
                    }
                    /// a wrong key cannot be taken back on the same connection
                    delete db;
                    db = new SQLite::Database(dbPath, openFlags);
                }
            }
            if (!ok) {
                std::cerr << "Error: wrong password, or not a database: " << dbPath << std::endl;
                delete db;
                return nullptr;
            }
        } else if (!isNew && !isReadable(*db)) {
            std::cerr << "Error: can't read " << dbPath
                      << " -- not a database, or an encrypted one (give its password with -password)" << std::endl;
            delete db;
            return nullptr;
        }

        // Read-side performance tuning, the same as Banksia GUI's
        db->exec("PRAGMA temp_store = MEMORY");   // sorts/temp b-trees in RAM
        db->exec("PRAGMA cache_size = -65536");   // 64 MB page cache
        db->exec("PRAGMA mmap_size = 268435456"); // up to 256 MB memory-mapped I/O
    } catch (const std::exception& e) {
        std::cerr << "Error: can't open database " << dbPath << ": " << e.what() << std::endl;
        delete db;
        return nullptr;
    }
    return db;
}

bool DbCore::hasTable(SQLite::Database& db, const std::string& tableName)
{
    try {
        return db.tableExists(tableName);
    } catch (const std::exception&) {
        return false;
    }
}

std::string DbCore::getInfo(SQLite::Database& db, const std::string& name)
{
    try {
        SQLite::Statement q(db, "SELECT Value FROM Info WHERE Name = ?");
        q.bind(1, name);
        if (q.executeStep()) {
            auto c = q.getColumn(0);
            return c.isNull() ? "" : c.getString();
        }
    } catch (const std::exception&) {
    }
    return "";
}

void DbCore::setInfo(SQLite::Database& db, const std::string& name, const std::string& value)
{
    SQLite::Statement q(db, "INSERT OR REPLACE INTO Info (Name, Value) VALUES (?, ?)");
    q.bind(1, name);
    q.bind(2, value);
    q.exec();
}

banksia::ChessVariant DbCore::readVariant(SQLite::Database& db)
{
    auto s = getInfo(db, "Variant");
    if (s.empty()) {
        return banksia::ChessVariant::none;
    }
    return banksia::string2ChessVariant(s);
}

void DbCore::refreshInfoCounts(SQLite::Database& db)
{
    static const std::vector<std::pair<std::string, std::string>> counted {
        { "Games", "GameCount" },       { "Players", "PlayerCount" },   { "Events", "EventCount" },
        { "Sites", "SiteCount" },       { "Comments", "CommentCount" },
    };
    if (!hasTable(db, "Info")) {
        return;
    }
    for (auto&& [table, name] : counted) {
        if (!hasTable(db, table)) {
            continue;
        }
        SQLite::Statement q(db, "SELECT COUNT(*) FROM " + table);
        if (q.executeStep()) {
            setInfo(db, name, std::to_string(q.getColumn(0).getInt64()));
        }
    }
}

bool DbCore::openOCGDB(const std::string& dbPath, bool readonly)
{
    closeDb();

    databasePath = dbPath;
    mDb = openDB(dbPath, readonly ? SQLite::OPEN_READONLY : SQLite::OPEN_READWRITE, paraRecord.password);
    if (!mDb) {
        return false;
    }

    if (!hasTable(*mDb, "Games")) {
        std::cerr << "Error: " << dbPath << " is not a game database (it has no table Games)" << std::endl;
        closeDb();
        return false;
    }

    queryInfoTable();
    extractGameTableInfo();

    if (!isVariantSupported(variant)) {
        std::cerr << "Error: " << dbPath << " is a database of " << banksia::chessVariant2String(variant)
#ifdef _CHESS_
                  << "; this program reads chess and chess960 databases (use ocgdb-xq for xiangqi and jieqi)"
#else
                  << "; this program reads xiangqi and jieqi databases (use ocgdb for chess)"
#endif
                  << std::endl;
        closeDb();
        return false;
    }
    return true;
}

void DbCore::closeDb()
{
    if (pool) {
        pool->wait_for_tasks();
    }

    /// the thread records hold statements on this connection
    threadMap.clear();

    if (mDb) {
        if (inTransaction) {
            try {
                mDb->exec("COMMIT");
            } catch (const std::exception&) {
            }
            inTransaction = false;
        }
        delete mDb;
        mDb = nullptr;
    }
}

void DbCore::queryInfoTable()
{
    gameCnt = 0;
    playerCnt = eventCnt = siteCnt = 0;
    commentCnt = 0;

    if (!mDb) return;

    auto v = readVariant(*mDb);
    /// a database without the row is of the program's own variant (the Beta versions wrote "standard")
    variant = v == banksia::ChessVariant::none ? defaultVariant() : v;

    auto count = [this](const std::string& infoName, const std::string& table) -> int64_t {
        auto s = getInfo(*mDb, infoName);
        if (!s.empty()) {
            try {
                return std::stoll(s);
            } catch (const std::exception&) {
            }
        }
        if (!hasTable(*mDb, table)) {
            return 0;
        }
        SQLite::Statement q(*mDb, "SELECT COUNT(*) FROM " + table);
        return q.executeStep() ? q.getColumn(0).getInt64() : 0;
    };

    gameCnt = count("GameCount", "Games");
    playerCnt = static_cast<IDInteger>(count("PlayerCount", "Players"));
    eventCnt = static_cast<IDInteger>(count("EventCount", "Events"));
    siteCnt = static_cast<IDInteger>(count("SiteCount", "Sites"));
    commentCnt = count("CommentCount", "Comments");
}

// setup searchField and fullGameQueryString
void DbCore::extractGameTableInfo()
{
    searchField = SearchField::none;
    gameFieldSet.clear();

    SQLite::Statement stmt(*mDb, "PRAGMA table_info(Games)");
    while (stmt.executeStep()) {
        std::string fieldName = stmt.getColumn(1).getString();
        gameFieldSet.insert(fieldName);

        if (fieldName == "Moves2") {
            searchField = SearchField::moves2;
        } else if (fieldName == "Moves1") {
            if (searchField < SearchField::moves1) {
                searchField = SearchField::moves1;
            }
        } else if (fieldName == "Moves") {
            if (searchField < SearchField::moves) {
                searchField = SearchField::moves;
            }
        }
    }

    /// LEFT JOIN: a game whose event, site or player is missing is still a game (databases made
    /// by other tools may leave the IDs NULL)
    std::string joinString;
    fullGameQueryString = "SELECT g.ID GID,";
    if (gameFieldSet.count("EventID")) {
        fullGameQueryString += " e.Name Event,";
        joinString += " LEFT JOIN Events e ON g.EventID = e.ID";
    }
    if (gameFieldSet.count("SiteID")) {
        fullGameQueryString += " s.Name Site,";
        joinString += " LEFT JOIN Sites s ON g.SiteID = s.ID";
    }
    if (gameFieldSet.count("WhiteID")) {
        fullGameQueryString += " w.Name White,";
        joinString += " LEFT JOIN Players w ON g.WhiteID = w.ID";
    }
    if (gameFieldSet.count("BlackID")) {
        fullGameQueryString += " b.Name Black,";
        joinString += " LEFT JOIN Players b ON g.BlackID = b.ID";
    }

    fullGameQueryString += " g.* FROM Games g" + joinString;
}

void DbCore::sendTransaction(bool begin)
{
    if (!mDb) return;
    if (begin == inTransaction) return;
    mDb->exec(begin ? "BEGIN" : "COMMIT");
    inTransaction = begin;
}

void DbCore::createNewTransaction(bool force)
{
    std::lock_guard<std::mutex> dolock(transactionMutex);
    if (!inTransaction) {
        sendTransaction(true);
        transactionCnt = 0;
    } else if (transactionCnt > TransactionCommit || force) {
        sendTransaction(false);
        sendTransaction(true);
        transactionCnt = 0;
    }
    transactionCnt++;
}

void DbCore::commitTransaction()
{
    std::lock_guard<std::mutex> dolock(transactionMutex);
    sendTransaction(false);
    transactionCnt = 0;
}
