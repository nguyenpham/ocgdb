/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_BUILDER_H
#define OCGDB_BUILDER_H

#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "dbcore.h"
#include "pgnread.h"

#ifdef _XIANGQI_
#include "wxfread.h"
#endif

namespace ocgdb {

    /// -create: a new database from PGN files (xiangqi: also WXF files).
    /// -merge: adds the games of PGN files and of other databases to an existing database.
    /// The logic of the inserts is Banksia GUI's (ui/ocgdb/builder.cpp), so both programs write
    /// the same databases.
    class Builder : virtual public DbCore,
#ifdef _XIANGQI_
                    virtual public WXFRead
#else
                    virtual public PGNRead
#endif
    {
    public:
        static int standardizeFEN(char* fenBuf);

    protected:
        virtual bool runTask() override;

        bool create();
        bool merge();

        /// the tables of a new database
        bool createDb(const std::string& path);
        /// an existing database, ready for adding games: its columns, names and largest IDs
        bool openForAppending(const std::string& path);

        void setupTagVec(const std::vector<std::string>& tagVec, int optionFlag);
        bool addNewField(const std::string& fieldName);
        void updateInfoTable();

        /// the games of another database, added as if they came from a PGN file
        void addGamesFromDb(const std::string& path);

        IDInteger getEventNameId(ThreadRecord*, const char* name);
        IDInteger getSiteNameId(ThreadRecord*, const char* name);
        IDInteger getPlayerNameId(ThreadRecord*, const char* name, int elo);
        IDInteger getNewGameID();

        virtual void printStats(bool completed) const override;

    private:
        virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<const char*, char*>&,
                                               const char*) override;

        IDInteger getNameId(const char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement,
                            std::unordered_map<std::string, IDInteger>& idMap);
        void loadNameMap(const std::string& tableName, std::unordered_map<std::string, IDInteger>& idMap,
                         IDInteger& maxId);

        /// the game in t->board is a copy of one already added (option nodup)
        bool isBoardDuplication(ThreadRecord* t, IDInteger& gameID);

        static std::string nameKey(const char* name);

    protected:
        std::vector<std::string> create_tagVec;
        std::map<std::string, int> create_tagMap;
        mutable std::mutex create_tagFieldMutex;

        mutable std::mutex gameMutex, eventMutex, siteMutex, playerMutex;

        /// The ID of a game is its place in its file after the IDs of the files before it, so a
        /// database is the same however its threads ran, and game N of a PGN file is the game
        /// with ID N (when nothing is skipped: a game skipped leaves its ID unused).
        /// fileBaseID: the largest ID before the current file; gameIDCnt: the largest ID given.
        IDInteger fileBaseID = 0;
        IDInteger gameIDCnt = 0;
        void startAFile();
        std::atomic<int64_t> addedCnt { 0 }, dupCnt { 0 };

    private:
        std::unordered_map<std::string, IDInteger> playerIdMap, eventIdMap, siteIdMap;

        mutable std::mutex dupHashKeyMutex;
        /// the keys of the games added (option nodup)
        std::unordered_set<uint64_t> gameKeySet;
    };

} // namespace ocgdb

#endif // OCGDB_BUILDER_H
