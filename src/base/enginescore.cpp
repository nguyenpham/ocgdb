/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "enginescore.h"

using namespace banksia;

EngineScore::EngineScore()
{
    resetAll();
}

bool EngineScore::load(const Json::Value& v)
{
    idNumber = v["id"].asInt();
    score = v["s"].asInt();
    mating = v["mating"].asBool();
    depth = v["d"].asInt();
    selectiveDepth = v["sd"].asInt();
    elapsedInMillisecond = v["t"].asInt();

    nodes = v["n"].asInt64();
    nps = v["nps"].asInt64();

    hashusage = v["hu"].asInt();
    ponderhitrate = v["pr"].asInt();
    tbhits = v["tbhits"].asInt();
    nnuehits = v["nnuehits"].asInt();
    stats_win = v["win"].asInt();
    stats_draw = v["draw"].asInt();
    stats_loss = v["loss"].asInt();

    pvhist.clear();
    auto vh = v["pv"];
    for (auto it = vh.begin(); it != vh.end(); ++it) {
        auto m = (*it)["m"].asInt();
        Hist hist;
        hist.move = MoveFull::decode(m);
        hist.sanString = (*it)["s"].asString();
        pvhist.push_back(hist);
    }

    return true;
}

Json::Value EngineScore::saveToJson() const
{
    Json::Value v;
    v["id"] = idNumber;
    v["s"] = score;
    v["mating"] = mating;
    v["d"] = depth;
    v["sd"] = selectiveDepth;
    v["t"] = elapsedInMillisecond;

    v["n"] = nodes;
    v["nps"] = nps;

    v["hu"] = hashusage;
    v["pr"] = ponderhitrate;
    v["tbhits"] = tbhits;
    v["nnuehits"] = nnuehits;
    v["win"] = stats_win;
    v["draw"] = stats_draw;
    v["loss"] = stats_loss;

    Json::Value array;
    for (auto&& hist : pvhist) {
        Json::Value h;
        h["m"] = hist.move.encode();
        h["s"] = hist.sanString;
        array.append(h);
    }
    v["pv"] = array;
    return v;
}


/// Reset only a new game
void EngineScore::resetAll()
{
    reset();
    hashusage = ponderhitrate = tbhits = -1;
    nnuehits = 0;
    stats_win = stats_draw = stats_loss = 0;
    movesleft = -1;

    currMoveHist.reset();
}

/// Reset after each move
void EngineScore::reset()
{
    score = ScoreNull;
    mating = false;
    depth = 0;
    selectiveDepth = 0; //, time = 0;
    elapsedInMillisecond = 0;

    nodes = nps = 0;
    pv.clear();
    fen.clear();
    wdl.clear();
    pvhist.clear();

    /// TCEC standard
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


    if (e.tbhits >= 0) tbhits = e.tbhits;
    if (e.hashusage >= 0) hashusage = e.hashusage;
    if (e.ponderhitrate >= 0) ponderhitrate = e.ponderhitrate;
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
        str += " win " + std::to_string(stats_win) + "; draw " + std::to_string(stats_draw) + "; loss "
               + std::to_string(stats_loss) + ";";
    }

    if (!pv.empty()) {
        str += " pv " + pv + ";";
    }

    return str;
}


void EngineScore::addToEPDMap(std::map<std::string, std::string>& theMap) const
{
    if (empty()) return;

    if (depth > 0) {
        theMap["acd"] = std::to_string(depth);
    }
    if (nodes > 0) {
        theMap["acn"] = std::to_string(nodes);
    }
    if (elapsedInMillisecond > 0) {
        theMap["acs"] = std::to_string(elapsedInMillisecond / 1000);
    }

    if (nps > 0) {
        theMap["nps"] = std::to_string(nps);
    }

    if (score != ScoreNull) {
        if (mating) {
            theMap["dm"] = std::to_string(score);
        } else {
            theMap["ce"] = std::to_string(score);
        }
    }

    if (hasWDL()) {
        theMap["win"] = std::to_string(stats_win);
        theMap["draw"] = std::to_string(stats_draw);
        theMap["loss"] = std::to_string(stats_loss);
    }

    if (!pv.empty()) {
        theMap["pv"] = pv;
    }
}


std::string EngineScore::score2string(int score, bool mating, bool pawnUnit, bool moveByWhite,
                                      bool scoreInWhiteView, int precision)
{
    if (score == banksia::ScoreNull) return "";

    std::ostringstream stringStream;

    stringStream << std::fixed;

    if (mating) {
        stringStream << "M" << std::showpos << score;
    } else {
        auto value = score;
        if (scoreInWhiteView && !moveByWhite) {
            value = -value;
        }

        stringStream << score2String(value, pawnUnit, precision);
    }
    return stringStream.str();
}

std::string EngineScore::score2string(bool pawnUnit, bool moveByWhite, bool scoreInWhiteView, int precision)
{
    return score2string(score, mating, pawnUnit, moveByWhite, scoreInWhiteView, precision);
}

/// score/depth time nodes
std::string EngineScore::computingString(bool pawnUnit, bool moveByWhite, bool scoreInWhiteView,
                                         int precision, bool humanEasy) const
{
    std::ostringstream stringStream;

    auto cnt = 0;

    stringStream << score2string(score, mating, pawnUnit, moveByWhite, scoreInWhiteView, precision);
    cnt++;

    if (depth > 0 || elapsedInMillisecond > 0) {
        stringStream << std::fixed << std::setprecision(precision) << std::noshowpos << "/" << depth << " ";

        if (humanEasy && elapsedInMillisecond > 10000) {
            stringStream << secondToClockString(elapsedInMillisecond / 1000);
        } else {
            stringStream << elapsedInMillisecond;
        }
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
