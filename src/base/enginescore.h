/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_ENGINESCORE_H
#define BANKSIA_BASE_ENGINESCORE_H

#include <memory>
#include <stdio.h>
#include <stdint.h>

#include "base/comm.h"


namespace banksia {

    enum class LogType { toEngine, fromEngine, system, error };

    enum class EngineScoreType { standard, special, string, max };

    const int Chess_MaxMoveNumber = 250;

    class Result
    {
    public:
        Result() { reset(); }
        Result(ResultType _result, ReasonType _reason = ReasonType::noreason, std::string _comment = "")
        {
            result = _result;
            reason = _reason;
            comment = _comment;
        }

        void reset()
        {
            result = ResultType::noresult;
            reason = ReasonType::noreason;
            comment = "";
        }

        ResultType result;
        ReasonType reason;
        std::string comment;

        bool isNone() const { return result == ResultType::noresult; }

        void setReason(const std::string& s)
        {
            reason = banksia::string2ReasonType(s);
            if (reason == ReasonType::extracomment && comment.empty()) {
                comment = s;
            }
        }

        std::string reasonString() const
        {
            if (reason == ReasonType::extracomment) {
                return comment;
            }
            return reasonStrings[static_cast<int>(reason)];
        }

        std::string toShortString() const { return resultType2String(result); }

        std::string toString() const
        {
            auto str = toShortString();
            if (reason != ReasonType::noreason) {
                str += " (" + reasonString() + ")";
            }
            return str;
        }
    };

    class Piece
    {
    public:
        // Default-initialised to a valid empty piece.
        int type = EMPTY;
        int idx = -1;
        Side side = Side::none;
        int cover = EMPTY;

        Piece() { }
        Piece(int _type, Side _side, int _cover = EMPTY) { set(_type, _side, _cover); }

        static Piece emptyPiece;

        void set(int _type, Side _side, int _cover = EMPTY)
        {
            type = _type;
            side = _side;
            cover = _cover;
            assert(isValid());
        }

        void setEmpty() { set(EMPTY, Side::none, EMPTY); }

        bool isEmpty() const { return type == EMPTY; }

        bool isPiece(int _type, Side _side) const { return type == _type && side == _side; }

        bool isValid() const
        {
            return (side == Side::none && type == EMPTY)
                   || ((side == Side::white || side == Side::black) && type > EMPTY && type <= COVER
                       && cover != KING);
        }

        bool operator==(const Piece& o) const { return type == o.type && side == o.side && cover == o.cover; }
        bool operator!=(const Piece& o) const { return type != o.type || side != o.side || cover != o.cover; }
    };

    class Move
    {
    public:
        Move() { }
        Move(int from, int dest, int promotion = EMPTY) : from(from), dest(dest), promotion(promotion) { }
        Move(const std::string& moveCoordinateString, ChessVariant chessVariant)
        {
            if (isChessFamily(chessVariant)) {
                const char* str = moveCoordinateString.c_str();

                from = chessCoordinateStringToPos(str);
                dest = chessCoordinateStringToPos(str + 2);
                promotion = EMPTY;
                if (moveCoordinateString.length() > 4) {
                    char ch = moveCoordinateString.at(4);
                    if (ch == '=' && moveCoordinateString.length() > 5) ch = moveCoordinateString.at(5);
                    if (ch >= 'A' && ch <= 'Z') {
                        ch += 'a' - 'A';
                    }
                    auto t = chessCharactorToPieceType(ch);
                    if (Move::isValidPromotion(t)) promotion = t;
                }
            } else {
                assert(false);
            }
        }

        static Move illegalMove;

        static bool isValidPromotion(int promotion)
        {
            return promotion == EMPTY || (promotion > KING && promotion <= PAWNXQ);
        }

        bool isValid() const { return isValid(from, dest); }

        static bool isValid(int from, int dest) { return from != dest && from >= 0 && dest >= 0; }

        bool operator==(const Move& other) const
        {
            return from == other.from && dest == other.dest && promotion == other.promotion;
        }

        bool operator!=(const Move& other) const
        {
            return from != other.from || dest != other.dest || promotion != other.promotion;
        }

        std::string toCoordinateString(ChessVariant chessVariant, MoveFormat mf,
                                       bool horizontalFlip = false) const
        {
            if (isChessFamily(chessVariant)) {
                auto f = horizontalFlip ? from + 7 - 2 * (from % 8) : from;
                auto d = horizontalFlip ? dest + 7 - 2 * (dest % 8) : dest;
                auto s = chessPosToCoordinateString(f) + chessPosToCoordinateString(d);
                if (promotion != EMPTY) {
                    s += chessPieceType2Char(promotion);
                }

                return s;
            }

            if (isXqFamily(chessVariant)) {
                auto f = horizontalFlip ? from + 8 - 2 * (from % 9) : from;
                auto d = horizontalFlip ? dest + 8 - 2 * (dest % 9) : dest;
                auto s = xqPosToCoordinateString(f, mf) + xqPosToCoordinateString(d, mf);

                if (chessVariant == ChessVariant::jieqi && promotion != EMPTY) {
                    s += xqPieceType2Char(promotion);
                }
                return s;
            }

            assert(false);
            return "";
        }


    public:
        int from, dest;
        int promotion;
    };

    class MoveFull : public Move
    {
    public:
        Piece piece;
        int score = 0;

        static MoveFull illegalMove;

        MoveFull() { }
        MoveFull(Piece piece, int from, int dest, int promotion = EMPTY)
            : Move(from, dest, promotion), piece(piece)
        {
        }
        MoveFull(int from, int dest, int promotion = EMPTY) : Move(from, dest, promotion) { }

        void set(Piece _piece, int _from, int _dest, int _promote = EMPTY)
        {
            piece = _piece;
            from = _from;
            dest = _dest;
            promotion = _promote;
        }

        void set(int _from, int _dest, int _promote)
        {
            from = _from;
            dest = _dest;
            promotion = _promote;
        }

        uint32_t encode() const
        {
            return from | dest << 7 | promotion << 14 | piece.type << 18 | static_cast<int>(piece.side) << 22;
        }

        bool hasPromotion() const { return promotion > KING; }

        static MoveFull decode(uint32_t u)
        {
            MoveFull m;
            m.from = u & 0x7f;
            m.dest = u >> 7 & 0x7f;
            m.promotion = u >> 14 & 0xf;
            m.piece.type = u >> 18 & 0xf;
            auto sd = u >> 22 & 0x1;
            m.piece.side = static_cast<Side>(sd);
            return m;
        }

        bool operator==(const MoveFull& other) const
        {
            return from == other.from && dest == other.dest && promotion == other.promotion;
        }
        bool operator!=(const MoveFull& other) const
        {
            return from != other.from || dest != other.dest || promotion != other.promotion;
        }
        bool operator==(const Move& other) const
        {
            return from == other.from && dest == other.dest && promotion == other.promotion;
        }
    };

    class HistBasic
    {
    public:
        MoveFull move;
        std::string sanString, sanIncheckSym;
        bool sanAmbiguous, sanSameCol, sanSamRow;
    };


    /// A member that is allocated only once something writes to it.
    ///
    /// Reading never allocates: an unwritten value reads as a default-constructed
    /// T, which is exactly what a plain member would have held, so `empty()`-style
    /// checks behave identically. Writing allocates once.
    ///
    /// Used for the parts of a game record that are large, optional, and touched
    /// by only one consumer -- the coach's two EngineScores in Hist, and the
    /// currmove/ponder HistBasics in EngineScore. Those are needed by a GUI that
    /// draws them live; a console tournament never reads them, and in a long run
    /// they were the largest single source of retained memory.
    template<class T> class Lazy
    {
    public:
        Lazy() = default;
        Lazy(Lazy&&) noexcept = default;
        Lazy& operator=(Lazy&&) noexcept = default;

        Lazy(const Lazy& other)
        {
            if (other.value_) value_.reset(new T(*other.value_));
        }
        Lazy& operator=(const Lazy& other)
        {
            if (this != &other) {
                if (other.value_)
                    value_.reset(new T(*other.value_));
                else
                    value_.reset();
            }
            return *this;
        }
        Lazy& operator=(const T& value)
        {
            ref() = value;
            return *this;
        }

        const T& get() const
        {
            static const T
                    none { }; /// value-initialised: aggregates like HistBasic have no user-provided ctor
            return value_ ? *value_ : none;
        }
        operator const T&() const { return get(); }
        const T* operator->() const { return &get(); }

        T& ref()
        {
            if (!value_) value_.reset(new T());
            return *value_;
        }
        T* operator->() { return &ref(); }

        void reset() { value_.reset(); }
        bool isSet() const { return static_cast<bool>(value_); }

    private:
        std::unique_ptr<T> value_;
    };

    /// info depth 24 seldepth 33 multipv 4 score cp 29 wdl 75 902 23 nodes 28618356 nps 802781 hashfull 1000 tbhits 0 time 35649 pv c2c4 e7e5 g2g3 c7c6 d2d4 e5d4 d1d4 d7d5 g1f3 g8f6 c4d5 c6d5 f1g2 b8c6 d4d3 f8b4 c1d2 b4d2 b1d2 e8g8 e1g1 f8e8 f1d1 c8g4 a1c1 h7h6 a2a3 a8c8

    class EngineScore : public Jsonable
    {
    public:
        int depth, multipv = 1;

        int idNumber = -1;
        int score;
        bool mating;
        int selectiveDepth; //, time = 0;
        int elapsedInMillisecond;

        i64 nodes, nps, hashKey;
        std::string pv, fen, wdl;
        std::vector<HistBasic> pvhist;


        std::string currMove;
        Lazy<HistBasic> currMoveHist;

        /// Not for reseting after everymove
        int hashusage, ponderhitrate, tbhits, nnuehits;
        int stats_win, stats_draw, stats_loss, movesleft;

        Lazy<HistBasic> ponderHist;

        EngineScore();

        virtual bool load(const Json::Value& obj) override;
        virtual Json::Value saveToJson() const override;

        void reset();
        void resetAll();
        bool hasWDL() const;

        bool hasMovesLeft() const;
        bool hasNNUE() const;

        void merge(const EngineScore& e);

        bool empty() const;

        std::string toEPDString() const;
        void addToEPDMap(std::map<std::string, std::string>& theMap) const;


        static std::string score2string(int score, bool mating, bool pawnUnit, bool moveByWhite = false,
                                        bool scoreInWhiteView = false, int precision = 2);
        std::string score2string(bool pawnUnit, bool moveByWhite = false, bool scoreInWhiteView = false,
                                 int precision = 2);

        /// The engine's score, depth, time and node count, as it appears in a PGN
        /// comment
        std::string computingString(bool pawnUnit, bool moveByWhite, bool scoreInWhiteView = false,
                                    int precision = 1, bool humanEasy = false) const;

        bool isValid() const override;
    };

    class EngineLog
    {
    public:
        EngineLog() { }
        EngineLog(const std::string& name, const std::string& line, LogType logType)
            : name(name), line(line), logType(logType)
        {
        }

        std::string name, line;
        LogType logType;
    };

    /// OCGDB addition (from Banksia GUI's core): where a move stands in the tree of a game's
    /// variations. Level 0 is the main line; a variation one level up, and groupIdx tells apart
    /// the variations that start at the same level. startNewLevel marks a variation's first move.
    class VariationRec
    {
    public:
        u8 level = 0, groupIdx = 0, startNewLevel = 0;

        bool operator==(const VariationRec& o) const { return level == o.level && groupIdx == o.groupIdx; }

        bool operator!=(const VariationRec& o) const { return level != o.level || groupIdx != o.groupIdx; }
    };


    class Hist : public HistBasic
    {
    public:
        const static int MoveFlag_Promotion = 1 << 0;
        const static int MoveFlag_CapEnpassant = 1 << 1;
        const static int MoveFlag_Castled = 1 << 2;

        Piece cap;
        /// castled must start at 0. The xiangqi board has no castling and never assigns it
        int enpassant = -1, status = 0, castled = 0;
        int8_t castleRights[2];
        u64 hashKey;
        int quietCnt, flag, plyCounter = -1;
        std::string comment;
        std::string fenString;


        std::vector<EngineScore> esVec;

        /// OCGDB addition: the variation this move belongs to, and the pieces of the board
        /// right after the board went to that variation's level (BoardCore::_checkMake)
        VariationRec variationRec;
        std::vector<Piece> variationPieces;

        void set(const MoveFull& _move) { move = _move; }

        bool isValid() const { return move.isValid() && cap.isValid(); }

        std::string computingString(bool pawnUnit, bool scoreInWhiteView = false, int precision = 1,
                                    bool humanEasy = false) const
        {
            std::string str;

            for (auto&& es : esVec) {
                if (!str.empty()) {
                    str += ", ";
                }
                str += es.computingString(pawnUnit, move.piece.side == banksia::Side::white, scoreInWhiteView,
                                          precision, humanEasy);
            }
            return str;
        }

        bool isEngineScoreValid() const
        {
            for (auto&& es : esVec) {
                if (!es.isValid()) return false;
            }
            return true;
        }

        // low word is computer info, high word is comment
        int countInfo() const
        {
            auto k = esVec.empty() ? 0 : 1;
            if (!comment.empty()) k = 1 << 16;
            return k;
        }

        bool removeInfo(bool compInfo, bool bComment)
        {
            auto r = false;
            if (compInfo && !esVec.empty()) {
                esVec.clear();
                r = true;
            }
            if (bComment && !comment.empty()) {
                comment = "";
                r = true;
            }

            return r;
        }
    };


    class Eco
    {
    public:
        std::string ecoString, openingString, variationString;

        int length = 0, ecoSubIdx = -1;
        uint64_t hash;

        uint64_t flipHash = 0; /// xiangqi: the key of the mirrored position

        Eco() { }

        Eco(const std::string& _ecoString, int _ecoSubIdx, const std::string& _openingString,
            const std::string& _variationString, uint64_t _hash, uint64_t _flipHash = 0)
        {
            ecoString = _ecoString;
            openingString = _openingString;
            variationString = _variationString;
            ecoSubIdx = _ecoSubIdx;
            hash = _hash;
            flipHash = _flipHash;
        }

        void clear()
        {
            ecoString.clear();
            openingString.clear();
            variationString.clear();
            length = 0;
        }


        bool smaller(const Eco& o) const
        {
            if (ecoString > o.ecoString) {
                return true;
            }
            if (ecoString == o.ecoString) {
                return openingString > o.openingString
                       || (openingString == o.openingString && variationString > o.variationString);
            }

            return false;
        }

        bool isEmpty() const { return ecoString.empty(); }


        std::string toBriefString() const
        {
            if (isEmpty()) {
                return "";
            }

            auto s = ecoString + ": " + openingString;
            if (!variationString.empty()) {
                s += ";" + variationString;
            }

            return s;
        }
    };

}

#endif /* BANKSIA_BASE_ENGINESCORE_H */
