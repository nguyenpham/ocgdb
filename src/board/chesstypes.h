/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef enginescore_hpp
#define enginescore_hpp

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <assert.h>
#include <mutex>
#include <functional>
#include <cstring>

#include "types.h"
#include "funcs.h"


namespace bslib {

const int MaxMoveBranch = 250;

class Result {
public:
    Result() {
        reset();
    }
    Result(ResultType _result, ReasonType _reason = ReasonType::noreason, std::string _comment = "") {
        result = _result;
        reason = _reason;
        comment = _comment;
    }

    void reset() {
        result = ResultType::noresult;
        reason = ReasonType::noreason;
        comment = "";
    }

    ResultType result;
    ReasonType reason;
    std::string comment;

    bool isNone() const {
        return result == ResultType::noresult;
    }

    std::string reasonString() const;

    std::string toShortString() const;

    std::string toString() const {
        auto str = toShortString();
        if (reason != ReasonType::noreason) {
            str += " (" + reasonString() + ")";
        }
        return str;
    }
};

class Piece {
public:
    int type, idx;
    Side side;

public:
    Piece() {}
    Piece(int _type, Side _side) {
        set(_type, _side);
    }

    static Piece emptyPiece;

    void set(int _type, Side _side) {
        type = _type;
        side = _side;
        assert(isValid());
    }

    void setEmpty() {
        set(EMPTY, Side::none);
    }

    bool isEmpty() const {
        return type == EMPTY;
    }

    bool isPiece(int _type, Side _side) const {
        return type == _type && side == _side;
    }

    bool isValid() const {
        return (side == Side::none && type == EMPTY) || (side != Side::none && type != EMPTY);
    }

    bool operator == (const Piece & o) const {
        return type == o.type && side == o.side;
    }
    bool operator != (const Piece & o) const {
        return type != o.type || side != o.side;
    }

    uint32_t encode() const {
        return type | static_cast<int>(side) << 7;
    }

    static Piece decode(uint32_t u) {
        Piece piece;
        piece.type = u & 0x7f;
        auto sd = u >> 7 & 0x1;
        piece.side = static_cast<Side>(sd);
        return piece;
    }

};

class Move {
public:
    Move() {}
    Move(int from, int dest, int promotion = EMPTY)
    : from(from), dest(dest), promotion(promotion)
    {}
    Move(const std::string& moveCoordinateString, ChessVariant chessVariant);

    static Move illegalMove;

    static bool isValidPromotion(int promotion) {
        return promotion == EMPTY || promotion > KING;
    }

    bool isValid() const {
        return isValid(from, dest);
    }

    static bool isValid(int from, int dest) {
        return from != dest  && from >= 0 && dest >= 0;
    }

    bool operator == (const Move& other) const {
        return from == other.from && dest == other.dest && promotion == other.promotion;
    }

    bool operator != (const Move& other) const {
        return from != other.from || dest != other.dest || promotion != other.promotion;
    }

    std::string toCoordinateString(ChessVariant chessVariant) const;

    uint32_t encodeMove() const {
        return from | dest << 7 | promotion << 14;
    }

    static Move decodeMove(uint32_t u) {
        Move m;
        m.from = u & 0x7f;
        m.dest = u >> 7 & 0x7f;
        m.promotion = u >> 14 & 0xf;
        return m;
    }

public:
    int from, dest;
    int promotion;
};

class MoveFull : public Move {
public:
    Piece piece;
    int score = 0;

    static MoveFull illegalMove;

public:
    MoveFull() {}
    MoveFull(Piece piece, int from, int dest, int promotion = EMPTY)
    : Move(from, dest, promotion), piece(piece)
    {}
    MoveFull(int from, int dest, int promotion = EMPTY)
    : Move(from, dest, promotion)
    {}


    void set(Piece _piece, int _from, int _dest, int _promote = EMPTY) {
        piece = _piece;
        from = _from;
        dest = _dest;
        promotion = _promote;
    }

    void set(int _from, int _dest, int _promote) {
        from = _from;
        dest = _dest;
        promotion = _promote;
    }

    uint32_t encode() const {
        return from | dest << 7 | promotion << 14 | piece.type << 18 | static_cast<int>(piece.side) << 22;
    }

    static MoveFull decode(uint32_t u) {
        MoveFull m;
        m.from = u & 0x7f;
        m.dest = u >> 7 & 0x7f;
        m.promotion = u >> 14 & 0xf;
        m.piece.type = u >> 18 & 0xf;
        auto sd = u >> 22 & 0x1;
        m.piece.side = static_cast<Side>(sd);
        return m;
    }

    bool operator == (const MoveFull& other) const {
        return from == other.from && dest == other.dest && promotion == other.promotion;
    }
    bool operator != (const MoveFull& other) const {
        return from != other.from || dest != other.dest || promotion != other.promotion;
    }
    bool operator == (const Move& other) const {
        return from == other.from && dest == other.dest && promotion == other.promotion;
    }
};

class HistBasic {
public:
    MoveFull move;
    std::string sanString;
};


class EngineScore
{
public:
    int depth, multipv = 1;

    int idNumber = -1;
    int score;
    bool mating;
    int selectiveDepth; //, time = 0;
    int elapsedInMillisecond;

    int64_t nodes, nps, hashKey;
    std::string pv, fen, wdl;
    std::vector<HistBasic> pvhist;

    /// TCEC standard
    int timeleftInMillisecond;
    int drawruleclock, resignruleclock; // r50,
    std::string ponderMoveString, mb;

    /// Not for reseting after everymove
    int hashusage, ponderhitrate, tbhits, nnuehits;
    int stats_win, stats_draw, stats_loss, movesleft;

    HistBasic ponderHist;

public:
    EngineScore();

    void reset();
    void resetAll();
    bool hasWDL() const;

    bool hasMovesLeft() const;
    bool hasNNUE() const;

    void merge(const EngineScore& e);

    bool empty() const;

    std::string toEPDString() const;
    std::string getPVString(Notation notation, bool isChess) const;

    static std::string score2string(int score, bool mating, bool pawnUnit, bool moveByWhite = false, bool scoreInWhiteView = false);

    std::string computingString(CommentComputerInfoType computingInfoType, bool pawnUnit, int quietCnt, bool moveByWhite, bool scoreInWhiteView = false, int precision = 1) const;
    std::string computingString_standard(bool pawnUnit, bool moveByWhite, bool scoreInWhiteView, int precision) const;
    std::string computingString_tcec(int quietCnt, bool moveByWhite) const;

    bool isValid() const;
};

class Hist : public HistBasic {
public:
    Piece cap;
    int enpassant, status, castled;
    int8_t castleRights[2];
    int64_t hashKey;
    int quietCnt;
    std::string comment, fenString; // moveString,
    std::vector<uint64_t> bitboardVec;

    std::vector<EngineScore> esVec;

    void set(const MoveFull& _move) {
        move = _move;
    }

    bool isValid() const {
        return move.isValid() && cap.isValid();
    }

    std::string computingString(CommentComputerInfoType computingInfoType, bool pawnUnit, bool scoreInWhiteView = false, int precision = 1) const {
        std::string str;

        for(auto && es: esVec) {
            if (esVec.size() > 1 && !str.empty()) {
                str += ", ";
            }
            str += es.computingString(computingInfoType, pawnUnit, quietCnt, move.piece.side == Side::white, scoreInWhiteView, precision);

        }
        return str;
    }

    bool isEngineScoreValid() const {
        for(auto && es : esVec) {
            if (!es.isValid()) return false;
        }
        return true;
    }

    // low word is computer info, high word is comment
    int countInfo() const {
        auto k = esVec.empty() ? 0 : 1;
        if (!comment.empty()) k = 1 << 16;
        return k;
    }

    bool removeInfo(bool compInfo, bool bComment) {
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


}

#endif /* enginescore_h */
