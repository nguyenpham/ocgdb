/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_DBCORE_H
#define OCGDB_DBCORE_H

#include <set>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"

#include "core.h"

namespace ocgdb {

    /// A task that works on a database: the connection, the Info table, the columns of Games.
    class DbCore : virtual public Core
    {
    public:
        virtual ~DbCore();

        /// Opens a database. With a password, the connection is keyed: a new database is
        /// encrypted (SQLCipher 4's scheme, as Banksia GUI writes), an existing one is tried with
        /// each scheme SQLite3 Multiple Ciphers knows. Null, with a message, when it fails.
        static SQLite::Database* openDB(const std::string& dbPath, int openFlags, const std::string& password);

        /// the Variant row of the Info table; `none` if there is none
        static banksia::ChessVariant readVariant(SQLite::Database& db);

        /// counts the rows of the tables the Info table describes and stores them
        static void refreshInfoCounts(SQLite::Database& db);

        /// a value of the Info table, "" when there is none
        static std::string getInfo(SQLite::Database& db, const std::string& name);
        static void setInfo(SQLite::Database& db, const std::string& name, const std::string& value);

        static bool hasTable(SQLite::Database& db, const std::string& tableName);

        /// the string as an SQL string literal: 'it''s'
        static std::string sqlQuote(const std::string& s);

    protected:
        /// opens the database for reading or writing, reads its Info table and the columns of
        /// Games, and checks the variant against this program
        bool openOCGDB(const std::string& dbPath, bool readonly);
        virtual void closeDb();

        void queryInfoTable();
        void extractGameTableInfo();

        void sendTransaction(bool begin);
        /// begins a transaction, or commits the current one and begins another every
        /// TransactionCommit calls (or when forced)
        void createNewTransaction(bool force = false);
        void commitTransaction();

    protected:
        SQLite::Database* mDb = nullptr;
        std::string databasePath;

        SearchField searchField = SearchField::none;
        std::set<std::string> gameFieldSet;
        /// all columns of a game with the names of its event, site and players:
        /// "SELECT g.ID GID, e.Name Event, ..., g.* FROM Games g LEFT JOIN ..."
        std::string fullGameQueryString;

        mutable std::mutex transactionMutex;
        int TransactionCommit = 256 * 1024;
        int transactionCnt = 0;
        bool inTransaction = false;
    };

} // namespace ocgdb

#endif /* OCGDB_DBCORE_H */
