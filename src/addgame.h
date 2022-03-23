/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef ADDGAME_H
#define ADDGAME_H

#include "dbread.h"
#include "builder.h"

namespace ocgdb {


class AddGame : public Builder
{
public:
    
private:
    virtual void runTask() override;
    
    IDInteger getNameId(const std::string& tableName, const std::string& name, int elo = -1);

    bool addGame(const std::string& dbPath, const std::string& pgnString);
    bool addGame(const std::string& dbPath, const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board);
    
    bool addGame(const std::unordered_map<std::string, std::string>& itemMap, const bslib::BoardCore* board);
};

} // namespace ocdb

#endif /* ADDGAME_H */
