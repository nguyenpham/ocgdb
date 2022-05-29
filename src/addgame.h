/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_ADDGAME_H
#define OCGDB_ADDGAME_H

#include "dbread.h"
#include "builder.h"

namespace ocgdb {

class AddGame;
class AddGameDbRead;

class AddGameDbRead : public Builder, public DbRead
{
public:
    AddGame* addGameInstance;
    virtual void processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec) override;

    void setOptionFlag(int);

private:
    virtual bool openDB(const std::string& dbPath) override;
    
};

class AddGame : public Builder
{
public:
    AddGame();
    
    ThreadRecord* getThreadRecordAndInit();
    void addAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec);

    bool createConvertingIDMaps(SQLite::Database* db);
    
private:
    virtual void runTask() override;

    virtual int getEventNameId(char* name) override;
    virtual int getSiteNameId(char* name) override;
    virtual int getPlayerNameId(char* name, int elo) override;
    IDInteger getNameId(const std::string& tableName, IDInteger& cnt, const std::string& name, int elo = -1);

    void addDb(const std::string& dbPath);
    virtual IDInteger getNewGameID() override;

    bool createConvertingIDMap(SQLite::Database* db, const std::string& tableName, IDInteger& cnt, std::map<IDInteger, IDInteger>& theMap);

private:
    AddGameDbRead dbRead;
    
    IDInteger newGameID = 0;
    bool createMode;
    
    std::map<IDInteger, IDInteger> playerConvertIDMap, eventConvertIDMap, siteConvertIDMap;
};

} // namespace ocdb

#endif /* OCGDB_ADDGAME_H */
