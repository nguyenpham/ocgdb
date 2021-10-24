/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <sstream>

#include "chesstypes.h"
#include "chess.h"
#include "funcs.h"

using namespace ocgdb;


std::string Result::reasonString() const
{
    return Funcs::reasonType2String(reason);
}

std::string Result::toShortString() const
{
    return Funcs::resultType2String(result, true);
}

Move::Move(const std::string& moveCoordinateString, ChessVariant chessVariant)
{
    if (Funcs::isChessFamily(chessVariant)) {
        const char* str = moveCoordinateString.c_str();

        from = Funcs::chessCoordinateStringToPos(str);
        dest = Funcs::chessCoordinateStringToPos(str + 2);
        promotion = EMPTY;
        if (moveCoordinateString.length() > 4) {
            char ch = moveCoordinateString.at(4);
            if (ch == '=' && moveCoordinateString.length() > 5) ch = moveCoordinateString.at(5);
            if (ch>='A' && ch <='Z') {
                ch += 'a' - 'A';
            }
            auto t = Funcs::chessCharactorToPieceType(ch);
            if (Move::isValidPromotion(t)) promotion = t;
        }
    } else {
        assert(false);
    }
}

std::string Move::toCoordinateString(ChessVariant chessVariant) const
{
    if (Funcs::isChessFamily(chessVariant)) {
        auto s = Funcs::chessPosToCoordinateString(from) + Funcs::chessPosToCoordinateString(dest);
        if (promotion != EMPTY) {
            s += Funcs::chessPieceType2Char(promotion);
        }

        return s;
    }

//    if (isXqFamily(chessVariant)) {
//        auto s = xqPosToCoordinateString(from) + xqPosToCoordinateString(dest);
//        return s;
//    }

    assert(false);
    return "";
}

EngineScore::EngineScore()
{
    resetAll();
}

/// Reset only a new game
void EngineScore::resetAll()
{
    reset();
    hashusage = ponderhitrate = tbhits = -1;
    nnuehits = 0;
    stats_win = stats_draw = stats_loss = 0;
    movesleft = -1;
    ponderMoveString.clear();
}

/// Reset after each move
void EngineScore::reset()
{
    score = ScoreNull;
    mating = false;
    depth = 0; selectiveDepth = 0; //, time = 0;
    elapsedInMillisecond = 0;

    nodes = nps = 0;
    pv.clear();
    fen.clear(); wdl.clear();
    pvhist.clear();

    /// TCEC standard
    timeleftInMillisecond = 0;
    drawruleclock = resignruleclock = -1; // r50 =
}

bool EngineScore::hasWDL() const
{
    return stats_win + stats_draw + stats_loss > 0;
}

bool EngineScore::hasMovesLeft() const
{
    return movesleft > 0;
}

bool EngineScore::hasNNUE() const
{
    return nnuehits > 0;
}

void EngineScore::merge(const EngineScore& e)
{
    if (e.score != ScoreNull) {
        score = e.score;
        mating = e.mating;
    }
    if (e.depth) depth = e.depth;
    if (e.multipv) multipv = e.multipv;
    if (e.elapsedInMillisecond) elapsedInMillisecond = e.elapsedInMillisecond;
    if (e.nodes) nodes = e.nodes;
    if (e.hashKey) hashKey = e.hashKey;
    if (e.nnuehits) nnuehits = e.nnuehits;
    if (e.movesleft > 0) movesleft = e.movesleft;

    if (!e.pv.empty()) pv = e.pv;
    if (!e.pvhist.empty()) pvhist = e.pvhist;
//    if (e.lcStats.idNumber > 0) {
//        lcStats = e.lcStats;
//    }

    if (e.timeleftInMillisecond) timeleftInMillisecond = e.timeleftInMillisecond;

    if (e.tbhits >= 0) tbhits = e.tbhits;
    if (e.hashusage >= 0) hashusage = e.hashusage;
    if (e.ponderhitrate >= 0) ponderhitrate = e.ponderhitrate;
    if (e.drawruleclock >= 0) drawruleclock = e.drawruleclock;
    if (e.resignruleclock >= 0) resignruleclock = e.resignruleclock;
    if (!e.ponderMoveString.empty()) ponderMoveString = e.ponderMoveString;
}

bool EngineScore::empty() const
{
    return depth <= 0 || (score == ScoreNull && !nodes && pv.empty());
}

bool EngineScore::isValid() const
{
    return idNumber > 0 || depth == 0;
}

std::string EngineScore::toEPDString() const
{
    if (empty()) return "";

    std::string str;

    if (depth > 0) {
        str += " acd " + std::to_string(depth) + ";";
    }
    if (nodes > 0) {
        str += " acn " + std::to_string(nodes) + ";";
    }
    if (elapsedInMillisecond > 0) {
        str += " acs " + std::to_string(elapsedInMillisecond / 1000) + ";";
    }

    if (nps > 0) {
        str += " nps " + std::to_string(nps) + ";";
    }

    if (score != ScoreNull) {
        if (mating) {
            str += " dm " + std::to_string(score) + ";";
        } else {
            str += " ce " + std::to_string(score) + ";";
        }
    }

    if (hasWDL()) {
        str +=  " win " + std::to_string(stats_win)
                + "; draw " + std::to_string(stats_draw)
                + "; loss " + std::to_string(stats_loss)
                + ";";
    }

    // predicted move
    if (!ponderMoveString.empty()) {
        str += " pm " + ponderMoveString + ";";
    }

    if (!pv.empty()) {
        str += " pv " + pv + ";";
    }

    return str;
}

std::string EngineScore::getPVString(Notation notation, bool isChess) const
{
    std::string str;

    for(auto && h : pvhist) {
        if (!str.empty()) str += " ";

        if (isChess) {

#if defined (_CHESS_) || defined (_XCHESS_)
            str += banksia::ChessBoard::hist2String(h, notation);
#endif
        } else {

#ifdef _XIANGQI_
            str += banksia::XqBoard::hist2String(h, notation);
#endif
        }
    }
    return str;
}


std::string EngineScore::computingString(CommentComputerInfoType computingInfoType, bool pawnUnit, int quietCnt, bool moveByWhite, bool scoreInWhiteView, int precision) const
{
    assert(precision >= 1 && precision <= 3);
    if (computingInfoType == CommentComputerInfoType::none || score == ScoreNull) {
        return "";
    }
    if (computingInfoType == CommentComputerInfoType::tcec) {
        return computingString_tcec(quietCnt, moveByWhite);
    }

    return computingString_standard(pawnUnit, moveByWhite, scoreInWhiteView, precision);
}

std::string EngineScore::score2string(int score, bool mating, bool pawnUnit, bool moveByWhite, bool scoreInWhiteView)
{
    std::ostringstream stringStream;

    stringStream << std::fixed;

    if (mating) {
        stringStream << "M" << std::showpos << score;
    } else {
        auto value = score;
        if (scoreInWhiteView && !moveByWhite) {
            value = -value;
        }

        stringStream << Funcs::score2String(value, pawnUnit);
    }
    return stringStream.str();
}

/// score/depth time nodes
std::string EngineScore::computingString_standard(bool pawnUnit, bool moveByWhite, bool scoreInWhiteView, int precision) const
{

    std::ostringstream stringStream;

    auto cnt = 0;

    stringStream << score2string(score, mating, pawnUnit, moveByWhite, scoreInWhiteView);
    cnt++;

    if (depth > 0 || elapsedInMillisecond > 0) {
        stringStream << std::fixed << std::setprecision(precision) << std::noshowpos << "/" << depth
                     << " " << elapsedInMillisecond;
        cnt++;
    }

    if (nodes > 0) {
        stringStream << " " << nodes;
        cnt++;
    }

    if (hasWDL()) {
        if (cnt > 0) {
            stringStream << " ";
        }
        stringStream << stats_win << "/" << stats_draw << "/" << stats_loss;
        cnt++;
    }
    return stringStream.str();
}

std::string EngineScore::computingString_tcec(int quietCnt, bool moveByWhite) const
{

    std::ostringstream stringStream;

    stringStream << "d=" << std::max(1, depth);
    if (selectiveDepth > 0) {
        stringStream << ", sd=" << selectiveDepth;
    }

    if (!ponderMoveString.empty()) {
        stringStream << ", pd=" << ponderMoveString;
    }

    if (elapsedInMillisecond > 0) {
        stringStream << ", mt=" << elapsedInMillisecond;
    }

    if (timeleftInMillisecond > 0) {
        stringStream << ", tl=" << timeleftInMillisecond;
    }

    if (nps > 0) {
        stringStream << ", s=" << nps;
    }

    if (nodes > 0) {
        stringStream << ", n=" << nodes;
    }

    if (!pvhist.empty()) {
        stringStream << ", pv=";// << es.pv;
        auto cnt = 0;
        for(auto && h : pvhist) {
            if (cnt) {
                stringStream << " ";
            }
            stringStream << h.sanString;
            cnt++;
        }
    }

    if (tbhits > 0) {
        stringStream << ", tb=" << tbhits;
    }

    if (hashusage >= 0) {
        stringStream << ", h=";
        if (hashusage == 0) {
            stringStream << "0.0";
        } else {
            stringStream << std::fixed << std::setprecision(1) << static_cast<double>(hashusage) / 10.0;
        }
    }

    if (ponderhitrate >= 0) {
        stringStream << ", ph=";
        if (ponderhitrate == 0) {
            stringStream << "0.0";
        } else {
            stringStream << std::fixed << std::setprecision(1)
                         << static_cast<double>(ponderhitrate) / 10.0;
        }
    }

    stringStream << ", wv=";
    if (score == 0 || score == ScoreNull) {
        stringStream << "0.00";
    } else {
        double value = double(score) / 100;
        if (!moveByWhite) {
            value = -value;
        }

        if (value < 0) {
            value = -value;
            stringStream << "-";
        }

        if (mating) {
            stringStream << "M";
        }

        stringStream << std::fixed << std::setprecision(2) << value; // << std::noshowpos; std::showpos <<
    }


    stringStream << ", r50=" << std::max(0, 100 - quietCnt) / 2;

    if (drawruleclock > 0) {
        stringStream << ", Rd=" << drawruleclock;
    }

    if (resignruleclock > 0) {
        stringStream << ", Rr=" << resignruleclock;
    }

    if (!mb.empty()) {
        stringStream << ", mb=" << mb;
    }

    return stringStream.str();
}
