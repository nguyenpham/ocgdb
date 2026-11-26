/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_DBREAD_H
#define OCGDB_DBREAD_H

#include <deque>
#include <functional>
#include <future>

#include "dbcore.h"

namespace ocgdb {

    /// Reads the games of a database. readGames() steps through a query on the calling thread
    /// and gives the games, in batches, to the threads of the pool (processAGameWithAThread).
    class DbRead : public virtual DbCore
    {
    public:
        DbRead();
        virtual ~DbRead();

        /// opens a database for reading (also for another task that needs its games: -merge)
        bool openToRead(const std::string& dbPath, const std::string& password);

        /// every game of the open database as PGN, in the order of their IDs, on this thread;
        /// stops when `f` returns false
        void forEachGameAsPgn(const std::function<bool(const std::string&)>& f);

        static SearchField getMoveField(SQLite::Database& db);

        /// the tags of a game from a row of fullGameQueryString
        static void extractHeader(SQLite::Statement& query, banksia::PgnRecord& record);

        /// Comments of games by game ID: (ply, text) in the order stored
        using CommentMap = std::unordered_map<int, std::vector<std::pair<int, std::string>>>;

        /// all comments of the database in one pass over the table: without an index on GameID
        /// (databases made before ocgdb 2 and some of Banksia GUI's have none) looking up a game's
        /// comments scans all of them -- 20 ms a game on a 416,575-comment database
        static CommentMap loadComments(SQLite::Database& db);

        /// a game, moves and comments, from a row of fullGameQueryString into the board; the
        /// comments from `comments` when given, otherwise with the statement queryComments
        static void queryForABoard(banksia::PgnRecord& record, SearchField searchField, SQLite::Statement& query,
                                   SQLite::Statement* queryComments, banksia::BoardCore* board,
                                   const CommentMap* comments = nullptr);

        /// the bytes of a BLOB column
        static void blobToMoveVec(const SQLite::Column& c, std::vector<int8_t>& moveVec);

    protected:
        virtual bool runTask() override { return false; }

        /// Reads the games the SQL query gives, on the pool. The query must have the columns ID
        /// (or GID), FEN, and the move column of the database (Moves2, Moves1 or Moves).
        bool readGames(const std::string& sqlString);

        /// "SELECT <columns> FROM Games g" with the joins and the condition of -where, if any
        std::string gamesQuery(bool withHeaders) const;

        virtual void processAGame(const banksia::PgnRecord& record, const std::vector<int8_t>& moveVec);
        virtual void processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                             const std::vector<int8_t>& moveVec);

        /// parses a game read by readGames() into the board (the main line only with mainLineOnly)
        void extractBoard(banksia::BoardCore* board, const banksia::PgnRecord& record,
                          const std::vector<int8_t>& moveVec, bool mainLineOnly);

    protected:
        /// called at every position of a game (PositionCallback); returning true stops the game
        banksia::PositionCallback checkToStop = nullptr;
        /// called with the board after a game has been read
        std::function<bool(const banksia::BoardCore*, const banksia::PgnRecord*)> boardCallback = nullptr;

        /// the board callback wants the tags of the games (printing them)
        bool wantHeaders = false;
        /// parse the bitboards of every position (PQL)
        bool wantBitboards = false;
        bool mainLineOnly = false;

    private:
        void flushGameBatch();
        void drainGameBatches();

        std::vector<std::pair<banksia::PgnRecord, std::vector<int8_t>>> gameBatch;
        std::deque<std::future<bool>> batchFutures;
    };

} // namespace ocgdb

#endif /* OCGDB_DBREAD_H */
