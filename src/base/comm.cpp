/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#include "comm.h"

namespace banksia {

    /// CMake passes the project version as APP_VERSION, the same way it passes
    /// APP_NAME: one source of truth, so a release cannot ship binaries that
    /// disagree with their own tag.
    ///
    /// Guarded because Banksia GUI compiles this same file and defines
    /// APP_NAME but not APP_VERSION.
#ifdef APP_VERSION
    const char* BANKSIA_VERSION = APP_VERSION;
#else
    const char* BANKSIA_VERSION = "4.0";
#endif

    std::string getVersion()
    {
        return BANKSIA_VERSION;
    }

    std::string getAppName()
    {
        return APP_NAME;
    }

    // ------------------------------------------------------------------------
    //  Process-wide settings
    // ------------------------------------------------------------------------

    bool banksiaVerbose = false;
    bool profileMode = false;
    bool consoleMode = false;


#ifdef _BANKSIA_
    ChessVariant chessVariant = ChessVariant::standard;
#else
    ChessVariant chessVariant = ChessVariant::xq;
#endif

    MoveFormat moveFormat(FENCharactorSet::standard, false);

    /// UCCI is the default standard for the xiangqi products.
    MoveFormat moveFormat_ucci(FENCharactorSet::standard, false);
    MoveFormat moveFormat_wiki(FENCharactorSet::set2, false);

    bool pvHistoryWanted()
    {
        /// The GUI shows principal variations in the eval table, the chart, the
        /// coach and the puzzle analyser, so it always wants them. Only console
        /// runs can skip the work, and only when the comment format cannot
        /// print a PV.
        return !consoleMode;
    }

    static const char* pieceTypeName = ".kqrbnpx";

    static const char* xqPieceTypeNames[2] = {
        ".kabrcnpx", // standard Bishop/kNight
        ".kaerchpx" // set 2, Elephant, Horse
    };

    const char* reasonStrings[] = { "*",
                                    "mate",
                                    "stalemate",
                                    "repetition",
                                    "resign",
                                    "fifty moves",
                                    "insufficient material",
                                    "illegal move",
                                    "timeout",
                                    "adjudication by lengths",
                                    "adjudication by egtb",
                                    "adjudication by online egtb",
                                    "adjudication by engines' scores",
                                    "adjudication by human",
                                    "perpetual chase",
                                    "both perpetual chases",
                                    "extra comment",
                                    "crash",
                                    "abort",
                                    nullptr };

    // noresult, win, draw, loss
    static const char* resultStrings[] = { "*", "1-0", "1/2-1/2", "0-1", nullptr };
    static const char* resultStrings_short[] = { "*", "1-0", "0.5", "0-1", nullptr };

    static const char* sideStrings[] = { "black", "white", "none", nullptr };
    static const char* shortSideStrings[] = { "b", "w", "n", nullptr };

    static const char* variantStrings[] = { "xiangqi", "jieqi", "standard", "chess960", "unknown", nullptr };

    // ------------------------------------------------------------------------
    //  Starting positions
    // ------------------------------------------------------------------------

    /// Indexed by ChessVariant, so the order must match types.h exactly:
    /// xq, jieqi, standard, chess960, none.
    static const char* originFens[]
            = { /// Xq, Jieqi -> UCCI
                "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1",
                "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/9/X1X1X1X1X/1X5X1/9/XXXXKXXXX w - - 0 1",

                /// Chess, Chess960
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq",
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq",

                /// none -- a real entry, not padding, so getOriginFen(ChessVariant::none)
                /// stays inside this array.
                ""
              };

    /// The xiangqi start position written with the Betza/wiki piece letters.
    /// Used only by the xiangqi build, which is why a chess-target warning
    /// calls it unused.
#ifdef _XIANGQI_
    [[maybe_unused]] static const char* originFen_wiki
            = "rheakaehr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RHEAKAEHR w - - 0 1";
#endif

    /// Zobrist key of each starting position, same order. Only xiangqi's is a
    /// real key; the chess entries are placeholders that no caller compares
    /// against, and the array simply ended before `none` -- so asking for the
    /// key of the last variant read past it.
    static const u64 originHashKeys[] = {
        138564303962928357ULL, /// xq, from the table in xq/xq_hash.cpp
        1, /// jieqi
        1, /// standard
        1, /// chess960
        0 /// none
    };

    /// The position a cleared board shows: the two kings and nothing else.
    /// Same order again.
    static const char* emptyFens[] = {
        "4k4/9/9/9/9/9/9/9/9/4K4 w - - 0 1", /// xq
        "4k4/9/9/9/9/9/9/9/9/4K4 w - - 0 1", /// jieqi
        "8/8/8/8/8/8/8/8 w KQkq", /// standard
        "8/8/8/8/8/8/8/8 w KQkq", /// chess960
        "8/8/8/8/8/8/8/8 w KQkq" /// none
    };

    static constexpr int variantCount = static_cast<int>(ChessVariant::none) + 1;

    static_assert(sizeof(originFens) / sizeof(originFens[0]) == variantCount,
                  "originFens must have one entry per ChessVariant");
    static_assert(sizeof(originHashKeys) / sizeof(originHashKeys[0]) == variantCount,
                  "originHashKeys must have one entry per ChessVariant");
    static_assert(sizeof(emptyFens) / sizeof(emptyFens[0]) == variantCount,
                  "emptyFens must have one entry per ChessVariant");

    std::string getOriginFen(ChessVariant variant)
    {
        auto idx = static_cast<int>(variant);
        if (idx < 0 || idx >= variantCount) {
            return "";
        }
#ifdef _XIANGQI_
        if (variant == ChessVariant::xq && moveFormat.charSet == FENCharactorSet::set2) {
            return originFen_wiki;
        }
#endif
        return originFens[idx];
    }

    std::string getEmptyFen(ChessVariant variant)
    {
        auto idx = static_cast<int>(variant);
        if (idx < 0 || idx >= variantCount) {
            return "";
        }
        return emptyFens[idx];
    }

    u64 getOriginHashKey(ChessVariant variant)
    {
        auto idx = static_cast<int>(variant);
        if (idx < 0 || idx >= variantCount) {
            return 0;
        }
        return originHashKeys[idx];
    }

    // ------------------------------------------------------------------------
    //  Variant families
    // ------------------------------------------------------------------------

    bool isVariantValid(ChessVariant variant)
    {
        switch (variant) {
#ifdef _XIANGQI_
            case ChessVariant::xq:
            case ChessVariant::jieqi:
                return true;
#endif

#ifdef _CHESS_
            case ChessVariant::standard:
            case ChessVariant::chess960:
                return true;
#endif

            default:
                return false;
        }
    }

    bool isChessFamily(ChessVariant variant)
    {
        return variant == ChessVariant::standard || variant == ChessVariant::chess960;
    }

    bool isXqFamily(ChessVariant variant)
    {
        return variant == ChessVariant::xq || variant == ChessVariant::jieqi;
    }

    // ------------------------------------------------------------------------
    //  Board vocabulary
    // ------------------------------------------------------------------------

    char chessPieceType2Char(int pieceType)
    {
        return pieceTypeName[pieceType];
    }

    int chessCharactorToPieceType(char ch)
    {
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }
        const char* p = strchr(pieceTypeName, ch);
        if (p != nullptr) {
            return static_cast<int>(p - pieceTypeName);
        }
        return EMPTY;
    }

    int chessCoordinateStringToPos(const std::string& str)
    {
        if (str.size() < 2) {
            return -1;
        }
        auto colChr = str[0], rowChr = str[1];
        if (colChr >= 'a' && colChr <= 'h' && rowChr >= '1' && rowChr <= '8') {
            int col = colChr - 'a';
            int row = rowChr - '1';
            return (7 - row) * 8 + col;
        }
        return -1;
    }

    std::string chessPosToCoordinateString(int pos)
    {
        auto row = pos / 8, col = pos % 8;

        char buf[5];
        buf[0] = char('a' + col);
        buf[1] = char('0' + 8 - row);
        buf[2] = 0;
        return buf;
    }

    char xqPieceType2Char(int pieceType, const MoveFormat& mf)
    {
        return xqPieceTypeNames[static_cast<int>(mf.charSet)][pieceType];
    }

    char xqPieceType2Char(int pieceType)
    {
        return xqPieceType2Char(pieceType, moveFormat);
    }

    int xqCharactorToPieceType(char ch)
    {
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }

        /// Fold the wiki set onto the standard one, so a FEN written either way
        /// reads the same.
        if (ch == 'e')
            ch = 'b';
        else if (ch == 'h')
            ch = 'n';

        const char* p = strchr(xqPieceTypeNames[0], ch);
        /// strchr returns null for a character that is not a piece letter --
        /// a digit or a '/' in a FEN, say. Subtracting the base pointer from
        /// that null was undefined behaviour and in practice produced a large
        /// bogus piece type rather than EMPTY.
        if (!p || ch == 0) {
            return EMPTY;
        }
        return static_cast<int>(p - xqPieceTypeNames[0]);
    }

    std::string xqPosToCoordinateString(int pos, const MoveFormat& mf)
    {
        int row = 9 - pos / 9, col = pos % 9;
        if (mf.rankFrom1) row++;
        return char('a' + col) + std::to_string(row);
    }

    // ------------------------------------------------------------------------
    //  Names of things
    // ------------------------------------------------------------------------

    std::string resultType2String(ResultType type, bool shortFrom)
    {
        auto t = static_cast<int>(type);
        if (t < 0 || t > 3) t = 0;
        return shortFrom ? resultStrings_short[t] : resultStrings[t];
    }

    ResultType string2ResultType(const std::string& s)
    {
        for (auto i = 0; resultStrings[i]; i++) {
            if (resultStrings[i] == s) {
                return static_cast<ResultType>(i);
            }
        }

        /// Only "1/2-1/2" is written, but a draw arrives spelled several other
        /// ways and an unrecognised result is not an error anyone sees -- the
        /// game simply stops counting
        static const char* drawAliases[] = { "0.5-0.5", "1/2", "0.5", "\xc2\xbd-\xc2\xbd", nullptr };
        for (auto i = 0; drawAliases[i]; i++) {
            if (drawAliases[i] == s) {
                return ResultType::draw;
            }
        }
        return ResultType::noresult;
    }

    std::string reasonType2String(ReasonType type)
    {
        auto t = static_cast<int>(type);
        if (t < 0) t = 0;
        return reasonStrings[t];
    }

    ReasonType string2ReasonType(const std::string& s)
    {
        for (auto i = 0; reasonStrings[i]; i++) {
            if (reasonStrings[i] == s) {
                return static_cast<ReasonType>(i);
            }
        }
        return s.empty() ? ReasonType::noreason : ReasonType::extracomment;
    }

    std::string side2String(Side side, bool shortFrom)
    {
        auto sd = static_cast<int>(side);
        if (sd < 0 || sd > 1) sd = 2;
        return shortFrom ? shortSideStrings[sd] : sideStrings[sd];
    }

    Side string2Side(std::string s)
    {
        toLower(s);
        for (auto i = 0; sideStrings[i]; i++) {
            if (sideStrings[i] == s || shortSideStrings[i] == s) {
                return static_cast<Side>(i);
            }
        }
        return Side::none;
    }

    std::string chessVariant2String(ChessVariant variant)
    {
        auto t = static_cast<int>(variant);
        if (t < 0 || t > static_cast<int>(ChessVariant::none)) t = 0;
        return variantStrings[t];
    }

    std::string chessVariant2PartlyTitle(ChessVariant variant)
    {
        /// A window title says the variant only when it is not the one this
        /// product is about.
#ifdef _BANKSIA_
        auto show = variant != ChessVariant::standard;
#else
        auto show = variant != ChessVariant::xq;
#endif
        return show ? " - " + chessVariant2String(variant) : "";
    }

    ChessVariant string2ChessVariant(std::string s)
    {
        toLower(s);
        for (auto i = 0; variantStrings[i]; i++) {
            if (variantStrings[i] == s) {
                return static_cast<ChessVariant>(i);
            }
        }

        if (s == "chess 960" || s == "fischerandom" || s == "fische random") {
            return ChessVariant::chess960;
        }
        return ChessVariant::standard;
    }


    std::string score2String(int centiscore, bool pawnUnit, int precision)
    {
        if (pawnUnit) {
            return score2String(static_cast<double>(centiscore) / 100, true, precision);
        }
        return std::to_string(centiscore);
    }

    std::string score2String(double score, bool pawnUnit, int precision)
    {
        if (pawnUnit) {
            std::ostringstream stringStream;
            stringStream << std::fixed << std::setprecision(precision) << std::showpos << score;
            return stringStream.str();
        }
        return score2String(static_cast<int>(score * 100), false, precision);
    }

    // ------------------------------------------------------------------------
    //  Identifiers and console output
    // ------------------------------------------------------------------------

    static std::atomic<int> uniqueIdCounter { 1 };

    int nextUniqueId()
    {
        return uniqueIdCounter.fetch_add(1, std::memory_order_relaxed);
    }

    void noteUniqueIdInUse(int id)
    {
        int current = uniqueIdCounter.load(std::memory_order_relaxed);
        while (id >= current
               && !uniqueIdCounter.compare_exchange_weak(current, id + 1, std::memory_order_relaxed)) {
            /// current was refreshed by the failed exchange; retry.
        }
    }

    static auto& consoleMutex() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
    {
        static std::mutex v;
        return v;
    }

    void printText(const std::string& str)
    {
        std::lock_guard<std::mutex> dolock(consoleMutex());
        std::cout << str << std::endl;
    }

} // namespace banksia
