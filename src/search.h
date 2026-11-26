/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_SEARCH_H
#define OCGDB_SEARCH_H

#include "dbread.h"
#include "pgnread.h"
#include "parser.h"

namespace ocgdb {

    /// -q: searches databases and PGN files with PQL queries. A game matches when
    /// one of the positions of its main line does (with "then": when its positions match the
    /// parts in order). The search on the games of Banksia GUI's PQL tab (ui/db/sqlpqlwidget.cpp),
    /// on the console.
    /// -bench: five queries on a database, timed.
    class Search : public virtual DbRead, public virtual PGNRead
    {
    public:
        /// the queries -bench runs
        static std::vector<std::string> benchQueries();

        /// a query without its comments (// to the end of the line) and outer spaces
        static std::string trimQuery(std::string query);

    protected:
        virtual bool runTask() override;

        virtual void printStats(bool completed) const override;

    private:
        bool searchAQuery(const std::string& query);
        bool searchADatabase(const std::string& dbPath, const std::string& query);
        bool searchAPgnFile(const std::string& path, const std::string& query);

        virtual void processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                             const std::vector<int8_t>& moveVec) override;
        virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<const char*, char*>&,
                                               const char*) override;

        void setupCallback();
        void printResults(const std::string& source);

    private:
        Parser parser;
        banksia::ChessVariant parsedVariant = banksia::ChessVariant::none;
        std::string query;

        /// a new number for each search of a source: the thread-local "then" state of a game
        /// belongs to one search
        int searchSerial = 0;
        bool fromPgn = false;

        /// a game found: its ID (or its place in the PGN file), the ply and the FEN of the
        /// position that matched, and the game as PGN when it is to be printed
        struct Found {
            int64_t id;
            int ply;
            std::string fen, pgn;
        };
        mutable std::mutex foundMutex;
        std::vector<Found> foundVec;
        int64_t totalFound = 0;
    };

} // namespace ocgdb

#endif /* OCGDB_SEARCH_H */
