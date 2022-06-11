/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_DBCORE_H
#define OCGDB_DBCORE_H

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"

#include "core.h"

namespace ocgdb {


class DbCore : virtual public Core
{
protected:
    virtual void queryInfo();
    
    static void sendTransaction(SQLite::Database* db, bool begin);
    void sendTransaction(bool begin) {
        sendTransaction(mDb, begin);
    }
    static SQLite::Database* openDB(const std::string& dbPath, bool readonly);

protected:
    SearchField searchField;
    SQLite::Database* mDb = nullptr;
};

} // namespace ocdb

#endif /* OCGDB_DBCORE_H */
