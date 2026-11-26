/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "records.h"
#include "dbread.h"

#ifdef _CHESS_
#include "chess/chess.h"
#include "chess/chess960.h"
#endif

#ifdef _XIANGQI_
#include "xq/xq.h"
#include "xq/jieqiboard.h"
#endif

bool debugMode = false;

using namespace ocgdb;

// same order and meaning as TagIdx_
const std::vector<std::string> ocgdb::knownPgnTagVec = { "ID",    "Event",    "Site",   "Date",        "Round",
                                                         "White", "WhiteElo", "Black",  "BlackElo",    "Result",
                                                         "TimeControl", "ECO", "PlyCount", "FEN" };

const std::vector<std::string> ocgdb::searchFieldNames = { "", "Moves", "Moves1", "Moves2" };


////////////////////////////////////////////////////////////////////////
banksia::BoardCore* ocgdb::createBoard(banksia::ChessVariant variant)
{
    switch (variant) {
#ifdef _CHESS_
        case banksia::ChessVariant::standard:
            return new banksia::ChessBoard();
        case banksia::ChessVariant::chess960:
            return new banksia::ChessBoard960();
#endif
#ifdef _XIANGQI_
        case banksia::ChessVariant::xq:
            return new banksia::XqBoard();
        case banksia::ChessVariant::jieqi:
            return new banksia::JieqiBoard();
#endif
        default:
            break;
    }
    return nullptr;
}

banksia::ChessVariant ocgdb::defaultVariant()
{
#ifdef _CHESS_
    return banksia::ChessVariant::standard;
#else
    return banksia::ChessVariant::xq;
#endif
}

bool ocgdb::isVariantSupported(banksia::ChessVariant variant)
{
#ifdef _CHESS_
    return variant == banksia::ChessVariant::standard || variant == banksia::ChessVariant::chess960;
#else
    return variant == banksia::ChessVariant::xq || variant == banksia::ChessVariant::jieqi;
#endif
}

std::string ocgdb::validDateString(std::string str)
{
    for (auto&& ch : str) {
        if (ch == '.' || ch == '/') {
            ch = '-';
        } else if (ch == '?') {
            ch = '1';
        }
    }

    auto ss = banksia::splitString(str, '-');
    if (ss.size() != 3) {
        return "";
    }

    int year, month, date;
    try {
        size_t i0, i1, i2;
        year = std::stoi(ss[0], &i0);
        month = std::stoi(ss[1], &i1);
        date = std::stoi(ss[2], &i2);
        if (i0 != ss[0].size() || i1 != ss[1].size() || i2 != ss[2].size()) {
            return "";
        }
    } catch (const std::exception&) {
        return "";
    }

    if (year < 0 || (year >= 100 && year < 1000) || month <= 0 || month > 12 || date < 0 || date > 31) {
        return "";
    }

    if (year < 100) {
        year += year > 30 ? 1900 : 2000;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, date);
    return buf;
}

std::string ocgdb::date2PgnString(std::string str)
{
    if (str.size() == 10 && str[4] == '-' && str[7] == '-') {
        str[4] = str[7] = '.';
    }
    return str;
}

/// PGN tag values are in quotes: a quote or a backslash inside is escaped (PGN standard 8.1.1)
static std::string pgnTagValue(const std::string& s)
{
    std::string r;
    r.reserve(s.size());
    for (auto ch : s) {
        if (ch == '"' || ch == '\\') {
            r += '\\';
        }
        if (ch == '\n' || ch == '\r') {
            ch = ' ';
        }
        r += ch;
    }
    return r;
}

std::string ocgdb::toPgnString(const banksia::BoardCore* board, const banksia::PgnRecord& record)
{
    static const std::vector<std::string> strTags { "Event", "Site", "Date", "Round", "White", "Black", "Result" };
    /// not tags of the game: the database's own columns
    static const std::set<std::string> skipTags { "ID", "GID", "EventID", "SiteID", "WhiteID", "BlackID",
                                                  "Moves", "Moves1", "Moves2", "SetUp", "Variant" };

    std::map<std::string, std::string> tags;
    for (auto&& it : record.tags) {
        if (skipTags.find(it.first) == skipTags.end()) {
            tags[it.first] = it.second;
        }
    }

    auto it = tags.find("Date");
    if (it != tags.end()) {
        it->second = date2PgnString(it->second);
    }

    std::string resultString = "*";
    it = tags.find("Result");
    if (it != tags.end() && !it->second.empty()) {
        resultString = it->second;
    }
    tags["Result"] = resultString;

    std::ostringstream ss;
    for (auto&& name : strTags) {
        auto it = tags.find(name);
        ss << "[" << name << " \"" << (it != tags.end() ? pgnTagValue(it->second) : (name == "Result" ? "*" : "?"))
           << "\"]\n";
    }
    for (auto&& [name, value] : tags) {
        if (std::find(strTags.begin(), strTags.end(), name) != strTags.end() || name == "FEN") {
            continue;
        }
        ss << "[" << name << " \"" << pgnTagValue(value) << "\"]\n";
    }

    if (board) {
        if (board->variant == banksia::ChessVariant::chess960 || board->variant == banksia::ChessVariant::jieqi) {
            ss << "[Variant \"" << banksia::chessVariant2String(board->variant) << "\"]\n";
        }
        if (!board->startFen.empty()) {
            ss << "[FEN \"" << board->startFen << "\"]\n[SetUp \"1\"]\n";
        }
    }

    ss << "\n";

    if (board) {
        if (!board->getFirstComment().empty()) {
            auto c = board->getFirstComment();
            std::replace(c.begin(), c.end(), '{', '(');
            std::replace(c.begin(), c.end(), '}', ')');
            ss << "{" << c << "}\n";
        }
        auto fullMoveCnt = std::max(1, board->getFullMoveCnt());
        ss << board->toMoveListString(banksia::Notation::san, banksia::moveFormat, 8, fullMoveCnt);
    } else if (!record.moveString.empty()) {
        ss << record.moveString;
    } else if (record.moveText) {
        ss << record.moveText;
    }
    ss << " " << resultString << "\n";
    return ss.str();
}


////////////////////////////////////////////////////////////////////////
/// option name -> bit of ParaRecord::optionFlag
static const std::map<std::string, int> optionNameMap = {
    // creating
    { "moves", create_flag_moves },
    { "moves1", create_flag_moves1 },
    { "moves2", create_flag_moves2 },
    { "acceptnewtags", create_flag_accept_new_tags },
    { "discardcomments", create_flag_discard_comments },
    { "discardsites", create_flag_discard_sites },
    { "discardnoelo", create_flag_discard_no_elo },
    { "discardfen", create_flag_discard_fen },
    { "discarderr", create_flag_discard_error_games },
    { "discardvariations", create_flag_discard_variations },
    { "reseteco", create_flag_reset_eco },
    { "nodup", create_flag_no_dup },

    // query
    { "printall", query_flag_print_all },
    { "printfen", query_flag_print_fen },
    { "printpgn", query_flag_print_pgn },

    // duplicates
    { "remove", dup_flag_remove },
    { "embededgames", dup_flag_embededgames },

    // Lichess
    { "nobot", lichess_flag_nobot },
    { "bot", lichess_flag_bot },
    { "keepsite", lichess_keep_site },
};

std::string ParaRecord::toString(Task task)
{
    static const std::vector<std::string> taskNames { "create", "merge", "export", "query", "sql",
                                                      "get game", "duplicate", "bench", "info", "none" };
    return taskNames[static_cast<int>(task)];
}

bool ParaRecord::isValid() const
{
    errorString.clear();

    switch (task) {
        case Task::none:
            errorString = "missing a task such as -create, -merge, -export, -q, -sql, -g, -dup, -info, -bench";
            return false;

        case Task::create:
            if (dbPaths.size() != 1 || pgnPaths.empty()) {
                errorString = "-create needs one database path (-db) and at least one PGN path (-pgn)";
                return false;
            }
            return true;

        case Task::merge:
            if (dbPaths.empty() || (pgnPaths.empty() && dbPaths.size() < 2)) {
                errorString = "-merge needs a database to add to (the first -db) and PGN files (-pgn) or "
                              "other databases (more -db)";
                return false;
            }
            return true;

        case Task::export_:
            if (dbPaths.size() != 1 || pgnPaths.size() != 1) {
                errorString = "-export needs one database path (-db) and one PGN path (-pgn)";
                return false;
            }
            return true;

        case Task::query:
            if (dbPaths.empty() && pgnPaths.empty()) {
                errorString = "-q needs databases (-db) or PGN files (-pgn) to search";
                return false;
            }
            if (queries.empty()) {
                errorString = "-q needs a query";
                return false;
            }
            return true;

        case Task::sql:
            if (dbPaths.size() != 1 || sqls.empty()) {
                errorString = "-sql needs one database path (-db) and an SQL statement";
                return false;
            }
            return true;

        case Task::getgame:
            if (dbPaths.size() != 1 || gameIDVec.empty()) {
                errorString = "-g needs one database path (-db) and game IDs greater than zero";
                return false;
            }
            for (auto id : gameIDVec) {
                if (id <= 0) {
                    errorString = "-g: game IDs start from 1";
                    return false;
                }
            }
            return true;

        case Task::dup:
        case Task::bench:
        case Task::info:
            if (dbPaths.empty()) {
                errorString = "-" + toString(task) + " needs a database path (-db)";
                return false;
            }
            return true;

        default:
            break;
    }
    return false;
}

std::string ParaRecord::toString() const
{
    std::ostringstream s;

    s << "\tTask: " << toString(task) << "\n";
    s << "\tVariant: " << (variant == banksia::ChessVariant::none ? "(default)" : banksia::chessVariant2String(variant))
      << "\n";

    s << "\tPGN paths:\n";
    for (auto&& path : pgnPaths) {
        s << "\t\t" << path << "\n";
    }

    s << "\tDatabase paths:\n";
    for (auto&& path : dbPaths) {
        s << "\t\t" << path << "\n";
    }

    s << "\tQueries:\n";
    for (auto&& query : queries) {
        s << "\t\t" << query << "\n";
    }

    s << "\tSQL:\n";
    for (auto&& sql : sqls) {
        s << "\t\t" << sql << "\n";
    }

    s << "\tWhere: " << where << "\n";

    s << "\tGame IDs:";
    for (auto&& numb : gameIDVec) {
        s << " " << numb;
    }
    s << "\n";

    s << "\tReport path: " << reportPath << "\n";
    s << "\tPassword: " << (password.empty() ? "(none)" : "(given)") << "\n";

    s << "\tOptions:";
    for (auto&& it : optionNameMap) {
        if (optionFlag & it.second) {
            s << " " << it.first;
        }
    }
    s << "\n";

    s << "\tcpu: " << cpuNumber << ", min Elo: " << limitElo << ", min ply count: " << limitLen
      << ", game limit: " << gameNumberLimit << ", result limit: " << resultNumberLimit << "\n";

    return s.str();
}

bool ParaRecord::setupOptions(const std::string& optionString)
{
    auto ok = true;
    for (auto s : banksia::splitString(optionString, ',')) {
        banksia::trim(s);
        banksia::toLower(s);
        if (s.empty()) continue;

        auto it = optionNameMap.find(s);
        if (it == optionNameMap.end()) {
            std::cerr << "Error: unknown option: " << s << std::endl;
            ok = false;
            continue;
        }

        optionFlag |= it->second;
        if (s == "printpgn" || s == "printfen") {
            optionFlag |= query_flag_print_all;
        }
    }
    return ok;
}


////////////////////////////////////////////////////////////////////////
ThreadRecord::~ThreadRecord()
{
    deleteAllStatements();

    delete board;
    delete board2;
    board = board2 = nullptr;
}

void ThreadRecord::deleteAllStatements()
{
    delete insertGameStatement;
    delete insertCommentStatement;
    delete playerInsertStatement;
    delete eventInsertStatement;
    delete siteInsertStatement;
    delete getGameByIdStatement;
    delete removeGameByIdStatement;
    delete qgr;

    insertGameStatement = nullptr;
    insertCommentStatement = nullptr;
    playerInsertStatement = eventInsertStatement = siteInsertStatement = nullptr;
    getGameByIdStatement = nullptr;
    removeGameByIdStatement = nullptr;
    qgr = nullptr;
    insertGameStatementIdxSz = 0;
}

void ThreadRecord::resetStats()
{
    errCnt = gameCnt = hdpLen = dupCnt = delCnt = 0;
}

bool ThreadRecord::initForBoards(banksia::ChessVariant variant)
{
    if (board && board->variant == variant) {
        return false;
    }

    delete board;
    delete board2;

    board = createBoard(variant);
    board2 = createBoard(variant);
    assert(board && board2);
    return true;
}

void ThreadRecord::initForInserting(SQLite::Database* db, banksia::ChessVariant variant)
{
    initForBoards(variant);
    if (playerInsertStatement) {
        return;
    }

    assert(db);
    playerInsertStatement = new SQLite::Statement(*db, "INSERT INTO Players (ID, Name, Elo) VALUES (?, ?, ?)");
    eventInsertStatement = new SQLite::Statement(*db, "INSERT INTO Events (ID, Name) VALUES (?, ?)");
    siteInsertStatement = new SQLite::Statement(*db, "INSERT INTO Sites (ID, Name) VALUES (?, ?)");
    insertCommentStatement = new SQLite::Statement(*db, "INSERT INTO Comments (GameID, Ply, Comment) VALUES (?, ?, ?)");
}

bool ThreadRecord::createInsertGameStatement(SQLite::Database* db, const std::vector<std::string>& fieldVec)
{
    delete insertGameStatement;
    insertGameStatement = nullptr;

    std::string sql0 = "INSERT INTO Games (", sql1 = ") VALUES (";

    usedFieldList.clear();
    for (auto str : fieldVec) {
        // Special fields, using ID
        if (str == "Event" || str == "Site" || str == "White" || str == "Black") {
            str += "ID";
        }
        if (!usedFieldList.empty()) {
            sql0 += ",";
            sql1 += ",";
        }
        usedFieldList.push_back(str);
        sql0 += str;
        sql1 += "?";
    }

    try {
        insertGameStatement = new SQLite::Statement(*db, sql0 + sql1 + ")");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
    insertGameStatementIdxSz = fieldVec.size();
    return true;
}

void ThreadRecord::initGetGameStatement(SQLite::Database* db, banksia::ChessVariant variant,
                                        const std::string& _moveName)
{
    initForBoards(variant);
    if (getGameByIdStatement && moveName == _moveName) {
        return;
    }

    assert(db && !_moveName.empty());
    moveName = _moveName;

    delete getGameByIdStatement;
    delete removeGameByIdStatement;
    getGameByIdStatement = new SQLite::Statement(*db, "SELECT FEN, " + moveName + " FROM Games WHERE ID = ?");
    removeGameByIdStatement = new SQLite::Statement(*db, "DELETE FROM Games WHERE ID = ?");
}

/// Compare move by move with the game loaded already in board
bool ThreadRecord::isBoardDuplication(IDInteger checkingGameID, bool embeded)
{
    assert(board && board2 && getGameByIdStatement);

    std::string fenText;
    std::vector<int8_t> moveVec;
    std::string moveText;
    {
        auto& stmt = *getGameByIdStatement;
        stmt.reset();
        stmt.bind(1, checkingGameID);

        /// that game may have been deleted by another thread
        if (!stmt.executeStep()) {
            return false;
        }

        auto c0 = stmt.getColumn(0);
        fenText = c0.isNull() ? "" : c0.getString();

        auto c1 = stmt.getColumn(1);
        if (moveName == "Moves") {
            moveText = c1.isNull() ? "" : c1.getString();
        } else if (!c1.isNull()) {
            auto p = static_cast<const int8_t*>(c1.getBlob());
            moveVec.assign(p, p + c1.getBytes());
        }
    }

    /// compared after setting it up: a FEN of the start position is stored as no FEN
    board2->newGame(fenText);
    if (board->startFen != board2->startFen) {
        return false;
    }

    banksia::PgnRecord record;
    record.gameID = checkingGameID;
    int flag = banksia::ParseMoveListFlag_quick_check | banksia::ParseMoveListFlag_ignore_high_levels;
    if (moveName == "Moves") {
        record.moveString = moveText;
        board2->fromMoveList(record, banksia::Notation::san, banksia::ParseVariation::discard,
                             banksia::ParseMoveListFlag_quick_check | banksia::ParseMoveListFlag_discardComment);
    } else {
        if (moveName == "Moves1") {
            flag |= banksia::ParseMoveListFlag_move_size_1_byte;
        }
        board2->fromMoveList(record, moveVec, flag);
    }

    return board->equalMoveLists(board2, embeded);
}


//////////////////////////////////
QueryGameRecord::QueryGameRecord(SQLite::Database& db, SearchField searchField, const std::string& fullGameQueryString,
                                 banksia::ChessVariant variant)
    : searchField(searchField), variant(variant)
{
    assert(!fullGameQueryString.empty());
    queryGameByID = new SQLite::Statement(db, fullGameQueryString + " WHERE g.ID = ?");

    try {
        queryComments = new SQLite::Statement(db, "SELECT Ply, Comment FROM Comments WHERE GameID = ?");
    } catch (const std::exception&) {
        queryComments = nullptr; // a database without the table Comments
    }
}

QueryGameRecord::~QueryGameRecord()
{
    delete queryGameByID;
    delete queryComments;
    delete board;
}

std::string QueryGameRecord::queryAndCreatePGNByGameID(banksia::PgnRecord& record)
{
    std::lock_guard<std::mutex> dolock(queryMutex);

    queryGameByID->reset();
    queryGameByID->bind(1, record.gameID);

    if (!queryGameByID->executeStep()) {
        return "";
    }

    if (!board) {
        board = createBoard(variant);
    }

    DbRead::queryForABoard(record, searchField, *queryGameByID, queryComments, board);
    return toPgnString(board, record);
}
