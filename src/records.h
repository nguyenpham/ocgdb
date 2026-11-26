/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_RECORDS_H
#define OCGDB_RECORDS_H

#include <vector>
#include <unordered_map>
#include <fstream>
#include <mutex>

#include "3rdparty/SQLiteCpp/SQLiteCpp.h"
#include "3rdparty/threadpool/thread_pool.hpp"

#include "base/types.h"
#include "base/base.h"


/// -debug: print more (the options, the SQL of the tasks)
extern bool debugMode;

namespace ocgdb {

    class QueryGameRecord;

    /// The program's version: the Makefile passes it (VERSION), so `ocgdb -version` and the
    /// database's Info table say the same as the build.
#ifdef APP_VERSION
    const std::string VersionString = APP_VERSION;
#else
    const std::string VersionString = "2.0.0";
#endif

    /// The structure of the databases. Banksia GUI writes the same (ui/ocgdb/records.h), so
    /// databases go from one program to the other.
    const std::string VersionDatabaseString = "0.6";

    /// The user's version of the data. Developers may change this string.
    const std::string VersionUserDatabaseString = "0.1";

    /// IDs of games, players, events, sites. Signed: -1 is "none".
    using IDInteger = int32_t;

    enum class Task {
        create,
        merge,
        export_,
        query,
        sql,
        getgame,
        dup,
        bench,
        info,
        none,
    };

    enum {
        TagIdx_GameID,
        TagIdx_Event,
        TagIdx_Site,
        TagIdx_Date,
        TagIdx_Round,
        TagIdx_White,
        TagIdx_WhiteElo,
        TagIdx_Black,
        TagIdx_BlackElo,
        TagIdx_Result,
        TagIdx_TimeControl,
        TagIdx_ECO,
        TagIdx_PlyCount,
        TagIdx_FEN,

        TagIdx_Max
    };

    /// same order and meaning as TagIdx_
    extern const std::vector<std::string> knownPgnTagVec;

    enum class SearchField {
        none,
        moves,  // text only
        moves1, // 1 byte per move, 2 for queen moves (xiangqi: rook, cannon)
        moves2, // 2 bytes per move
    };

    extern const std::vector<std::string> searchFieldNames; // "", "Moves", "Moves1", "Moves2"

    enum {
        create_flag_moves = 1 << 0,
        create_flag_moves1 = 1 << 1,
        create_flag_moves2 = 1 << 2,
        create_flag_accept_new_tags = 1 << 3,
        create_flag_discard_comments = 1 << 4,
        create_flag_discard_sites = 1 << 5,
        create_flag_discard_no_elo = 1 << 6,
        create_flag_discard_fen = 1 << 7,
        create_flag_discard_error_games = 1 << 8,
        create_flag_reset_eco = 1 << 9,
        create_flag_discard_variations = 1 << 10,

        query_flag_print_all = 1 << 12,
        query_flag_print_fen = 1 << 13,
        query_flag_print_pgn = 1 << 14,

        dup_flag_remove = 1 << 15,
        dup_flag_embededgames = 1 << 16,

        lichess_flag_nobot = 1 << 20,
        lichess_flag_bot = 1 << 21,
        lichess_keep_site = 1 << 22,

        create_flag_no_dup = 1 << 23,
    };

    /// Everything the command line says (main.cpp).
    class ParaRecord
    {
    public:
        std::vector<std::string> pgnPaths, dbPaths, queries, sqls;
        std::string reportPath, desc, password;
        /// an SQL condition on the table Games (alias g): which games -export, -q, -dup read
        std::string where;

        int optionFlag = 0;

        Task task = Task::none;
        banksia::ChessVariant variant = banksia::ChessVariant::none;
        int cpuNumber = -1, limitElo = 0, limitLen = 0;
        std::vector<int> gameIDVec;

        int64_t gameNumberLimit = 0;   // stop after this many games (0: all)
        int64_t resultNumberLimit = 0; // stop querying after this many results (0: all)

        mutable std::string errorString;

        std::string getErrorString() const { return errorString; }

        static std::string toString(Task task);
        std::string toString() const;
        bool isValid() const;

        bool setupOptions(const std::string& optionString);
    };


    //////////////////////////
    /// What a thread of the pool keeps: its boards and its prepared statements. A statement is
    /// used by the thread that prepared it only; the connection itself is shared (SQLite is
    /// compiled serialized, SQLITE_THREADSAFE=1).
    class ThreadRecord
    {
    public:
        ~ThreadRecord();

        bool initForBoards(banksia::ChessVariant variant);
        void initForInserting(SQLite::Database* db, banksia::ChessVariant variant);
        bool createInsertGameStatement(SQLite::Database* db, const std::vector<std::string>& fieldVec);
        void initGetGameStatement(SQLite::Database* db, banksia::ChessVariant variant, const std::string& moveName);

        void deleteAllStatements();
        void resetStats();

        /// Compare move by move the game in board with the game checkingGameID of the database;
        /// with `embeded`, also true when one game is the start of the other
        bool isBoardDuplication(IDInteger checkingGameID, bool embeded = false);

    public:
        int64_t errCnt = 0, gameCnt = 0, hdpLen = 0, dupCnt = 0, delCnt = 0;
        size_t insertGameStatementIdxSz = 0;

        banksia::BoardCore *board = nullptr, *board2 = nullptr;
        std::vector<uint8_t> buf;

        /// the columns of insertGameStatement, in order
        std::vector<std::string> usedFieldList;

        SQLite::Statement* insertGameStatement = nullptr;
        SQLite::Statement* insertCommentStatement = nullptr;
        SQLite::Statement *playerInsertStatement = nullptr, *eventInsertStatement = nullptr,
                          *siteInsertStatement = nullptr;
        SQLite::Statement* getGameByIdStatement = nullptr;
        SQLite::Statement* removeGameByIdStatement = nullptr;

        QueryGameRecord* qgr = nullptr;

    private:
        std::string moveName;
    };


    /// Reads whole games (tags, moves, comments) by their IDs, for printing them as PGN.
    class QueryGameRecord
    {
    public:
        QueryGameRecord(SQLite::Database& db, SearchField searchField, const std::string& fullGameQueryString,
                        banksia::ChessVariant variant);
        ~QueryGameRecord();

        /// the game as PGN, or "" when there is no such game
        std::string queryAndCreatePGNByGameID(banksia::PgnRecord& record);

    public:
        SQLite::Statement *queryGameByID = nullptr, *queryComments = nullptr;
        banksia::BoardCore* board = nullptr;
        SearchField searchField;
        banksia::ChessVariant variant;

    private:
        std::mutex queryMutex;
    };


    /// A new board of the variant, or null if this program does not play it (the chess program
    /// plays standard chess and chess960, the xiangqi one xiangqi and jieqi).
    banksia::BoardCore* createBoard(banksia::ChessVariant variant);

    /// The variant this program plays when nothing else says: standard chess or xiangqi.
    banksia::ChessVariant defaultVariant();

    /// Whether this program can play the variant.
    bool isVariantSupported(banksia::ChessVariant variant);

    /// A game as PGN: the Seven Tag Roster first in its order (Event, Site, Date, Round, White,
    /// Black, Result), then the other tags by name, then the moves with variations and comments.
    std::string toPgnString(const banksia::BoardCore* board, const banksia::PgnRecord& record);

    /// "2022.02.21", "2022-02-21" -> "2022-02-21" (the database form, ISO 8601); each "?" becomes
    /// "1" ("1950.??.??" -> "1950-11-11", as Banksia GUI does); "" when
    /// it is not a date. Two-digit years: 31-99 are 1931-1999, 0-30 2000-2030.
    std::string validDateString(std::string str);

    /// "2022-02-21" -> "2022.02.21" (the PGN form)
    std::string date2PgnString(std::string str);

} // namespace ocgdb

#endif /* OCGDB_RECORDS_H */
