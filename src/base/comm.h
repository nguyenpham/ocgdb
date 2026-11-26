/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

/*
 The shared vocabulary of the chess and xiangqi families: the process-wide
 settings, the names of things, and the conversions between those names and
 the enums in types.h.

 The headers included below each stand alone; comm.h includes them for
 convenience, and new code should include the specific header it needs.

     strutil.h    text, and time formatted as text
     fileutil.h   files, folders, processes, machine facts
     jsonable.h   Obj, Jsonable, JsonSavable, JsonParsing
     tickable.h   the periodic-housekeeping mixin

 Everything here is pure std C++. Nothing under banksia/ may use Qt -- this
 core is shared with the graphical applications, but it is built here with no
 Qt on the include path at all, and that is what enforces it.
 */

#ifndef BANKSIA_BASE_COMM_H
#define BANKSIA_BASE_COMM_H

#include <cassert>
#include <string>
#include <vector>

#include "types.h"

#include "strutil.h"
#include "fileutil.h"
#include "jsonable.h"
#include "tickable.h"

namespace banksia {

    extern const char* BANKSIA_VERSION;

    std::string getVersion();
    std::string getAppName();

    /// `inline` so there is one object for the whole program. Without it, a
    /// namespace-scope const std::string in a header gives every translation
    /// unit including it a private copy, each with its own static initialiser
    /// to run before main(). It stays a std::string rather than becoming a
    /// const char* because callers concatenate it onto string literals.
#ifdef _WIN32
    inline const std::string folderSlash = "\\";
#else
    inline const std::string folderSlash = "/";
#endif

    // ------------------------------------------------------------------------
    //  Process-wide settings
    //
    //  Globals because they are read from everywhere and set once, at startup,
    //  from the command line or the settings file.
    // ------------------------------------------------------------------------

    /// The family this binary was built for; see types.h.
    extern ChessVariant chessVariant;

    /// True in the console tournament managers, false in the GUI. Guards work
    /// that only a user interface consumes.
    extern bool consoleMode;

    extern bool banksiaVerbose;
    extern bool profileMode;


    /// Whether anything will read EngineScore::pvhist.
    ///
    /// Producing it is the most expensive thing UciEngine::parseInfo() does,
    /// and it does it for every `info` line an engine sends: clone the board
    /// (which carries its move history), recompute the Zobrist key, build a
    /// FEN, then replay the whole principal variation with full legal-move
    /// generation per move. With twelve engines resident that is the bulk of
    /// the manager's CPU.
    ///
    /// A console tournament never consumes the result: the PGN comment it
    /// writes carries score, depth, time and node count, and never a PV.
    bool pvHistoryWanted();

    // ------------------------------------------------------------------------
    //  Move notation
    // ------------------------------------------------------------------------

    /// How pieces and squares are spelled. Xiangqi has two conventions in
    /// common use -- UCCI's Latin letters and the Betza/wiki set that writes
    /// Horse and Elephant -- and boards may number ranks from 0 or from 1.
    class MoveFormat
    {
    public:
        MoveFormat(FENCharactorSet charSet = FENCharactorSet::standard, bool rankFrom1 = false)
            : charSet(charSet), rankFrom1(rankFrom1)
        {
        }

        void setup(XqCharSetStd std, const MoveFormat* customMF)
        {
            switch (std) {
                case XqCharSetStd::ucci:
                    charSet = FENCharactorSet::standard;
                    rankFrom1 = false;
                    break;
                case XqCharSetStd::wiki:
                    charSet = FENCharactorSet::set2;
                    rankFrom1 = false;
                    break;
                default:
                    if (customMF) {
                        charSet = customMF->charSet;
                        rankFrom1 = customMF->rankFrom1;
                    }
                    break;
            }
        }

        XqCharSetStd getStd() const
        {
            if (rankFrom1) {
                return XqCharSetStd::custom;
            }
            return charSet == FENCharactorSet::standard ? XqCharSetStd::ucci : XqCharSetStd::wiki;
        }

        FENCharactorSet charSet;
        bool rankFrom1;

        char xqTra_advance = '+';
        char xqTra_backward = '-';
        char xqTra_sideway = '=';
        char xqTra_jieqi_promote = '*';
    };

    extern MoveFormat moveFormat;
    extern MoveFormat moveFormat_ucci;
    extern MoveFormat moveFormat_wiki;

    // ------------------------------------------------------------------------
    //  Identifiers
    // ------------------------------------------------------------------------

    /// Monotonic, process-wide identifiers. Game::idNumber is a map key in
    /// GameCallback and MatchRecord::pairId groups the game pairs of the
    /// pentanomial statistics, so two live ones must never collide.
    ///
    /// noteUniqueIdInUse() keeps the counter above ids read back from a saved
    /// tournament, so a resumed run cannot reissue one that is already live.
    int nextUniqueId();
    void noteUniqueIdInUse(int id);

    // ------------------------------------------------------------------------
    //  Board vocabulary
    // ------------------------------------------------------------------------

    /// Chess squares, in the a1 = 0 ... h8 = 63 order python-chess uses.
    int chessCoordinateStringToPos(const std::string& str);
    std::string chessPosToCoordinateString(int pos);
    char chessPieceType2Char(int pieceType);
    int chessCharactorToPieceType(char ch);

    std::string xqPosToCoordinateString(int pos, const MoveFormat& mf);
    char xqPieceType2Char(int pieceType, const MoveFormat& mf);
    char xqPieceType2Char(int pieceType);

    /// Accepts either xiangqi character set: the wiki set's 'e' (elephant) and
    /// 'h' (horse) are folded onto the standard 'b' and 'n'. Returns EMPTY for
    /// anything that is not a piece letter.
    int xqCharactorToPieceType(char ch);

    bool isVariantValid(ChessVariant variant);
    bool isChessFamily(ChessVariant variant);
    bool isXqFamily(ChessVariant variant);

    /// The starting position of a variant, and the bare-kings position used
    /// when the board is cleared. All three return an empty string or 0, not
    /// garbage, for a variant index this binary has no entry for.
    std::string getOriginFen(ChessVariant variant);
    std::string getEmptyFen(ChessVariant variant);
    uint64_t getOriginHashKey(ChessVariant variant);

    // ------------------------------------------------------------------------
    //  Names of things
    // ------------------------------------------------------------------------

    extern const char* reasonStrings[];

    std::string resultType2String(ResultType type, bool shortFrom = false);
    ResultType string2ResultType(const std::string& s);

    std::string reasonType2String(ReasonType type);
    ReasonType string2ReasonType(const std::string& s);

    std::string side2String(Side side, bool shortFrom = true);
    Side string2Side(std::string s);

    std::string chessVariant2String(ChessVariant variant);
    std::string chessVariant2PartlyTitle(ChessVariant variant);
    ChessVariant string2ChessVariant(std::string s);


    /// An engine score as text. `pawnUnit` divides by 100 and always shows a
    /// sign, which is how scores are written in PGN comments.
    std::string score2String(int centiscore, bool pawnUnit, int precision = 2);
    std::string score2String(double score, bool pawnUnit, int precision = 2);

    // ------------------------------------------------------------------------
    //  Console output
    // ------------------------------------------------------------------------

    /// One line to stdout, serialised against other threads doing the same.
    void printText(const std::string& str);

} // namespace banksia

#endif /* BANKSIA_BASE_COMM_H */
