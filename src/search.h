/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "dbread.h"
#include "pgnread.h"
#include "parser.h"

namespace ocgdb {


class Search : public DbRead, public PGNRead
{
public:
    virtual ~Search();

public:
    
    void setupForBench(ParaRecord& paraRecord);

private:
    virtual void processAGameWithAThread(ThreadRecord* t, const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec) override;
    virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *) override;

    virtual bool openDB(const std::string& dbPath) override;
    virtual void closeDb() override;

    virtual void runTask() override;
    virtual void printStats() const override;

private:
    mutable std::mutex gameIDMutex;
    std::string query;
    
    Parser parser;
    QueryGameRecord* qgr = nullptr;

};

} // namespace ocdb

#endif /* SEARCH_H */
