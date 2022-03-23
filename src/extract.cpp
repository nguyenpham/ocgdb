/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "extract.h"

using namespace ocgdb;

void Extract::runTask()
{
    std::cout   << "Extract games..." << std::endl;
    getGame(paraRecord);
}

void Extract::getGame(const ParaRecord& _paraRecord)
{
    paraRecord = _paraRecord;

    SQLite::Database db(paraRecord.dbPaths.front(), SQLite::OPEN_READONLY);
    auto searchField = DbRead::getMoveField(&db);

    printGamePGNByIDs(db, paraRecord.gameIDVec, searchField);
}
