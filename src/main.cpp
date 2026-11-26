/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <iostream>
#include <memory>

#include "records.h"
#include "report.h"
#include "builder.h"
#include "duplicate.h"
#include "exporter.h"
#include "search.h"
#include "sqlcmd.h"

#ifdef _CHESS_
#include "chess/chess.h"
#endif

static void print_usage();

#ifdef _CHESS_
static const char* programName = "ocgdb";
#else
static const char* programName = "ocgdb-xq";
#endif

static std::unique_ptr<ocgdb::Core> createTask(ocgdb::Task task)
{
    switch (task) {
        case ocgdb::Task::create:
        case ocgdb::Task::merge:
            return std::make_unique<ocgdb::Builder>();
        case ocgdb::Task::export_:
            return std::make_unique<ocgdb::Exporter>();
        case ocgdb::Task::getgame:
            return std::make_unique<ocgdb::Extract>();
        case ocgdb::Task::dup:
            return std::make_unique<ocgdb::Duplicate>();
        case ocgdb::Task::query:
        case ocgdb::Task::bench:
            return std::make_unique<ocgdb::Search>();
        case ocgdb::Task::sql:
            return std::make_unique<ocgdb::SqlCmd>();
        case ocgdb::Task::info:
            return std::make_unique<ocgdb::DbInfo>();
        default:
            return nullptr;
    }
}

int main(int argc, const char* argv[])
{
    std::cout << "Open Chess Game Database Standard (OCGDB) " << programName << " " << ocgdb::VersionString
#ifdef _CHESS_
              << " (chess, chess960)"
#else
              << " (xiangqi, jieqi)"
#endif
              << std::endl;

    if (argc < 2) {
        print_usage();
        return 0;
    }

    // init
#ifdef _CHESS_
    banksia::ChessBoard::staticInit();
#endif

    auto errCnt = 0;
    ocgdb::ParaRecord paraRecord;

    auto setTask = [&](ocgdb::Task task) {
        if (paraRecord.task != ocgdb::Task::none && paraRecord.task != task) {
            std::cerr << "Error: two tasks given: " << ocgdb::ParaRecord::toString(paraRecord.task) << " and "
                      << ocgdb::ParaRecord::toString(task) << std::endl;
            errCnt++;
        }
        paraRecord.task = task;
    };

    for (auto i = 1; i < argc && errCnt == 0; i++) {
        auto str = std::string(argv[i]);

        if (str == "-h" || str == "-help" || str == "--help") {
            print_usage();
            return 0;
        }
        if (str == "-version" || str == "--version") {
            return 0; // the banner said it
        }
        if (str == "-debug") {
            debugMode = true;
            continue;
        }
        if (str == "-create") {
            setTask(ocgdb::Task::create);
            continue;
        }
        if (str == "-merge") {
            setTask(ocgdb::Task::merge);
            continue;
        }
        if (str == "-export") {
            setTask(ocgdb::Task::export_);
            continue;
        }
        if (str == "-dup") {
            setTask(ocgdb::Task::dup);
            continue;
        }
        if (str == "-bench") {
            setTask(ocgdb::Task::bench);
            continue;
        }
        if (str == "-info") {
            setTask(ocgdb::Task::info);
            continue;
        }

        /// the parameters with a value
        if (i + 1 >= argc) {
            std::cerr << "Error: " << str << " needs a value, or is unknown" << std::endl;
            errCnt++;
            break;
        }
        std::string value = argv[++i];

        if (str == "-pgn") {
            paraRecord.pgnPaths.push_back(value);
        } else if (str == "-db") {
            paraRecord.dbPaths.push_back(value);
        } else if (str == "-r") {
            paraRecord.reportPath = value;
        } else if (str == "-cpu") {
            paraRecord.cpuNumber = std::atoi(value.c_str());
        } else if (str == "-elo") {
            paraRecord.limitElo = std::atoi(value.c_str());
        } else if (str == "-plycount") {
            paraRecord.limitLen = std::atoi(value.c_str());
        } else if (str == "-resultcount") {
            paraRecord.resultNumberLimit = std::atoll(value.c_str());
        } else if (str == "-limit") {
            paraRecord.gameNumberLimit = std::atoll(value.c_str());
        } else if (str == "-desc") {
            paraRecord.desc = value;
        } else if (str == "-password") {
            paraRecord.password = value;
        } else if (str == "-where") {
            paraRecord.where = value;
        } else if (str == "-variant") {
            auto v = banksia::string2ChessVariant(value);
            if (!ocgdb::isVariantSupported(v)) {
                std::cerr << "Error: " << programName << " does not support the variant '" << value << "'"
#ifdef _CHESS_
                          << " (it plays standard and chess960; ocgdb-xq plays xiangqi and jieqi)"
#else
                          << " (it plays xiangqi and jieqi; ocgdb plays standard and chess960)"
#endif
                          << std::endl;
                errCnt++;
            }
            paraRecord.variant = v;
        } else if (str == "-o") {
            if (!paraRecord.setupOptions(value)) {
                errCnt++;
            }
        } else if (str == "-q") {
            setTask(ocgdb::Task::query);
            paraRecord.queries.push_back(value);
        } else if (str == "-sql") {
            setTask(ocgdb::Task::sql);
            paraRecord.sqls.push_back(value);
        } else if (str == "-g") {
            setTask(ocgdb::Task::getgame);
            paraRecord.gameIDVec.push_back(std::atoi(value.c_str()));
        } else {
            std::cerr << "Error: unknown parameter: " << str << std::endl;
            errCnt++;
        }
    }

    if (errCnt) {
        std::cerr << "Run " << programName << " -help for the parameters" << std::endl;
        return 1;
    }

    if (debugMode) {
        std::cout << "All parameters:\n" << paraRecord.toString() << std::endl;
    }

    if (!paraRecord.isValid()) {
        std::cerr << "Error: " << paraRecord.getErrorString() << "\nRun " << programName << " -help for the parameters"
                  << std::endl;
        return 1;
    }

    auto core = createTask(paraRecord.task);
    if (!core) {
        return 1;
    }
    return core->run(paraRecord) ? 0 : 1;
}

static void print_usage()
{
#ifdef _CHESS_
    const std::string db = "big.ocgdb.db3", pgn = "big.pgn";
    const std::string q1 = "Q = 3", q2 = "P[d4, e5, f4, g4] = 4 and kb7", q3 = "N[e5] then q = 0";
#else
    const std::string db = "xq.ocgdb.db3", pgn = "xq.pgn";
    const std::string q1 = "R = 2 and C = 2", q2 = "c[e] = 2 and kb7 = 0", q3 = "H[e5] then r = 0";
#endif
    const std::string p = programName;

    std::cerr << "Usage:\n"
                 " "
              << p << " <task> [<parameters>]\n"
              << "\n"
                 "Tasks:\n"
                 " -create              create a new database from PGN files, works with -db, -pgn\n"
                 " -merge               add the games of PGN files (-pgn) and of other databases (more -db) to the\n"
                 "                      first database (-db)\n"
                 " -export              export a database (-db) to a PGN file (-pgn)\n"
                 " -q <query>           search positions with a PQL query (repeat for more queries) in\n"
                 "                      databases (-db) and PGN files (-pgn)\n"
                 " -sql <statement>     run an SQL statement on a database (repeat for more), works with -db\n"
                 " -g <id>              print the game with this ID (repeat for more), works with -db\n"
                 " -dup                 find duplicate games in databases (-db)\n"
                 " -info                what a database holds, works with -db\n"
                 " -bench               benchmark searching positions, works with -db\n"
                 "\n"
                 "Parameters:\n"
                 " -pgn <file>          a PGN file (xiangqi: also a WXF file), repeat to add more\n"
                 " -db <file>           a database file, extension .ocgdb.db3, repeat to add more\n"
                 "                      (:memory: for a database in memory)\n"
                 " -password <text>     the password of encrypted databases (SQLCipher, as Banksia GUI writes)\n"
                 " -variant <name>      "
#ifdef _CHESS_
                 "standard (default) or chess960\n"
#else
                 "xiangqi (default) or jieqi\n"
#endif
                 " -where <condition>   an SQL condition on the games (-q, -export, -dup), e.g.\n"
                 "                      \"g.WhiteElo >= 2600 AND w.Name LIKE '%Carlsen%'\"\n"
                 " -r <file>            write the results to this file too (-q, -g, -dup, -sql, -info)\n"
                 " -elo <n>             only games where both players have Elo n or more (-create, -merge, -q)\n"
                 " -plycount <n>        only games of n half-moves or more (-create, -merge, -q, -dup)\n"
                 " -resultcount <n>     stop after n results (-q, -sql)\n"
                 " -limit <n>           read n games at most (-create, -merge, -q, -export)\n"
                 " -cpu <n>             number of threads, default: all cores\n"
                 " -desc \"<text>\"       a description, written to the table Info when creating\n"
                 " -debug               print the parameters and the SQL used\n"
                 " -o <opt>[,<opt>...]  options, separated by commas:\n"
                 "    moves             store moves as text, column Moves (-create)\n"
                 "    moves1            store moves in 1 byte (most moves), column Moves1 (-create)\n"
                 "    moves2            store moves in 2 bytes, column Moves2 (-create; default)\n"
                 "    acceptnewtags     a new column for each new PGN tag (-create, -merge)\n"
                 "    discardcomments   do not store comments (-create, -merge)\n"
                 "    discardvariations do not store variations (-create, -merge)\n"
                 "    discardsites      do not store the tag Site (-create, -merge)\n"
                 "    discardnoelo      skip games without the Elo of both players (-create, -merge)\n"
                 "    discardfen        skip games that start from a FEN (-create, -merge)\n"
                 "    discarderr        skip games with errors in their moves (-create, -merge)\n"
                 "    reseteco          compute ECO codes again, even when the games have them (-create, -merge)\n"
                 "    nodup             skip games that are copies of games already added (-create, -merge)\n"
                 "    printall          list the duplicates (-dup)\n"
                 "    printfen          print the FEN of the position found (-q, -g)\n"
                 "    printpgn          print the games found (-q, -dup)\n"
                 "    embededgames      also games that are the start of a longer game (-dup)\n"
                 "    remove            delete the duplicates (-dup)\n"
                 "    nobot             Lichess: skip games played by BOTs (-create, -merge)\n"
                 "    bot               Lichess: only games played by BOTs (-create, -merge)\n"
                 "    keepsite          Lichess: keep the game URL (tag Site) in the column Source (-create)\n"
                 "\n"
                 "Examples:\n"
              << " " << p << " -create -pgn " << pgn << " -db " << db << " -cpu 4\n"
              << " " << p << " -create -pgn a.pgn -pgn b.pgn -db " << db << " -elo 2100 -o moves,moves2,discardsites\n"
              << " " << p << " -merge -db " << db << " -pgn new.pgn -o nodup\n"
              << " " << p << " -export -db " << db << " -pgn out.pgn -where \"g.Date >= '2020'\"\n"
              << " " << p << " -db " << db << " -q \"" << q1 << "\" -q \"" << q2 << "\"\n"
              << " " << p << " -db " << db << " -q \"" << q3 << "\" -o printpgn -r found.pgn\n"
              << " " << p << " -pgn " << pgn << " -q \"" << q1 << "\" -o printfen\n"
              << " " << p << " -db " << db << " -sql \"SELECT COUNT(*) FROM Games WHERE Result = '1-0'\"\n"
              << " " << p << " -db " << db << " -g 423 -g 4432\n"
              << " " << p << " -db " << db << " -dup -o remove,printall -r report.txt\n"
              << " " << p << " -db " << db << " -info\n"
              << "\n"
                 "The user guide: docs/user-guide.md\n";
}
