/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>

#include "dbread.h"
#include "report.h"

using namespace ocgdb;

DbRead::DbRead() { }

DbRead::~DbRead()
{
    working = false;
    drainGameBatches();
}

bool DbRead::openToRead(const std::string& dbPath, const std::string& password)
{
    paraRecord.password = password;
    return openOCGDB(dbPath, true);
}

SearchField DbRead::getMoveField(SQLite::Database& db)
{
    auto searchField = SearchField::none;

    SQLite::Statement stmt(db, "PRAGMA table_info(Games)");
    while (stmt.executeStep()) {
        std::string fieldName = stmt.getColumn(1).getString();

        if (fieldName == "Moves2") {
            searchField = SearchField::moves2;
            break;
        }
        if (fieldName == "Moves1") {
            if (searchField < SearchField::moves1) {
                searchField = SearchField::moves1;
            }
        } else if (fieldName == "Moves") {
            if (searchField < SearchField::moves) {
                searchField = SearchField::moves;
            }
        }
    }

    return searchField;
}

void DbRead::blobToMoveVec(const SQLite::Column& c, std::vector<int8_t>& moveVec)
{
    moveVec.clear();
    if (c.isNull()) return;
    auto n = c.getBytes();
    if (n <= 0) return;
    auto p = static_cast<const int8_t*>(c.getBlob());
    moveVec.assign(p, p + n);
}

void DbRead::extractHeader(SQLite::Statement& query, banksia::PgnRecord& record)
{
    for (int i = 0, cnt = query.getColumnCount(); i < cnt; ++i) {
        auto c = query.getColumn(i);
        std::string name = c.getName();
        if (name == "ID" || name == "GID") {
            record.gameID = c.getInt();
            continue;
        }
        if (name == "EventID") {
            record.eventID = c.getInt();
            continue;
        }
        if (name == "SiteID") {
            record.siteID = c.getInt();
            continue;
        }
        if (name == "WhiteID") {
            record.whiteID = c.getInt();
            continue;
        }
        if (name == "BlackID") {
            record.blackID = c.getInt();
            continue;
        }

        // Ignore Moves, Moves1, Moves2
        if (name == "Moves" || name == "Moves1" || name == "Moves2") {
            continue;
        }

        std::string str;
        switch (c.getType()) {
            case SQLITE_INTEGER:
                str = std::to_string(c.getInt64());
                break;
            case SQLITE_FLOAT:
                str = std::to_string(c.getDouble());
                break;
            case SQLITE3_TEXT:
                str = c.getString();
                break;
            default: // NULL, BLOB
                break;
        }

        if (name == "FEN") {
            record.fenText = str;
            continue;
        }
        if (name == "Result") {
            record.result = str;
        }

        if (name != "Event" && str.empty()) {
            continue;
        }
        record.tags[name] = str;
    }
}

DbRead::CommentMap DbRead::loadComments(SQLite::Database& db)
{
    CommentMap map;
    if (!hasTable(db, "Comments")) {
        return map;
    }
    SQLite::Statement q(db, "SELECT GameID, Ply, Comment FROM Comments");
    while (q.executeStep()) {
        auto c = q.getColumn(2);
        if (c.isNull()) continue;
        map[q.getColumn(0).getInt()].emplace_back(q.getColumn(1).getInt(), c.getString());
    }
    return map;
}

/// whether the table Comments has an index starting with GameID
static bool hasCommentIndex(SQLite::Database& db)
{
    SQLite::Statement q(db, "SELECT name FROM pragma_index_list('Comments')");
    while (q.executeStep()) {
        SQLite::Statement c(db, "SELECT name FROM pragma_index_info(?) WHERE seqno = 0");
        c.bind(1, q.getColumn(0).getString());
        if (c.executeStep() && c.getColumn(0).getString() == "GameID") {
            return true;
        }
    }
    return false;
}

void DbRead::queryForABoard(banksia::PgnRecord& record, SearchField searchField, SQLite::Statement& query,
                            SQLite::Statement* queryComments, banksia::BoardCore* board, const CommentMap* comments)
{
    assert(board);

    extractHeader(query, record);
    record.variant = board->variant;

    board->newGame(record.fenText);

    // Tables games may have 0-2 columns for moves
    if (searchField == SearchField::moves1 || searchField == SearchField::moves2) {
        std::string moveName = "Moves2";

        int flag = banksia::ParseMoveListFlag_create_san | banksia::ParseMoveListFlag_quick_check;

        if (searchField == SearchField::moves1) {
            flag |= banksia::ParseMoveListFlag_move_size_1_byte;
            moveName = "Moves1";
        }

        std::vector<int8_t> moveVec;
        blobToMoveVec(query.getColumn(moveName.c_str()), moveVec);

        if (!moveVec.empty()) {
            board->fromMoveList(record, moveVec, flag);
        }

        /// the comments: Ply is the index of the move in the history, the variations' moves
        /// included (-1 for the comment before the first move)
        auto setComment = [board](int ply, const std::string& comment) {
            if (comment.empty()) return;
            if (ply >= 0 && ply < board->getHistListSize()) {
                board->_getHistPointerAt(ply)->comment = comment;
            } else if (ply < 0) {
                board->setFirstComment(comment);
            }
        };
        if (comments) {
            auto it = comments->find(record.gameID);
            if (it != comments->end()) {
                for (auto&& [ply, comment] : it->second) {
                    setComment(ply, comment);
                }
            }
        } else if (queryComments) {
            queryComments->reset();
            queryComments->bind(1, record.gameID);
            while (queryComments->executeStep()) {
                auto cc = queryComments->getColumn(1);
                if (!cc.isNull()) {
                    setComment(queryComments->getColumn(0).getInt(), cc.getString());
                }
            }
        }
    } else if (searchField == SearchField::moves) {
        auto c = query.getColumn("Moves");
        record.moveString = c.isNull() ? "" : c.getString();

        /// the text has its comments and variations: kept
        int flag = banksia::ParseMoveListFlag_quick_check | banksia::ParseMoveListFlag_create_san;
        board->fromMoveList(record, banksia::Notation::san, banksia::ParseVariation::parse, banksia::moveFormat, flag);
        record.moveString.clear();
    }

    board->result.result = banksia::string2ResultType(record.result);
}

std::string DbRead::gamesQuery(bool withHeaders) const
{
    std::string sql;
    if (withHeaders || !paraRecord.where.empty()) {
        sql = fullGameQueryString;
    } else {
        sql = "SELECT g.ID, g.FEN, g.Result";
        if (gameFieldSet.count("PlyCount")) sql += ", g.PlyCount";
        if (gameFieldSet.count("WhiteElo")) sql += ", g.WhiteElo";
        if (gameFieldSet.count("BlackElo")) sql += ", g.BlackElo";
        if (searchField != SearchField::none) {
            sql += ", g." + searchFieldNames[static_cast<int>(searchField)];
        }
        sql += " FROM Games g";
    }

    std::string cond;
    if (!paraRecord.where.empty()) {
        cond = "(" + paraRecord.where + ")";
    }
    if (paraRecord.limitLen > 0 && gameFieldSet.count("PlyCount")) {
        if (!cond.empty()) cond += " AND ";
        cond += "g.PlyCount >= " + std::to_string(paraRecord.limitLen);
    }
    if (paraRecord.limitElo > 0 && gameFieldSet.count("WhiteElo") && gameFieldSet.count("BlackElo")) {
        if (!cond.empty()) cond += " AND ";
        cond += "g.WhiteElo >= " + std::to_string(paraRecord.limitElo) + " AND g.BlackElo >= "
                + std::to_string(paraRecord.limitElo);
    }
    if (!cond.empty()) {
        sql += " WHERE " + cond;
    }
    return sql;
}

bool DbRead::readGames(const std::string& sqlString)
{
    assert(mDb && pool);

    SQLite::Statement statement(*mDb, sqlString);

    int idIdx = -1, fenIdx = -1, resultIdx = -1, moveIdx = -1;
    for (int i = 0, n = statement.getColumnCount(); i < n; i++) {
        std::string name = statement.getColumnName(i);
        if ((name == "GID") || (name == "ID" && idIdx < 0)) {
            idIdx = i;
        } else if (name == "FEN") {
            fenIdx = i;
        } else if (name == "Result") {
            resultIdx = i;
        } else if (name == searchFieldNames[static_cast<int>(searchField)]) {
            moveIdx = i;
        }
    }

    if (idIdx < 0 || moveIdx < 0) {
        std::cerr << "Error: the query must give the columns ID and " << searchFieldNames[static_cast<int>(searchField)]
                  << std::endl;
        return false;
    }

    for (auto&& t : threadMap) {
        t.second.resetStats();
    }

    int64_t readCnt = 0;
    while (working && statement.executeStep()) {
        if (paraRecord.gameNumberLimit > 0 && readCnt >= paraRecord.gameNumberLimit) {
            break;
        }
        readCnt++;

        banksia::PgnRecord record;
        record.variant = variant;
        record.gameID = statement.getColumn(idIdx).getInt();

        if (fenIdx >= 0) {
            auto c = statement.getColumn(fenIdx);
            if (!c.isNull()) record.fenText = c.getString();
        }
        if (resultIdx >= 0) {
            auto c = statement.getColumn(resultIdx);
            if (!c.isNull()) record.result = c.getString();
        }

        if (wantHeaders) {
            extractHeader(statement, record);
        }

        std::vector<int8_t> moveVec;
        auto c = statement.getColumn(moveIdx);
        if (searchField == SearchField::moves) {
            record.moveString = c.isNull() ? "" : c.getString();
        } else {
            blobToMoveVec(c, moveVec);
        }

        gameBatch.emplace_back(std::move(record), std::move(moveVec));
        if (gameBatch.size() >= 64) {
            flushGameBatch();
        }

        if ((readCnt & 0xfff) == 0) {
            printProgress();
        }
    }

    drainGameBatches();
    pool->wait_for_tasks();
    return true;
}

void DbRead::flushGameBatch()
{
    if (gameBatch.empty() || !pool) {
        return;
    }
    auto batch = std::make_shared<std::vector<std::pair<banksia::PgnRecord, std::vector<int8_t>>>>(std::move(gameBatch));
    gameBatch.clear();
    batchFutures.push_back(pool->submit([this, batch]() {
        for (auto&& [record, moveVec] : *batch) {
            if (!working) break;
            processAGame(record, moveVec);
        }
    }));

    /// enough queued work to keep every thread busy while the reader reads, and no more
    const size_t maxInFlight = 4 * std::max<size_t>(1, pool->get_thread_count());
    while (batchFutures.size() > maxInFlight) {
        batchFutures.front().wait();
        batchFutures.pop_front();
    }
}

void DbRead::drainGameBatches()
{
    flushGameBatch();
    for (auto&& f : batchFutures) {
        f.wait();
    }
    batchFutures.clear();
}

void DbRead::processAGame(const banksia::PgnRecord& record, const std::vector<int8_t>& moveVec)
{
    if (working) {
        processedCnt++;
        auto t = getThreadRecord();
        assert(t);
        processAGameWithAThread(t, record, moveVec);
    }
}

void DbRead::extractBoard(banksia::BoardCore* board, const banksia::PgnRecord& record,
                          const std::vector<int8_t>& moveVec, bool mainLineOnly)
{
    board->newGame(record.fenText);

    int flag = banksia::ParseMoveListFlag_quick_check;
    if (wantBitboards) {
        flag |= banksia::ParseMoveListFlag_create_bitboard;
    }
    if (wantHeaders) {
        flag |= banksia::ParseMoveListFlag_create_san;
    }

    if (searchField == SearchField::moves) { // there is a text move only
        flag |= banksia::ParseMoveListFlag_discardComment;
        board->fromMoveList(record, banksia::Notation::san,
                            mainLineOnly ? banksia::ParseVariation::discard : banksia::ParseVariation::parse,
                            banksia::moveFormat, flag, checkToStop);
    } else {
        if (mainLineOnly) {
            flag |= banksia::ParseMoveListFlag_ignore_high_levels;
        }
        if (searchField == SearchField::moves1) {
            flag |= banksia::ParseMoveListFlag_move_size_1_byte;
        }
        board->fromMoveList(record, moveVec, flag, checkToStop);
    }

    board->result.result = banksia::string2ResultType(record.result);
}

void DbRead::processAGameWithAThread(ThreadRecord* t, const banksia::PgnRecord& record,
                                     const std::vector<int8_t>& moveVec)
{
    assert(t);

    t->initForBoards(variant);
    assert(t->board);

    extractBoard(t->board, record, moveVec, mainLineOnly);

    t->hdpLen += t->board->getHistListSize();
    t->gameCnt++;

    if (boardCallback) {
        boardCallback(t->board, &record);
    }
}

void DbRead::forEachGameAsPgn(const std::function<bool(const std::string&)>& f)
{
    assert(mDb);

    QueryGameRecord qgr(*mDb, searchField, fullGameQueryString, variant);

    /// the full rows, one pass: the comments are looked up per game (indexed by GameID)
    auto sql = fullGameQueryString;
    if (!paraRecord.where.empty()) {
        sql += " WHERE (" + paraRecord.where + ")";
    }
    sql += " ORDER BY g.ID";

    CommentMap comments;
    auto preload = hasTable(*mDb, "Comments") && !hasCommentIndex(*mDb);
    if (preload) {
        comments = loadComments(*mDb);
    }

    SQLite::Statement statement(*mDb, sql);
    auto board = createBoard(variant);

    while (working && statement.executeStep()) {
        banksia::PgnRecord record;
        queryForABoard(record, searchField, statement, qgr.queryComments, board, preload ? &comments : nullptr);
        if (!f(toPgnString(board, record))) {
            break;
        }
    }

    delete board;
}
