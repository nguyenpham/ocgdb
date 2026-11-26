/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>

#include "builder.h"
#include "dbread.h"

using namespace ocgdb;

static const std::string sourceFieldName = "Source";
static const char* lichessURL = "https://lichess.org/";
static const size_t lichessURLLength = 20;


/// replace Halfmove clock to 0 and Fullmove number to 1
int Builder::standardizeFEN(char* fenBuf)
{
    assert(fenBuf);
    auto fenSz = static_cast<int>(strlen(fenBuf));
    assert(fenSz > 10 && fenSz < 90);

    for (auto i = fenSz - 2, c = 0; i > 0; --i) {
        if (fenBuf[i] == ' ') {
            c++;
            if (c >= 2) {
                fenBuf[i + 1] = '0';
                fenBuf[i + 2] = ' ';
                fenBuf[i + 3] = '1';
                fenBuf[i + 4] = 0;
                return i + 4;
            }
        }
    }

    return fenSz;
}

/// the escapes of a PGN tag value removed, in place: \" becomes " and \\ becomes \ (PGN standard 8.1.1)
static void unescapeTagValue(char* s)
{
    if (!strchr(s, '\\')) return;
    auto q = s;
    for (auto p = s; *p; p++) {
        if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) {
            p++;
        }
        *q++ = *p;
    }
    *q = 0;
}

bool Builder::runTask()
{
    return paraRecord.task == Task::merge ? merge() : create();
}

/// the move columns asked for; Moves2 when none
static int moveFlags(int optionFlag)
{
    auto movebit = optionFlag & (create_flag_moves | create_flag_moves1 | create_flag_moves2);
    if (!movebit) {
        return create_flag_moves2;
    }
    if ((movebit & (create_flag_moves1 | create_flag_moves2)) == (create_flag_moves1 | create_flag_moves2)) {
        std::cout << "Warning: two binary columns for moves asked for: Moves2 is used, Moves1 is not" << std::endl;
        movebit &= ~create_flag_moves1;
    }
    return movebit;
}

bool Builder::create()
{
    auto dbPath = paraRecord.dbPaths.front();
    if (variant == banksia::ChessVariant::none) {
        variant = defaultVariant();
    }
    if (!isVariantSupported(variant)) {
        std::cerr << "Error: this program does not support the variant " << banksia::chessVariant2String(variant)
                  << std::endl;
        return false;
    }

    paraRecord.optionFlag = (paraRecord.optionFlag & ~(create_flag_moves | create_flag_moves1 | create_flag_moves2))
                            | moveFlags(paraRecord.optionFlag);

    /// a covered man moves as the man of its point, a chariot's slide included: the 1-byte
    /// coding, which keeps only short steps in one byte, has no room for it nor for the reveal
    if (variant == banksia::ChessVariant::jieqi && (paraRecord.optionFlag & create_flag_moves1)) {
        std::cerr << "Error: Moves1 (option moves1) cannot store jieqi games; use moves2 or moves" << std::endl;
        return false;
    }

    std::cout << "Creating " << dbPath << " (" << banksia::chessVariant2String(variant) << ")" << std::endl;

    /// remove old db file if existed
    if (dbPath != ":memory:") {
        std::error_code ec;
        std::filesystem::remove(dbPath, ec);
        if (dbPath.find(".ocgdb.db3") == std::string::npos) {
            std::cout << "Warning: the database file's extension should be .ocgdb.db3" << std::endl;
        }
    }

    if (!createDb(dbPath)) {
        std::cerr << "Error: can't create the database " << dbPath << std::endl;
        return false;
    }

    startTime = getNow();
    for (auto&& path : paraRecord.pgnPaths) {
        if (!working) break;
#ifdef _XIANGQI_
        auto lower = path;
        banksia::toLower(lower);
        isPgn = lower.size() >= 4 && lower.substr(lower.size() - 4) == ".pgn";
#endif
        startAFile();
        processPgnFile(path);
        commitTransaction();
    }

    updateInfoTable();

    /// A game's comments are looked up by GameID: with no index each lookup scanned the whole
    /// table. Built once, after the inserts, which is cheaper than keeping it up to date.
    mDb->exec("CREATE INDEX IF NOT EXISTS idx_comments_gameid ON Comments(GameID)");

    printStats(true);
    closeDb();
    return true;
}

bool Builder::merge()
{
    auto dbPath = paraRecord.dbPaths.front();
    std::cout << "Adding games to " << dbPath << std::endl;

    if (!openForAppending(dbPath)) {
        return false;
    }

    startTime = getNow();
    for (auto&& path : paraRecord.pgnPaths) {
        if (!working) break;
#ifdef _XIANGQI_
        auto lower = path;
        banksia::toLower(lower);
        isPgn = lower.size() >= 4 && lower.substr(lower.size() - 4) == ".pgn";
#endif
        startAFile();
        processPgnFile(path);
        commitTransaction();
    }

#ifdef _XIANGQI_
    isPgn = true;
#endif
    for (size_t i = 1; i < paraRecord.dbPaths.size() && working; i++) {
        addGamesFromDb(paraRecord.dbPaths.at(i));
        commitTransaction();
    }

    updateInfoTable();
    mDb->exec("CREATE INDEX IF NOT EXISTS idx_comments_gameid ON Comments(GameID)");

    printStats(true);
    closeDb();
    return true;
}

void Builder::printStats(bool completed) const
{
    auto elapsed = elapsedMs();
    std::lock_guard<std::mutex> dolock(printMutex);
    std::cout << "#games read: " << processedCnt << ", added: " << addedCnt;
    if (dupCnt > 0) {
        std::cout << ", duplicates skipped: " << dupCnt;
    }
    if (errCnt > 0) {
        std::cout << ", with errors: " << errCnt;
    }
    std::cout << ", elapsed: " << elapsed << " ms " << banksia::secondToClockString(static_cast<int>(elapsed / 1000), ":")
              << ", speed: " << processedCnt * 1000LL / elapsed << " games/s";
    if (fileSize > 0 && !completed) {
        std::cout << ", " << processedPgnSz * 100 / fileSize << "%";
    }
    std::cout << std::endl;
}

void Builder::setupTagVec(const std::vector<std::string>& tagVec, int optionFlag)
{
    create_tagVec = tagVec;
    create_tagMap.clear();

    std::map<std::string, int> knownTagMap;
    for (auto i = 0; i < TagIdx_Max; i++) {
        knownTagMap[knownPgnTagVec[i]] = i;
    }

    auto hasFEN = false;
    for (auto&& s : create_tagVec) {
        if (s == "FEN") hasFEN = true;
        auto it = knownTagMap.find(s);
        if (it != knownTagMap.end()) {
            create_tagMap[s] = it->second;
        }
    }

    if (!hasFEN) {
        create_tagVec.push_back("FEN");
        create_tagMap["FEN"] = TagIdx_FEN;
    }

    auto sz = static_cast<int>(knownTagMap.size());
    for (auto&& s : create_tagVec) {
        if (knownTagMap.find(s) == knownTagMap.end()) {
            create_tagMap[s] = sz++;
        }
    }

    if (optionFlag & create_flag_moves) {
        create_tagVec.push_back("Moves");
    }
    if (optionFlag & (create_flag_moves1 | create_flag_moves2)) {
        create_tagVec.push_back((optionFlag & create_flag_moves2) ? "Moves2" : "Moves1");
    }
}

bool Builder::createDb(const std::string& path)
{
    mDb = openDB(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE, paraRecord.password);
    if (!mDb) {
        return false;
    }
    databasePath = path;

    gameIDCnt = 0;
    eventCnt = playerCnt = siteCnt = 1; // ID 1: the empty name
    commentCnt = 0;

    std::vector<std::string> tagVec;
    for (auto&& s : knownPgnTagVec) {
        if (s != "FEN") tagVec.push_back(s); // FEN is added by setupTagVec, after the known tags
    }
    setupTagVec(tagVec, paraRecord.optionFlag);

    try {
        auto& db = *mDb;
        db.exec("DROP TABLE IF EXISTS Info");
        db.exec("CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT)");
        setInfo(db, "Data Structure Version", VersionDatabaseString);
        setInfo(db, "Type", "OCGDB");
        setInfo(db, "Version", VersionUserDatabaseString); // User Data version
        setInfo(db, "Variant", banksia::chessVariant2String(variant));
        setInfo(db, "License", "free");
        setInfo(db, "Description", paraRecord.desc);
        setInfo(db, "Generator", "ocgdb " + VersionString);

        db.exec("DROP TABLE IF EXISTS Events");
        db.exec("CREATE TABLE Events (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE)");
        db.exec("INSERT INTO Events (Name) VALUES ('')"); // default empty

        db.exec("DROP TABLE IF EXISTS Sites");
        db.exec("CREATE TABLE Sites (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE)");
        db.exec("INSERT INTO Sites (Name) VALUES ('')"); // default empty

        db.exec("DROP TABLE IF EXISTS Players");
        db.exec("CREATE TABLE Players (ID INTEGER PRIMARY KEY, Name TEXT UNIQUE, Elo INTEGER)");
        db.exec("INSERT INTO Players (ID, Name) VALUES (1, '')"); // default empty

        // Table Games
        {
            db.exec("DROP TABLE IF EXISTS Games");

            std::string sql0 = "CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT", sql1;
            for (auto&& str : create_tagVec) {
                if (str == "ID") {
                    continue;
                }

                sql0 += ", " + str;

                std::string stype = "TEXT";

                // Special fields, using ID
                if (str == "Event" || str == "Site" || str == "White" || str == "Black") {
                    sql1 += ", FOREIGN KEY(" + str + "ID) REFERENCES " + (str == "Event"  ? "Events"
                                                                          : str == "Site" ? "Sites"
                                                                                          : "Players");
                    sql0 += "ID";
                    stype = "INTEGER";
                } else if (str == "WhiteElo" || str == "BlackElo" || str == "PlyCount") {
                    stype = "INTEGER";
                } else if (str == "Moves1" || str == "Moves2") {
                    stype = "BLOB DEFAULT NULL";
                }

                sql0 += " " + stype;
            }

            db.exec(sql0 + sql1 + ")");
        }

        db.exec("DROP TABLE IF EXISTS Comments");
        db.exec("CREATE TABLE Comments (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INTEGER, Ply INTEGER, "
                "Comment TEXT)");

        /// the opening tree of Banksia GUI, built from the games; a new database has none
        db.exec("DROP TABLE IF EXISTS Tree");

        db.exec("PRAGMA journal_mode=OFF");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    extractGameTableInfo();
    return true;
}

std::string Builder::nameKey(const char* name)
{
    std::string s = name;
    banksia::trim(s);
    banksia::toLower(s);
    return s;
}

void Builder::loadNameMap(const std::string& tableName, std::unordered_map<std::string, IDInteger>& idMap,
                          IDInteger& maxId)
{
    idMap.clear();
    maxId = 1;
    if (!hasTable(*mDb, tableName)) {
        return;
    }
    SQLite::Statement q(*mDb, "SELECT ID, Name FROM " + tableName);
    while (q.executeStep()) {
        auto id = q.getColumn(0).getInt();
        auto c = q.getColumn(1);
        maxId = std::max(maxId, id);
        if (!c.isNull()) {
            auto key = nameKey(c.getText());
            if (!key.empty()) {
                idMap[key] = id;
            }
        }
    }
}

bool Builder::openForAppending(const std::string& path)
{
    if (!openOCGDB(path, false)) {
        return false;
    }

    if (searchField == SearchField::none) {
        std::cerr << "Error: database " << path << " has no column for moves" << std::endl;
        return false;
    }

    /// the database's own columns decide what is stored
    paraRecord.optionFlag &= ~(create_flag_moves | create_flag_moves1 | create_flag_moves2);
    std::vector<std::string> tagVec;
    {
        static const std::set<std::string> idSet { "EventID", "SiteID", "WhiteID", "BlackID" };

        SQLite::Statement stmt(*mDb, "PRAGMA table_info(Games)");
        while (stmt.executeStep()) {
            std::string fieldName = stmt.getColumn(1).getString();

            if (fieldName == "Moves2") {
                paraRecord.optionFlag |= create_flag_moves2;
                continue;
            }
            if (fieldName == "Moves1") {
                paraRecord.optionFlag |= create_flag_moves1;
                continue;
            }
            if (fieldName == "Moves") {
                paraRecord.optionFlag |= create_flag_moves;
                continue;
            }

            if (idSet.count(fieldName)) {
                fieldName = fieldName.substr(0, fieldName.size() - 2);
            }
            tagVec.push_back(fieldName);
        }
    }
    paraRecord.optionFlag = (paraRecord.optionFlag & ~(create_flag_moves | create_flag_moves1 | create_flag_moves2))
                            | moveFlags(paraRecord.optionFlag);
    setupTagVec(tagVec, paraRecord.optionFlag);

    if (!hasTable(*mDb, "Comments")) {
        mDb->exec("CREATE TABLE Comments (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INTEGER, Ply INTEGER, "
                  "Comment TEXT)");
    }

    loadNameMap("Players", playerIdMap, playerCnt);
    loadNameMap("Events", eventIdMap, eventCnt);
    loadNameMap("Sites", siteIdMap, siteCnt);

    gameIDCnt = 0;
    {
        SQLite::Statement q(*mDb, "SELECT MAX(ID) FROM Games");
        if (q.executeStep() && !q.getColumn(0).isNull()) {
            gameIDCnt = q.getColumn(0).getInt();
        }
    }

    std::cout << "The database has " << gameCnt << " games, " << playerIdMap.size() << " players, variant "
              << banksia::chessVariant2String(variant) << std::endl;
    return true;
}

bool Builder::addNewField(const std::string& fieldName)
{
    std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
    if (create_tagMap.find(fieldName) != create_tagMap.end()) {
        return true;
    }

    /// a tag name becomes a column name: letters, digits and _ only
    for (auto ch : fieldName) {
        if (!isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }
    if (fieldName.empty() || isdigit(static_cast<unsigned char>(fieldName[0]))) {
        return false;
    }

    try {
        {
            std::lock_guard<std::mutex> lock(transactionMutex);
            sendTransaction(false);
            mDb->exec("ALTER TABLE Games ADD COLUMN " + fieldName + " TEXT");
            sendTransaction(true);
            transactionCnt = 0;
        }
        auto sz = static_cast<int>(create_tagMap.size());
        create_tagMap[fieldName] = sz;
        /// before the move columns, which are last
        auto it = create_tagVec.end();
        while (it != create_tagVec.begin()
               && (*(it - 1) == "Moves" || *(it - 1) == "Moves1" || *(it - 1) == "Moves2")) {
            --it;
        }
        create_tagVec.insert(it, fieldName);
    } catch (const std::exception& e) {
        std::cerr << "Error: can't add the column " << fieldName << ": " << e.what() << std::endl;
        return false;
    }
    return true;
}

void Builder::updateInfoTable()
{
    assert(mDb);
    commitTransaction();
    refreshInfoCounts(*mDb);
}

IDInteger Builder::getPlayerNameId(ThreadRecord* t, const char* name, int elo)
{
    std::lock_guard<std::mutex> dolock(playerMutex);
    return getNameId(name, elo, playerCnt, t->playerInsertStatement, playerIdMap);
}

IDInteger Builder::getEventNameId(ThreadRecord* t, const char* name)
{
    std::lock_guard<std::mutex> dolock(eventMutex);
    return getNameId(name, -1, eventCnt, t->eventInsertStatement, eventIdMap);
}

IDInteger Builder::getSiteNameId(ThreadRecord* t, const char* name)
{
    std::lock_guard<std::mutex> dolock(siteMutex);
    return getNameId(name, -1, siteCnt, t->siteInsertStatement, siteIdMap);
}

IDInteger Builder::getNameId(const char* name, int elo, IDInteger& cnt, SQLite::Statement* insertStatement,
                             std::unordered_map<std::string, IDInteger>& idMap)
{
    /// a game without the tag (no [Site], say) passes nullptr
    if (!name) return 1;

    while (*name > 0 && *name <= ' ') {
        name++;
    }

    /// empty, * or ?
    if (*name == 0 || *name == '*' || *name == '?') return 1;

    auto key = nameKey(name);
    auto it = idMap.find(key);
    if (it != idMap.end()) {
        return it->second;
    }

    auto theId = ++cnt;

    std::string theName = name;
    banksia::trim(theName);

    try {
        insertStatement->reset();
        insertStatement->clearBindings();
        insertStatement->bind(1, theId);
        insertStatement->bind(2, theName);
        if (elo > 0) {
            insertStatement->bind(3, elo);
        }
        insertStatement->exec();
    } catch (const std::exception& e) {
        std::cerr << "Error: can't add the name " << theName << ": " << e.what() << std::endl;
        --cnt;
        return 1;
    }

    idMap[key] = theId;
    return theId;
}

void Builder::startAFile()
{
    std::lock_guard<std::mutex> dolock(gameMutex);
    fileBaseID = gameIDCnt;
    pgnGameIndex = 0;
}

IDInteger Builder::getNewGameID()
{
    std::lock_guard<std::mutex> dolock(gameMutex);
    auto id = fileBaseID + static_cast<IDInteger>(currentGameIndex);
    gameIDCnt = std::max(gameIDCnt, id);
    return id;
}

/// The game in t->board against the games added so far: the same start and the same moves of
/// the main line. The key is a 64-bit FNV-1a hash of the start position and of every move of the
/// main line. (Banksia GUI keys on the final position, the length and four of the moves; games
/// that transpose into the same position were taken for copies: 36 of 39,478 games of a test.)
bool Builder::isBoardDuplication(ThreadRecord* t, IDInteger&)
{
    assert(t && t->board);

    uint64_t key = 0xcbf29ce484222325ULL;
    auto mix = [&key](uint64_t v) {
        for (int i = 0; i < 8; i++) {
            key ^= (v >> (i * 8)) & 0xff;
            key *= 0x100000001b3ULL;
        }
    };
    for (auto ch : t->board->startFen) {
        mix(static_cast<uint8_t>(ch));
    }
    mix(0xffff);
    for (auto&& h : t->board->_mainLineHists()) {
        mix(t->board->encode2Bytes(*h));
    }

    std::lock_guard<std::mutex> dolock(dupHashKeyMutex);
    return !gameKeySet.insert(key).second;
}

/// Adds a game when creating or merging a database
void Builder::processPGNGameWithAThread(ThreadRecord* t, const std::unordered_map<const char*, char*>& itemMap,
                                        const char* moveText)
{
    assert(t);

    if (!working) {
        return;
    }

    t->initForInserting(mDb, variant);
    assert(t->board);

    if (itemMap.empty()) {
        errCnt++;
        return;
    }

    /// values by column name: texts and numbers
    std::map<std::string, std::string> stringMap;
    std::map<std::string, int> intMap;

    auto whiteElo = 0, blackElo = 0, plyCount = 0, botCnt = 0;
    const char *whiteName = nullptr, *blackName = nullptr, *eventName = nullptr, *siteName = nullptr;
    std::string fenString, ecoString, resultString, dateString;

    intMap["EventID"] = 1; // empty
    intMap["SiteID"] = 1;

    for (auto&& it : itemMap) {
        auto s = it.second;
        unescapeTagValue(s);

        if (strcmp(it.first, "Variant") == 0) {
            auto v = banksia::string2ChessVariant(it.second);
            /// "Standard", "chess" and the like are not variants this core names
            if (v != banksia::ChessVariant::none && v != variant) {
                errCnt++;
                return;
            }
            continue;
        }
        if (strcmp(it.first, "SetUp") == 0) {
            continue;
        }

        /// Lichess bot
        if ((paraRecord.optionFlag & (lichess_flag_nobot | lichess_flag_bot))
            && (strcmp(it.first, "WhiteTitle") == 0 || strcmp(it.first, "BlackTitle") == 0)
            && strcmp(it.second, "BOT") == 0) {
            if (paraRecord.optionFlag & lichess_flag_nobot) {
                return;
            }
            botCnt++;
        }

        int tagIdx = -1;
        {
            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
            auto it2 = create_tagMap.find(it.first);
            if (it2 != create_tagMap.end()) {
                tagIdx = it2->second;
            }
        }

        if (tagIdx >= 0) {
            while (*s <= ' ' && *s > 0) s++; // trim left

            switch (tagIdx) {
                case TagIdx_Event:
                    eventName = s;
                    break;
                case TagIdx_Site:
                    if ((paraRecord.optionFlag & create_flag_discard_sites) == 0) {
                        siteName = s;
                    }
                    break;
                case TagIdx_White:
                    whiteName = s;
                    break;
                case TagIdx_Black:
                    blackName = s;
                    break;

                case TagIdx_Result: {
                    char ch0 = s[0], ch1 = s[0] ? s[1] : 0;
                    if (ch0 == '1' && ch1 == '-') { /// 1-0
                        resultString = banksia::resultType2String(banksia::ResultType::win, false);
                    } else if (ch0 == '0' && ch1 == '-') { /// 0-1
                        resultString = banksia::resultType2String(banksia::ResultType::loss, false);
                    } else if ((ch0 == '1' && ch1 == '/') || (ch0 == '0' && ch1 == '.')) { /// 1/2-1/2 or 0.5-0.5
                        resultString = banksia::resultType2String(banksia::ResultType::draw, false);
                    }
                    break;
                }

                case TagIdx_FEN:
                    if (paraRecord.optionFlag & create_flag_discard_fen) {
                        return;
                    }
                    fenString = s;
                    break;

                case TagIdx_Date:
                    dateString = s;
                    break;

                case TagIdx_WhiteElo:
                case TagIdx_BlackElo: {
                    auto elo = atoi(s);
                    if (elo <= 300 || elo >= 5000) {
                        elo = 0;
                    } else {
                        intMap[tagIdx == TagIdx_WhiteElo ? "WhiteElo" : "BlackElo"] = elo;
                    }
                    (tagIdx == TagIdx_WhiteElo ? whiteElo : blackElo) = elo;
                    break;
                }

                case TagIdx_PlyCount:
                    plyCount = atoi(s); // replaced by the count of the moves below
                    break;

                case TagIdx_ECO:
                    ecoString = s;
                    break;

                case TagIdx_GameID:
                    break;

                default: {
                    // ignore empty string or one started with *, ?
                    auto ch = s[0];
                    if (ch == 0 || ch == '*' || ch == '?') {
                        break;
                    }
                    stringMap[it.first] = s;
                    break;
                }
            } /// switch
            continue;
        }

        if (strcmp(it.first, "UTCDate") == 0) {
            if (dateString.empty()) {
                dateString = s;
            }
            continue;
        }

        if (strcmp(it.first, "UTCTime") == 0) {
            std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
            if (create_tagMap.find("Time") != create_tagMap.end() && !stringMap.count("Time")) {
                stringMap["Time"] = s;
            }
            continue;
        }

        if ((paraRecord.optionFlag & create_flag_accept_new_tags) && addNewField(it.first)) {
            auto ch = s[0];
            if (ch != 0 && ch != '*' && ch != '?') {
                stringMap[it.first] = s;
            }
        }
    } /// loop

    if ((paraRecord.optionFlag & lichess_flag_bot) && !botCnt) {
        return;
    }

    if ((paraRecord.optionFlag & create_flag_discard_no_elo) && (whiteElo <= 0 || blackElo <= 0)) {
        return;
    }

    if (paraRecord.limitElo > 0 && (whiteElo < paraRecord.limitElo || blackElo < paraRecord.limitElo)) {
        return;
    }

    /// Quick validate fenString
    {
        auto len = fenString.size();
        if (len < 12 && (variant == banksia::ChessVariant::chess960 || len > 0)) {
            errCnt++;
            return;
        }

        if (!t->board->newGame(fenString)) {
            errCnt++;
            return;
        }
    }

    /// the start position of the game, as the board sees it (NULL for the standard one)
    fenString = t->board->startFen;
    if (!fenString.empty()) {
        stringMap["FEN"] = fenString;
    }

    if (!resultString.empty()) {
        stringMap["Result"] = resultString;
    }

    if (!dateString.empty()) {
        dateString = validDateString(dateString);
        if (!dateString.empty()) {
            stringMap["Date"] = dateString;
        }
    }

    /// Parse moves
    const auto binaryMoves = (paraRecord.optionFlag & (create_flag_moves1 | create_flag_moves2)) != 0;
    {
        /// no SAN strings: nothing stored needs them, and writing them (a legality test of the
        /// replies at every check) took 28% of the time (153,600 xiangqi games: 7.2 s -> 5.2 s)
        int flag = banksia::ParseMoveListFlag_quick_check;
        if (paraRecord.optionFlag & create_flag_discard_comments) {
            flag |= banksia::ParseMoveListFlag_discardComment;
        }

        banksia::PgnRecord record;
        record.moveText = moveText;
#ifdef _XIANGQI_
        auto notation = isPgn ? banksia::Notation::san : banksia::Notation::traditional_en;
#else
        auto notation = banksia::Notation::san;
#endif
        auto parseVariation = (paraRecord.optionFlag & create_flag_discard_variations) || !binaryMoves
                                      ? banksia::ParseVariation::discard
                                      : banksia::ParseVariation::parse;
        t->board->fromMoveList(record, notation, parseVariation, banksia::moveFormat, flag);

        plyCount = t->board->getMainLineSize(); /// not the variations' moves

        if (paraRecord.limitLen > plyCount) {
            return;
        }

        if (t->board->parseMoveErrorCnt > 0) {
            errCnt++;
            if (paraRecord.optionFlag & create_flag_discard_error_games) {
                return;
            }
        }
    }

    IDInteger gameID = -1;

    /// Duplication
    if ((paraRecord.optionFlag & create_flag_no_dup) && isBoardDuplication(t, gameID)) {
        dupCnt++;
        return;
    }

    createNewTransaction();

    /// Player names
    intMap["WhiteID"] = getPlayerNameId(t, whiteName, whiteElo);
    intMap["BlackID"] = getPlayerNameId(t, blackName, blackElo);

    /// Event & Site
    if (eventName) {
        intMap["EventID"] = getEventNameId(t, eventName);
    }

    if (siteName) {
        /// a Lichess "site" is the URL of the game: kept, if asked, in the column Source
        if (memcmp(siteName, lichessURL, lichessURLLength) == 0 && strlen(siteName) > lichessURLLength + 5) {
            if ((paraRecord.optionFlag & lichess_keep_site) && addNewField(sourceFieldName)) {
                stringMap[sourceFieldName] = siteName;
            }
        } else {
            intMap["SiteID"] = getSiteNameId(t, siteName);
        }
    }

    if (gameID < 0) {
        gameID = getNewGameID();
    }
    intMap["ID"] = gameID;
    intMap["PlyCount"] = plyCount;

    if (ecoString.empty() || (paraRecord.optionFlag & create_flag_reset_eco)) {
        auto eco = t->board->getLastEco();
        if (!eco.ecoString.empty()) {
            ecoString = eco.ecoString;
        }
    }
    if (!ecoString.empty()) {
        stringMap["ECO"] = ecoString;
    }

    if (paraRecord.optionFlag & create_flag_moves) {
        // trim left
        while (*moveText > 0 && *moveText <= ' ') moveText++;
        stringMap["Moves"] = moveText;
    }

    /// Moves1 / Moves2, with the variations, and the comments
    if (binaryMoves) {
        t->buf.clear();
        const auto moves2 = (paraRecord.optionFlag & create_flag_moves2) != 0;
        auto addCnt = 0;

        /// every hist, the variations' too (plyCount is the main line's)
        const auto histCnt = t->board->getHistListSize();
        banksia::VariationRec vr;
        uint16_t encode2byte_1 = 0, encode2byte_2 = 0; // 1 up, 2 down

        for (auto i = 0; i < histCnt; i++) {
            auto h = t->board->_getHistPointerAt(i);

            if (moves2) { // 2 bytes encoding
                if (vr != h->variationRec) {
                    if (encode2byte_1 == 0) {
                        banksia::Hist hist;
                        hist.move.from = hist.move.dest = 1;
                        hist.move.promotion = 0;
                        encode2byte_1 = t->board->encode2Bytes(hist);

                        hist.move.from = hist.move.dest = 2;
                        encode2byte_2 = t->board->encode2Bytes(hist);
                        assert(encode2byte_1 != 0 && encode2byte_2 != 0);
                    }

                    auto d = h->variationRec.level - vr.level;
                    auto m = encode2byte_1; /// 1 go up
                    if (d != 0) {
                        if (d < 0) {
                            d = -d;
                            m = encode2byte_2; /// 2 go down
                        }
                        for (auto j = 0; j < d; j++) {
                            t->buf.push_back(static_cast<uint8_t>(m));
                            t->buf.push_back(static_cast<uint8_t>(m >> 8));
                        }
                    } else {
                        /// Changed groups, go down then back up or vs
                        auto m2 = encode2byte_2;
                        if (vr.groupIdx < h->variationRec.groupIdx) {
                            std::swap(m, m2);
                        }
                        t->buf.push_back(static_cast<uint8_t>(m));
                        t->buf.push_back(static_cast<uint8_t>(m >> 8));

                        t->buf.push_back(static_cast<uint8_t>(m2));
                        t->buf.push_back(static_cast<uint8_t>(m2 >> 8));
                    }

                    vr = h->variationRec;
                }

                auto m = t->board->encode2Bytes(*h);
                t->buf.push_back(static_cast<uint8_t>(m));
                t->buf.push_back(static_cast<uint8_t>(m >> 8));

            } else { /// Moves1: the main line only (the 1-byte coding has no marks for variations)
                if (h->variationRec.level > 0) {
                    continue;
                }
                auto pair = t->board->encode1Byte(h->move);
                assert(pair.second == 1 || pair.second == 2);
                t->buf.push_back(static_cast<uint8_t>(pair.first));
                if (pair.second == 2) {
                    t->buf.push_back(static_cast<uint8_t>(pair.first >> 8));
                }
            }

            if (!h->comment.empty() && (moves2 || h->variationRec.level == 0)) {
                try {
                    auto& c = *t->insertCommentStatement;
                    c.reset();
                    c.bind(1, gameID);
                    c.bind(2, moves2 ? i : addCnt);
                    c.bind(3, h->comment);
                    c.exec();
                    commentCnt++;
                } catch (const std::exception& e) {
                    std::cerr << "Error: comment of game " << gameID << ": " << e.what() << std::endl;
                }
            }
            addCnt++;
        }
    }

    /// first comment
    if (binaryMoves && !t->board->getFirstComment().empty()) {
        try {
            auto& c = *t->insertCommentStatement;
            c.reset();
            c.bind(1, gameID);
            c.bind(2, -1);
            c.bind(3, t->board->getFirstComment());
            c.exec();
            commentCnt++;
        } catch (const std::exception& e) {
            std::cerr << "Error: comment of game " << gameID << ": " << e.what() << std::endl;
        }
    }

    t->hdpLen += plyCount;

    {
        std::lock_guard<std::mutex> dolock(create_tagFieldMutex);
        if (!t->insertGameStatement || t->insertGameStatementIdxSz != create_tagVec.size()) {
            t->createInsertGameStatement(mDb, create_tagVec);
        }
    }

    if (!t->insertGameStatement) {
        errCnt++;
        return;
    }

    try {
        auto& stmt = *t->insertGameStatement;
        stmt.reset();
        stmt.clearBindings(); /// NULL for the columns this game has no value for

        for (size_t k = 0; k < t->usedFieldList.size(); k++) {
            auto&& name = t->usedFieldList.at(k);
            auto idx = static_cast<int>(k + 1);

            if (name == "Moves1" || name == "Moves2") {
                if (!t->buf.empty()) {
                    stmt.bind(idx, t->buf.data(), static_cast<int>(t->buf.size()));
                }
                continue;
            }

            auto it = intMap.find(name);
            if (it != intMap.end()) {
                stmt.bind(idx, it->second);
                continue;
            }
            auto it2 = stringMap.find(name);
            if (it2 != stringMap.end()) {
                stmt.bind(idx, it2->second);
            }
        }

        stmt.exec();
        addedCnt++;
    } catch (const std::exception& e) {
        std::cerr << "Error: can't add game " << gameID << ": " << e.what() << std::endl;
        errCnt++;
    }
}

/// The games of another database are written as PGN, a block at a time, and read back through
/// the PGN reader: one way into the database, whatever the games come from.
void Builder::addGamesFromDb(const std::string& path)
{
    std::cout << "Adding the games of " << path << std::endl;

    auto db = openDB(path, SQLite::OPEN_READONLY, paraRecord.password);
    if (!db) {
        errCnt++;
        return;
    }

    {
        auto v = readVariant(*db);
        if (v == banksia::ChessVariant::none) {
            v = defaultVariant();
        }
        if (v != variant) {
            std::cerr << "Error: " << path << " is a database of " << banksia::chessVariant2String(v)
                      << ", not of " << banksia::chessVariant2String(variant) << std::endl;
            delete db;
            errCnt++;
            return;
        }
    }

    /// the columns and the query of the other database
    DbRead reader;
    if (!reader.openToRead(path, paraRecord.password)) {
        delete db;
        errCnt++;
        return;
    }
    delete db;

    std::string block;
    auto flush = [&]() {
        if (block.empty()) return;
        std::vector<char> buf(block.begin(), block.end());
        buf.resize(buf.size() + 4, 0);
        processDataBlock(buf.data(), static_cast<long>(block.size()), false);
        pool->wait_for_tasks();
        block.clear();
    };

    startAFile();
    reader.forEachGameAsPgn([&](const std::string& pgn) {
        block += pgn;
        block += "\n";
        if (block.size() > 4 * 1024 * 1024) {
            flush();
            printProgress();
        }
        return working.load();
    });
    flush();
}
