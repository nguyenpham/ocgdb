/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef EXPORTER_H
#define EXPORTER_H

#include "dbread.h"

namespace ocgdb {


class Exporter : public DbRead
{
public:
    virtual void processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec) override;

private:
    virtual bool openDB(const std::string& dbPath) override;
    virtual void runTask() override;
//    virtual void printStats() const override;
    
private:
    int flag;
    mutable std::mutex pgnOfsMutex;
    std::ofstream pgnOfs;
};

} // namespace ocdb

#endif /* EXPORTER_H */
