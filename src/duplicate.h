/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef DUPLICATE_H
#define DUPLICATE_H

#include "dbread.h"

namespace ocgdb {


class Duplicate : public DbRead
{
public:

    virtual void processAGame(const bslib::PgnRecord& record, const std::vector<int8_t>& moveVec) override;

private:
    virtual void runTask() override;
    virtual void printStats() const override;
    
    void printDuplicates(ThreadRecord* t, const bslib::PgnRecord& record, int theDupID, uint64_t);

    void threadCheckDupplication(const bslib::PgnRecord&, const std::vector<int8_t>& moveVec);

private:
    mutable std::mutex dupHashKeyMutex;
    std::unordered_map<int64_t, std::vector<int>> hashGameIDMap;

};

} // namespace ocdb

#endif /* DUPLICATE_H */
