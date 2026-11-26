/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_DUPLICATE_H
#define OCGDB_DUPLICATE_H

#include "dbread.h"

namespace ocgdb {

    /// -dup: finds the games of a database that are copies of others (the same start and the
    /// same moves of the main line), with embededgames also those that are the start of a longer
    /// game; with remove, deletes them. From Banksia GUI's ui/ocgdb/duplicate.cpp.
    class Duplicate : virtual public DbRead
    {
    protected:
        virtual bool runTask() override;

        virtual void processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                             const std::vector<int8_t>& moveVec) override;

        virtual void printStats(bool completed) const override;

    private:
        bool checkADatabase(const std::string& dbPath);
        void reportRemove();

    private:
        int64_t delCnt = 0, dupPairCnt = 0;

        mutable std::mutex dupHashKeyMutex, dupVecMutex;
        std::unordered_map<uint64_t, std::vector<IDInteger>> hashGameIDMap;

        /// pairs of the same game, found in whatever order the threads ran
        std::vector<std::pair<IDInteger, IDInteger>> samePairVec;
        /// (the longer game, a game that is its start)
        std::vector<std::pair<IDInteger, IDInteger>> embededPairVec;
        std::string moveName;
    };

} // namespace ocgdb

#endif /* OCGDB_DUPLICATE_H */
