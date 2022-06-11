/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "builder.h"
#include "board/chess.h"


using namespace ocgdb;

const int EpdMinLen = 20; // for quick checking
const int EpdVecThresholdSaving = 1024;


void Builder::createDb_EPD()
{
    startTime = getNow();

//    dbType = ocgdb::DbType::epd;
//    variant = paraRecord.variant;

    epdBoard = bslib::Funcs::createBoard(chessVariant);

    // Prepare
    assert(!paraRecord.dbPaths.empty());
    auto dbPath = paraRecord.dbPaths.front();

    // remove old db file if existed
    std::remove(dbPath.c_str());

    // init
    if (!createDb_EPD(dbPath, paraRecord.desc)) {
        return;
    }

    errCnt = epdCnt = processedCnt = 0;
    transactionCnt = 0;
    for(auto && path : paraRecord.dbPaths) {

        auto s = path;
        bslib::Funcs::toLower(s);

        if (bslib::Funcs::endsWith(s, ".epd")) {
            processFile_EPD(path);
        } else if (bslib::Funcs::endsWith(s, ".pgn")) {
            processPgnFile(path);
        }

        saveToDb_EPD();

        if (transactionCnt > 0) {
            sendTransaction(false);
        }
        transactionCnt = 0;
    }

    delete epdBoard;
    epdBoard = nullptr;

    // completing
    updateInfoTable_EPD();

//    printStats(true);
}

void Builder::updateInfoTable_EPD()
{
    std::string str = "DELETE FROM Info WHERE Name='EPDCount'";
    mDb->exec(str);

    str = std::string("INSERT INTO Info (Name, Value) VALUES ('EPDCount', '" + std::to_string(epdCnt) + "')");
    mDb->exec(str);
}

bool Builder::createDb_EPD(const std::string& path, const std::string& description)
{
    assert(!path.empty());

    // Open a database file in create/write mode
    //        mDb = new QSqlDatabase(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);

    mDb = DbCore::openDB(path, false);
    if (!mDb) {
        return false;
    }

    mDb->exec("DROP TABLE IF EXISTS Info");
    mDb->exec("CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT)");
    mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Data Structure Version', '" + ocgdb::VersionDatabaseString + "')");

    // User Data version
    mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Version', '" + ocgdb::VersionUserDatabaseString + "')");
    mDb->exec("INSERT INTO Info (Name, Value) VALUES ('Variant', 'standard')");
    mDb->exec("INSERT INTO Info (Name, Value) VALUES ('License', 'free')");
    mDb->exec(std::string("INSERT INTO Info (Name, Value) VALUES ('Description', '") + description + "')");


    mDb->exec("DROP TABLE IF EXISTS EPD");
    mDb->exec("CREATE TABLE EPD (EID INTEGER PRIMARY KEY AUTOINCREMENT, EPD TEXT)");

    mDb->exec("PRAGMA journal_mode=OFF");

    std::cout << "SQLite database file '" << path << "' opened successfully\n";

    epdFieldList.push_back("EPD");
    epdInsertStatement = new SQLite::Statement(*mDb, "INSERT INTO EPD (EPD) VALUES(:EPD)");

    return true;
}


/// It is better to keep the order of the input
void Builder::processFile_EPD(const std::string& path)
{
    epdRecordVec.clear();

    std::ifstream input_file(path);
    if (!input_file.is_open()) {
        std::cerr << "Could not open the file '" << path << "'" << std::endl;
        return;
    }

    std::string line;
    while (getline(input_file, line)) {
      bslib::Funcs::trim(line);

        if (!line.empty()) {
          gameCnt++;
          processedCnt++;

          auto r = processALine_EPD(line);

          if (r.epdString.empty()) {
              continue;
          }

          if (paraRecord.optionFlag & ocgdb::dup_flag_remove) {
              epdBoard->newGame(r.epdString);
              if (epdVistedHashSet.find(epdBoard->key()) != epdVistedHashSet.end()) {
                  continue;
              }
              epdVistedHashSet.insert(epdBoard->key());
          }

          epdRecordVec.push_back(r);
          if (epdRecordVec.size() > EpdVecThresholdSaving) {
              saveToDb_EPD();
          }

//          if ((gameCnt & 0xfff) == 0) {
//              printStats(false);
//          }

      }
   }
    input_file.close();

   saveToDb_EPD();
}

static const std::set<std::string> epdOpcodeNumberSet {
    "acd", // analysis count depth
    "acn", // analysis count nodes
    "acs", // analysis count seconds
    "ce", // centipawn evaluation
    "dm", // direct mate fullmove count
    "fmvn", // fullmove number
    "hmvc", // halfmove clock
    "rc", // repetition count
};

void Builder::saveToDb_EPD()
{
    if (epdRecordVec.empty()) {
        return;
    }

    std::set<std::string> fieldSet;
    for(auto && s : epdFieldList) {
        fieldSet.insert(s);
    }

    auto sz = epdFieldList.size();

    for(auto && r : epdRecordVec) {
        for(auto && o : r.vec) {
            if (fieldSet.find(o.opcode) == fieldSet.end()) {
                fieldSet.insert(o.opcode);
                epdFieldList.push_back(o.opcode);
            }
        }
    }


    if (sz < epdFieldList.size()) {
        for(auto i = sz; i < epdFieldList.size(); i++) {
            auto fieldName = epdFieldList.at(i);

            auto s1 = "ALTER TABLE EPD ADD COLUMN " + fieldName + " ";

            auto s2 = "TEXT";
            if (epdOpcodeNumberSet.find(fieldName) != epdOpcodeNumberSet.end()) {
                s2 = "INTEGER";
            }

            auto str = s1 + s2;
            mDb->exec(str);
        }

        std::string str = "INSERT INTO EPD (", s2;

        for(auto && s : epdFieldList) {
            if (!s2.empty()) {
                str += ", ";
                s2 += ", ";
            }
            str += s;
            s2 += ":" + s;
        }

        str += ") VALUES(" + s2 + ")";

        epdInsertStatement = new SQLite::Statement(*mDb, str);
    }

    epdInsertStatement->reset();
    
    for(auto && r : epdRecordVec) {
        epdInsertStatement->bind(":EPD", r.epdString);
        std::set<std::string> useTagSet { "EPD"};
        for(auto && o : r.vec) {
            auto s = o.stringOperand + o.sanMove + o.numberOperand;
            assert(!s.empty());
            epdInsertStatement->bind(":" + o.opcode, s);
            useTagSet.insert(o.opcode);
        }

//        // null fields
//        if (useTagSet.size() < epdFieldList.size()) {
//            for(auto && s : epdFieldList) {
//                if (useTagSet.find(s) == useTagSet.end()) {
//                    epdInsertStatement->bind(":" + s, QVariant());
//                }
//            }
//        }

        if (epdInsertStatement->exec()) {
            epdCnt++;
        } else {
            std::cerr << "Error: " << epdInsertStatement->getErrorMsg() << std::endl;
            break;
        }

    }

    epdRecordVec.clear();
}

EPDRecord Builder::processALine_EPD(const std::string& str)
{
    EPDRecord epdRecord;
    size_t k = 0;
    for(int i = 0; i < 4; i++, k++) {
        k = str.find(' ', k);
        if (k <= 0) {
            return epdRecord;
        }
    }

    assert(k > 4);

    epdRecord.epdString = str.substr(0, k - 1);

    enum class State {
        none, opcode, operand, sanmove, string, number, err
    };

    EPDOperation r;

    auto state = State::none;
    for(auto i = k; i < str.length(); i++) {
        auto ch = str.at(i);

        if (ch == ';' && state != State::string) {
            if (r.isValid()) {
                epdRecord.vec.push_back(r);
            }
            r.reset();
            state = State::none;
            continue;
        }

        switch(state) {
        case State::none:
            if (std::isalpha(ch)) {
                state = State::opcode;
                r.opcode = ch;
                continue;
            }
            break;

        case State::opcode:
            if (std::isalnum(ch) || ch == '_') {
                r.opcode += ch;
            } else if (ch == ' ') {
                state = State::operand;
            } else {
                r.reset();
                state = State::err;
            }
            continue;

        case State::operand:
            if (std::isalpha(ch)) {
                state = State::sanmove;
                r.sanMove = ch;
            } else if (std::isdigit(ch) || ch == '+' || ch == '-') {
                state = State::number;
                r.numberOperand = ch;
            } else if (ch == '"') {
                state = State::string;
            } else if (ch != ' ') {
                state = State::err;
            }
            continue;

        case State::string:
            if (ch == '"') {
                if (r.isValid()) {
                    epdRecord.vec.push_back(r);
                }
                r.reset();
                state = State::none;
                break;
            }
            r.stringOperand += ch;
            continue;

        case State::number:
            if (std::isdigit(ch) || ch == '.') {
                r.numberOperand += ch;
            }
            continue;

        case State::sanmove:
            if (std::isalnum(ch) || ch == '+' || ch == '-' || ch == '#') { // O-O
                r.sanMove += ch;
                break;
            }
            break;

        case State::err:
            if (ch == ';') {
                state = State::none;
            }
            continue;

        default:
            assert(false);
            continue;
        }
    }

    if (r.isValid()) {
        epdRecord.vec.push_back(r);
    }

    if (epdRecord.epdString.size() < EpdMinLen) {
        epdRecord.epdString = "";
    }

    return epdRecord;
}


void Builder::processPGNGameWithAThread_EPD(ThreadRecord* t, const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(t);

    t->initForBoards(chessVariant);
    assert(t->board && t->board2);

    if (itemMap.empty()) {
        t->errCnt++;
        return;
    }

    {
        std::lock_guard<std::mutex> dolock(transactionMutex);
        if (transactionCnt > TransactionCommit) {
            sendTransaction(false);
            transactionCnt = 0;
        }
        if (transactionCnt == 0) {
            sendTransaction(true);
        }
        transactionCnt++;
    }

    auto whiteElo = 0, blackElo = 0;
    std::string fenString;

    for(auto && it : itemMap) {
        auto s = it.second;

        if (strcmp(it.first, "Variant") == 0) {
            auto v = bslib::Funcs::string2ChessVariant(it.second);
            if (v != chessVariant) {
                t->errCnt++;
                return;
            }
            continue;
        }
        if (strcmp(it.first, "FEN") == 0) {
            fenString = s;
            continue;
        }
        if (strcmp(it.first, "WhiteElo") == 0) {
            whiteElo = std::atoi(s);
            continue;
        }
        if (strcmp(it.first, "BlackElo") == 0) {
            blackElo = std::atoi(s);
            continue;
        }
        if (strcmp(it.first, "PlyCount") == 0) {
            if (paraRecord.limitLen && paraRecord.limitLen > std::atoi(s)) {
                return;
            }
            continue;
        }
    }

    if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
        return;
    }

    if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
        return;
    }

    t->board->newGame(fenString);

    int flag = bslib::BoardCore::ParseMoveListFlag_quick_check;

    bslib::PgnRecord record;
    record.moveText = moveText;
    record.gameID = -1;
    t->board->fromMoveList(&record, bslib::Notation::san, flag);

    auto plyCount = t->board->getHistListSize();

    if (paraRecord.limitLen > plyCount) {
        return;
    }

    std::vector<EPDRecord> v;

    t->board2->newGame(fenString);
    bslib::Hist hist;
    hist.comment = t->board->getFirstComment();
    auto r = process_addBoard_EPD(t->board2, &hist);
    if (!r.epdString.empty()) {
        v.push_back(r);
    }

    for(auto i = 0; i < plyCount; i++) {
        auto h = t->board->_getHistPointerAt(i);
        auto move = h->move;
        if (!t->board2->_quickCheckMake(move.from, move.dest, move.promotion, false)) {
            break;
        }
        auto r = process_addBoard_EPD(t->board2, h);
        if (!r.epdString.empty()) {
            v.push_back(r);
        }
    }

    {
        std::lock_guard<std::mutex> dolock(epdRecordVecMutex);
        for(auto && q : v) {
            epdRecordVec.push_back(q);
        }

        if (epdRecordVec.size() > EpdVecThresholdSaving) {
            saveToDb_EPD();
        }
    }

    gameCnt++;
    processedCnt++;
}


EPDRecord Builder::process_addBoard_EPD(const bslib::BoardCore* board, const bslib::Hist* hist)
{
    assert(board);
    EPDRecord r;

    if (paraRecord.optionFlag & ocgdb::dup_flag_remove) {
        std::lock_guard<std::mutex> dolock(epdVistedHashSetMutex);
        if (epdVistedHashSet.find(board->key()) != epdVistedHashSet.end()) {
            return r;
        }
        epdVistedHashSet.insert(board->key());
    }

    r.epdString = board->getEPD(true, false);

    // hmvc halfmove clock
    EPDOperation o0;
    o0.opcode = "hmvc";
    o0.numberOperand = std::to_string(board->quietCnt / 2);
    r.vec.push_back(o0);

    // fmvn fullmove number
    EPDOperation o1;
    o1.opcode = "fmvn";
    o1.numberOperand = std::to_string((board->getHistListSize() + 1) / 2);
    r.vec.push_back(o1);

    if (!hist->comment.empty()) {
        EPDOperation o;
        o.opcode = "c0";
        o.stringOperand = hist->comment;
        r.vec.push_back(o);
    }

    return r;
}
