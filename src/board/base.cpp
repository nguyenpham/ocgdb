/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <chrono>
#include <map>
#include <sstream>

#include "base.h"
#include "chess.h"

#include "funcs.h"

using namespace ocgdb;

Piece Piece::emptyPiece(0, Side::none);
Move Move::illegalMove(-1, -1);
MoveFull MoveFull::illegalMove(-1, -1);


bool BoardCore::fromOriginPosition() const
{
    return startFen.empty();
}

std::string BoardCore::getStartingFen(FENCharactorSet) const
{
    return startFen;
}

std::string BoardCore::getFen(FENCharactorSet) const
{
    auto k = std::max<int>(fullMoveCnt, (histList.size() + 1) / 2);
    return getFen(quietCnt / 2, k);
}

std::string BoardCore::getEPD(FENCharactorSet theSet) const
{
    auto hist = histList.empty() ? Hist() : getLastHist();
    return getEPD(hist, theSet);
}

std::string BoardCore::getEPD(const Hist& hist, FENCharactorSet theSet) const
{
    auto str = getFen(-1, -1, theSet);

    auto k = std::max<int>(fullMoveCnt, (histList.size() + 1) / 2);

    // hmvc halfmove clock, fmvn fullmove number
    str += " hmvc " + std::to_string(quietCnt / 2) + "; fmvn " + std::to_string(k) + ";";

    if (!hist.esVec.empty()) {
        auto es = hist.esVec.begin();
        str += es->toEPDString();
    }

    return str;
}

void BoardCore::setFen(const std::string& fen)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _setFen(fen);
}

MoveFull BoardCore::createFullMove(int from, int dest, int promotion) const
{
    MoveFull move(from, dest, promotion);
    if (isPositionValid(from)) {
        move.piece = _getPiece(from);
    }
    return move;
}

std::string BoardCore::getUciPositionString(const Move& pondermove, FENCharactorSet charSet) const
{
    std::string str = "position " + (fromOriginPosition() ? "startpos" : ("fen " + getStartingFen(charSet)));

    if (!histList.empty()) {
        str += " moves";
        for(auto && hist : histList) {
            str += " " + toString_coordinate(hist.move);
        }
    }

    if (pondermove.isValid()) {
        if (histList.empty()) {
            str += " moves";
        }
        str += " " + toString(pondermove);
    }
    return str;
}

Move BoardCore::moveFromString_coordinate(const std::string& moveString) const
{
    if (moveString.length() < 4) return Move::illegalMove;

    auto str = moveString.substr(2);
    int from = coordinateStringToPos(moveString);
    int dest = coordinateStringToPos(str);
    
    auto promotion = EMPTY;
    if (moveString.length() > 4) {
        char ch = moveString.at(4);
        if (ch == '=' && moveString.length() > 5) ch = moveString.at(5);
        if (ch>='A' && ch <='Z') {
            ch += 'a' - 'A';
        }
        auto t = charactorToPieceType(ch);
        if (Move::isValidPromotion(t)) promotion = t;
    }
    
    return Move(from, dest, promotion);
}

void BoardCore::newGame(std::string fen)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _newGame(fen);
}

void BoardCore::_newGame(std::string fen)
{
    histList.clear();
    _setFen(fen);
    result.reset();

    assert(_isHashKeyValid());
}

void BoardCore::setupPieceIndexes()
{
    auto idx = 0;
    for(auto && piece : pieces) {
        if (piece.isEmpty()) continue;
        piece.idx = idx++;
    }
}


bool BoardCore::isHashKeyValid()
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _isHashKeyValid();
}

bool BoardCore::_isHashKeyValid()
{
    auto hk = initHashKey();
    if (hashKey == hk) {
        return true;
    }
    return false;
}

void BoardCore::setHashKey(uint64_t key)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    hashKey = key;
}

int BoardCore::attackerCnt() const
{
    auto cnt = 0;
    for(auto && piece : pieces) {
        if (!piece.isEmpty() && piece.type != KING) cnt++;
    }
    return cnt;
}

bool BoardCore::isLegalMove(int from, int dest, int promotion)
{
    if (!MoveFull::isValid(from, dest)) {
        return false;
    }

    std::lock_guard<std::mutex> dolock(dataMutex);
    return _isLegalMove(from, dest, promotion);
}

bool BoardCore::_isLegalMove(int from, int dest, int promotion)
{
    auto piece = _getPiece(from), cap = _getPiece(dest);

    if (piece.isEmpty()
        || piece.side != side
        || (piece.side == cap.side && (variant != ChessVariant::chess960 || piece.type != KING || cap.type != static_cast<int>(PieceTypeStd::rook)))
        || (promotion > KING && !Move::isValidPromotion(promotion))) {
        return false;
    }
    
    std::vector<MoveFull> moveList;
    _genLegal(moveList, piece.side, from, dest, promotion);
    return !moveList.empty();
}

void BoardCore::gen(std::vector<MoveFull>& moveList, Side attackerSide) const
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _gen(moveList, attackerSide);
}

void BoardCore::genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _genLegalOnly(moveList, attackerSide);
}

void BoardCore::_genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide)
{
    _gen(moveList, attackerSide);
    
    std::vector<MoveFull> moves;
    Hist hist;
    for (auto && move : moveList) {
        _make(move, hist);
        if (!_isIncheck(attackerSide)) {
            moves.push_back(move);
        }
        _takeBack(hist);
    }
    moveList = moves;
}

void BoardCore::genLegal(std::vector<MoveFull>& moves, Side side, int from, int dest, int promotion)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _genLegal(moves, side, from, dest, promotion);
}

void BoardCore::_genLegal(std::vector<MoveFull>& moves, Side side, int from, int dest, int promotion)
{
    std::vector<MoveFull> moveList;
    _gen(moveList, side);
    
    Hist hist;

    auto isChess = Funcs::isChessFamily(variant);

    for (auto && move : moveList) {
        
        if ((from >= 0 && move.from != from)
            || (dest >= 0 && move.dest != dest)
            || (isChess && promotion > KING && promotion != move.promotion)
            ) {
            continue;
        }
        
        _make(move, hist);
        if (!_isIncheck(side)) {
            moves.push_back(move);
        }
        _takeBack(hist);
    }
}

bool BoardCore::checkMake(int from, int dest, int promotion)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _checkMake(from, dest, promotion);
}

bool BoardCore::isIncheck(Side beingAttackedSide) const
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _isIncheck(beingAttackedSide);
}

int BoardCore::findKing(Side side) const
{
    for (int pos = 0; pos < int(pieces.size()); ++pos) {
        if (_isPiece(pos, KING, side)) {
            return pos;
        }
    }
    return -1;
}

void BoardCore::make(const MoveFull& move)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _make(move);
}

void BoardCore::takeBack()
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _takeBack();
}

void BoardCore::make(const MoveFull& move, Hist& hist)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _make(move, hist);
}

void BoardCore::takeBack(const Hist& hist)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _takeBack(hist);
}

void BoardCore::_make(const MoveFull& move)
{
    Hist hist;
    _make(move, hist);
    histList.push_back(hist);
    side = xSide(side);
    
    hashKey ^= xorSideHashKey();
    
    assert(_isHashKeyValid());
}

void BoardCore::_takeBack()
{
    if (!histList.empty()) {
        auto hist = histList.back();
        histList.pop_back();
        _takeBack(hist);
        side = xSide(side);
        assert(_isHashKeyValid());
    }
}

std::chrono::milliseconds::rep now()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::steady_clock::now().time_since_epoch()).count();
}


Move BoardCore::flip(const Move& move, FlipMode flipMode) const
{
    auto m = move;
    m.from = int8_t(flip(m.from, flipMode));
    m.dest = int8_t(flip(m.dest, flipMode));
    return m;
}

MoveFull BoardCore::flip(const MoveFull& move, FlipMode flipMode) const
{
    auto m = move;
    m.from = int8_t(flip(m.from, flipMode));
    m.dest = int8_t(flip(m.dest, flipMode));
    return m;
}

static const FlipMode flipflip_h[] = { FlipMode::horizontal, FlipMode::none, FlipMode::rotate, FlipMode::vertical };
static const FlipMode flipflip_v[] = { FlipMode::vertical, FlipMode::rotate, FlipMode::none, FlipMode::horizontal };
static const FlipMode flipflip_r[] = { FlipMode::rotate, FlipMode::vertical, FlipMode::horizontal, FlipMode::none };

FlipMode BoardCore::flip(FlipMode oMode, FlipMode flipMode)
{
    switch (flipMode) {
        case FlipMode::none:
            break;
        case FlipMode::horizontal:
            return flipflip_h[static_cast<int>(oMode)];
            
        case FlipMode::vertical:
            return flipflip_v[static_cast<int>(oMode)];
        case FlipMode::rotate:
            return flipflip_r[static_cast<int>(oMode)];
    }
    return oMode;
}

std::string BoardCore::toString(const Hist& hist, Notation notation) const
{
    switch (notation) {
    case Notation::san:
        return toString_san(hist);
    case Notation::lan:
        return toString_lan(hist);
    case Notation::algebraic_coordinate:
        return toString_coordinate(hist.move);
    default:
        break;
    }
    return toString_traditional(hist.move, notation);
}

std::string BoardCore::toString_coordinate(const MoveFull& move) const
{
    return toString(move);
}

std::string BoardCore::toString_traditional(const MoveFull& move, Notation notation) const
{
    assert(false);
    return toString(move);
}

std::string BoardCore::toString_san(const Hist& hist) const
{
    return hist.sanString;
}

std::string BoardCore::toString_lan(const Hist& hist) const
{
    return toString_lan(hist.move, variant);
}

std::string BoardCore::toString_lan(const MoveFull& move, ChessVariant variant)
{
    std::string str;

    auto isChess = Funcs::isChessFamily(variant);
    if (isChess && move.piece.type != PAWNSTD) {
        str += Funcs::chessPieceType2Char(move.piece.type) + ('A' - 'a');
    }

    if (isChess) {
        str += Funcs::chessPosToCoordinateString(move.from) + Funcs::chessPosToCoordinateString(move.dest);
    } else {
        assert(false);
    }

    if (move.promotion) {
        str += "=";
        str += Funcs::chessPieceType2Char(move.promotion);
    }
    return str;
}

void BoardCore::_parseComment(const std::string& comment, Hist& hist)
{
    if (       comment.find('=') != std::string::npos
            && comment.find("d=") != std::string::npos
            && comment.find("n=") != std::string::npos) {
        _parseComment_tcec(comment, hist);
    } else {
        _parseComment_standard(comment, hist);
    }
}

static int tcecTimeStringToMillisecond(const std::string& str) {
    auto v = Funcs::splitString(str, ':');
    if (v.size() >= 3) {
        auto h = std::max<int>(0, std::atoi(v.at(0).c_str()));
        auto m = std::max<int>(0, std::atoi(v.at(1).c_str()));
        auto s = std::max<int>(0, std::atoi(v.at(2).c_str()));
        return (h * 60 * 60 + m * 60 + s) * 1000;
    }
    return std::max<int>(0, std::atoi(str.c_str()));
}

void BoardCore::_parseComment_tcec(const std::string& comment, Hist& hist)
{
    auto commentVec = Funcs::splitString(comment, ',');
    EngineScore es;
    for(auto && s : commentVec) {
        auto p = s.find('=');
        if (p == std::string::npos) continue;

        auto s0 = s.substr(0, p);
        auto s1 = s.substr(p + 1);

        if (s0.empty() || s1.empty()) continue;

        if (s0 == "d") {
            es.depth = std::max<int>(1, std::atoi(s1.c_str()));
        }
        else if (s0 == "sd") {
            es.selectiveDepth = std::max<int>(1, std::atoi(s1.c_str()));
        }
        else if (s0 == "n") {
            es.nodes = std::max<int64_t>(0, std::atoll(s1.c_str()));
        }
        else if (s0 == "pv") {
            if (!s1.empty()) {
                auto n = histList.size();
                auto pvhist = _parsePv(s1, false);
                assert(n <= histList.size());
                while (n < histList.size()) {
                    _takeBack();
                }

                if (!pvhist.empty()) {
                    es.pv = s1;
                    es.fen = getFen();
                    es.hashKey = key();
                    es.pvhist = pvhist;
                }
            }
        }
        else if (s0 == "pd") { /// ponderMove
            es.ponderMoveString = s1;
        }
        else if (s0 == "mt") { /// moveTime
            es.elapsedInMillisecond = tcecTimeStringToMillisecond(s1);
        }
        else if (s0 == "tl") { // timeleft
            es.timeleftInMillisecond = tcecTimeStringToMillisecond(s1);
        }
        else if (s0 == "s") { /// speed
            es.nps = std::max<int>(0, std::atoi(s1.c_str()));
            if (s1.find("kN/s") != std::string::npos) {
                es.nps *= 1000;
            }
        }
        else if (s0 == "tb") { /// tbhits
            if (s1 != "null") {
                es.tbhits = std::max<int>(0, std::atoi(s1.c_str()));
            }
        }
        else if (s0 == "h") { /// hashusage
            es.hashusage = std::max<int>(0, static_cast<int>(std::atof(s1.c_str()) * 10.0));
        }
        else if (s0 == "ph") { /// ponderhitrate
//            es.ponderhitrate = std::max<int>(0, static_cast<int>(std::atof(s1.c_str()) * 10.0));
            es.ponderhitrate = std::max<int>(0, std::atoi(s1.c_str()));
        }

        else if (s0 == "R50") { /// R50
//            hist.es.r50 = std::max<int>(0, std::atoi(s1.c_str()));
        }
        else if (s0 == "Rd") { /// drawruleclock
            es.drawruleclock = std::max<int>(0, std::atoi(s1.c_str()));
        }
        else if (s0 == "Rr") { /// resignruleclock
            es.resignruleclock = std::max<int>(0, std::atoi(s1.c_str()));
        }
        else if (s0 == "wv") {  /// eval from white's perspective 'wv'
            auto negative = false;
            if (s1[0] == '-') {
                negative = true;
                s1 = s1.substr(1);
                if (s1.empty()) continue;
            }
            if (s1[0] == 'M') {
                s1 = s1.substr(1);
                if (s1.empty()) continue;
                es.mating = true;
            }

            es.score = static_cast<int>(std::atof(s1.c_str()) * 100);
            if (negative) {
                es.score = -es.score;
            }
            if (hist.move.piece.side == Side::white) {
                es.score = -es.score;
            }
        }
        else if (s0 == "mb") { /// material balance
            es.mb = s1;
        }
    }

    hist.esVec.push_back(es);
}

std::vector<HistBasic> BoardCore::parsePv(const std::string& pvString, bool isCoordinateOnly)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _parsePv(pvString, isCoordinateOnly);
}

std::vector<HistBasic> BoardCore::_parsePv(const std::string& pvString, bool isCoordinateOnly)
{
    auto vec = Funcs::splitString(pvString, ' ');

    std::vector<HistBasic> pvhist;

    for(auto && s : vec) {
        if (s.empty()) continue;
        auto move = moveFromString_coordinate(s);
        if (!move.isValid() && !isCoordinateOnly) {
            move = moveFromString_san(s);
        }

        if (!move.isValid() || !_checkMake(move.from, move.dest, move.promotion)) {
            break;
        }
        pvhist.push_back(histList.back());
    }

    return pvhist;
}

void BoardCore::_parseComment_standard(const std::string& comment, Hist& hist)
{
    auto commentVec = Funcs::splitString(comment, ' ');

    // WDL
    EngineScore es;
    for(int i = 0; i < static_cast<int>(commentVec.size()); ++i) {
        auto ss = commentVec.at(i);
        auto n = std::count(ss.begin(), ss.end(), '/');
        if (n != 2) {
            continue;
        }

        auto vec = Funcs::splitString(ss, '/');
        if (vec.size() != 3) {
            continue;
        }

        auto s1 = vec.at(0);
        if (!isdigit(s1[0])) {
            continue;
        }
        auto s2 = vec.at(1);
        auto s3 = vec.at(2);

        es.stats_win = std::atoi(s1.c_str());
        es.stats_draw = std::atoi(s2.c_str());
        es.stats_loss = std::atoi(s3.c_str());

        commentVec.erase(commentVec.begin() + i); //, commentVec.begin() + i + 3);
        break;
    }

    if (commentVec.size() >= 2 && isdigit(commentVec.at(1).at(0)) && commentVec.front().find('/') != std::string::npos) {
        auto ss = commentVec.front();
        auto ch = ss.at(0);
        if (isdigit(ch) || ch == 'M' || ch == '-' || ch == '+') {
            auto p = ss.find('/');
            auto s0 = ss.substr(0, p);
            auto s1 = ss.substr(p + 1);

            if (ch == 'M' || (s0.length() > 2 && s0.at(1) == 'M')) {
                s0 = s0.substr(ch == 'M' ? 1 : 2);
                auto score = std::atof(s0.c_str());
                es.score = int(score);
                es.mating = true;
            } else {
                auto score = std::atof(s0.c_str());
                es.score = int(score * 100);
            }

            es.depth = std::atoi(s1.c_str());
            es.elapsedInMillisecond = std::atof(commentVec.at(1).c_str());

            if (commentVec.size() <= 2) {
                commentVec.clear();
                return;
            }

            commentVec.erase(commentVec.begin()); // delete score/depth
            commentVec.erase(commentVec.begin()); // delete time

            if (isdigit(commentVec.front().at(0))) {
                es.nodes = std::atoll(commentVec.front().c_str());
                commentVec.erase(commentVec.begin()); // delete nodes
            }

            if (commentVec.size() >= 2 && commentVec.front() == ";") {
                commentVec.erase(commentVec. begin());
            } else {
                commentVec.clear();
            }
        }
    }
    std::string str;
    for(auto && ss : commentVec) {
        if (!str.empty()) str += " ";
        str += ss;
    }

    hist.esVec.push_back(es);
    hist.comment = str;
}

bool BoardCore::fromMoveList(const std::string& str, Notation notation, int* moveCount)
{
    std::lock_guard<std::mutex> dolock(dataMutex);

    enum class State {
        none, move, comment, evalsym, variant, counter
    };
    
    auto st = State::none;
    
    auto level = 0;
    auto ok = true;
    
    std::vector<std::string> moveStringVec;
    std::map<size_t, std::string> commentMap;
    std::map<size_t, std::string> eSymMap;

    std::string moveString, comment, esym;

    char ch = 0, prevch = 0;
    for(size_t i = 0; i < str.length() && ok; i++) {
        
        prevch = ch;
        ch = str.at(i);
        switch (st) {
            case State::none:
                if (isalpha(ch)) {
                    moveString = ch;
                    st = State::move;
                } else if (ch == '!' || ch == '?') {
                    esym = ch;
                    st = State::evalsym;
                } else if (ch == '{') {
                    comment.clear();
                    st = State::comment;
                } else if (ch == '(') {
                    st = State::variant;
                    level++;
                } else if (isdigit(ch)) {
                    st = State::counter;
                }
                break;
                
            case State::move:
                if (isalnum(ch) || ch == '=' || ch == '+' || (ch == '-' && (prevch == 'O' || prevch == '0'))) { // O-O
                    moveString += ch;
                } else {
                    // 17. Qd4 gxf1=Q+
                    if (moveString.length() < 2 || moveString.length() > 8) {
                        ok = false;
                        break;
                    }
                    
                    moveStringVec.push_back(moveString);
                    moveString.clear();
                    
                    i--;
                    st = State::none;
                }
                
                break;
                
            case State::evalsym:
                if (ch == '!' || ch == '?') {
                    esym += ch;
                    break;
                }
                eSymMap[moveStringVec.size()] = esym;
                esym.clear();
                
                i--;
                st = State::none;
                break;


            case State::comment:
                if (ch == '}') {
                    if (!comment.empty()) {
                        auto k = moveStringVec.size();
                        auto it = commentMap.find(k);
                        if (it != commentMap.end()) {
                            comment = it->second + " " + comment;
                        }
                        
                        commentMap[k] = comment;
                        comment.clear();
                    }
                    st = State::none;
                    break;
                }
                comment += ch;
                break;

            case State::variant:
                if (ch == '(') {
                    level++;
                } else
                if (ch == ')') {
                    level--;
                    if (level == 0) {
                        st = State::none;
                    }
                }
                break;

            case State::counter:
                if (isalnum(ch)) {
                    break;
                }
                if (ch != '.' && ch != ')') {
                    i--;
                }

                st = State::none;
                break;

//            default:
//                break;
        }
    }
    
    if (moveString.size() > 1 && moveString.size() < 10) {
        moveStringVec.push_back(moveString);
    }

    if (!esym.empty()) {
        eSymMap[moveStringVec.size()] = esym;
    }

    if (!comment.empty()) {
        commentMap[moveStringVec.size()] = comment;
    }

    if (moveCount) {
        *moveCount = static_cast<int>(moveStringVec.size());
    }

    for(size_t i = 0; i < moveStringVec.size(); i++) {
        auto ss = moveStringVec.at(i);

        Move move;

        if (notation == Notation::san) {
            move = moveFromString_san(ss);
        } else {
            move = moveFromString_coordinate(ss);
        }

        if (move == Move::illegalMove) {
            return false;
        }

        /// Parse comment before making move for parsing pv
        auto parsedComment = false;
        Hist tmphist;
        auto it = commentMap.find(i + 1);
        if (it != commentMap.end()) {
            //histList.back().comment = it->second;
            _parseComment(it->second, tmphist);
            parsedComment = true;
        }

        if (!_checkMake(move.from, move.dest, move.promotion)) {
            return false;
        }
        
        assert(!histList.empty());
        if (parsedComment) {
            histList.back().comment = tmphist.comment;
            histList.back().esVec = tmphist.esVec;
        }
        
//        {
//            auto it = eSymMap.find(i);
//            if (it != eSymMap.end()) {
//                auto esym = Funcs::string2MoveEvaluationSymbol(it->second);
//                if (esym != MoveEvaluationSymbol::none && !histList.empty()) {
//                    histList.back().mes = esym;
//                }
//            }
//
//        }
    }
    return true;
}

std::string BoardCore::toMoveListString(Notation notation, int itemPerLine, bool moveCounter, CommentComputerInfoType computerInfoType, bool pawnUnit, int precision) const
{
    return toMoveListString(histList, variant, notation, itemPerLine, moveCounter, computerInfoType, pawnUnit, precision);
}


std::string BoardCore::toMoveListString(const std::vector<Hist>& histList, ChessVariant variant, Notation notation, int itemPerLine, bool moveCounter,
                                        CommentComputerInfoType computerInfoType, bool pawnUnit, int precision)
{
    std::ostringstream stringStream;
    
    auto c = 0;
    for(size_t i = 0, k = 0; i < histList.size(); i++, k++) {
        auto hist = histList.at(i);
        if (i == 0 && hist.move.piece.side == Side::black) k++; // counter should be from event number
        
        if (c) stringStream << " ";
        if (moveCounter && (k & 1) == 0) {
            stringStream << (1 + k / 2) << ". ";
        }
        
        switch (notation) {
            case Notation::san:
                stringStream << hist.sanString;
                break;
                
            case Notation::lan:
                stringStream << BoardCore::toString_lan(hist.move, variant);
                break;
            case Notation::algebraic_coordinate:
            default:
                if (Funcs::isChessFamily(variant)) {
                    stringStream << ChessBoard::moveString_coordinate(hist.move);
                } else {
                }
                break;
        }
        
        // Comment
        auto haveComment = false;
        if (computerInfoType != CommentComputerInfoType::none) {
            auto s = hist.computingString(computerInfoType, pawnUnit, precision);
            if (!s.empty()) {
                haveComment = true;
                stringStream << " {" << s;
            }
        }

        if (!hist.comment.empty() && moveCounter) {
            stringStream << (haveComment ? "; " : " {");
            
            haveComment = true;
            stringStream << hist.comment ;
        }
        
        if (haveComment) {
            stringStream << "} ";
        }
        
        c++;
        if (itemPerLine > 0 && c >= itemPerLine) {
            c = 0;
            stringStream << std::endl;
        }
    }
    
    return stringStream.str();
}

std::string BoardCore::toSimplePgn() const
{
    std::ostringstream stringStream;

    stringStream << "[Event \"event\"]\n";
    if (!startFen.empty()) {
        stringStream << "[FEN \"" << startFen << "\"]\n";
    }

    stringStream << "\n" << toMoveListString(Notation::san, 1000, false,
                                             CommentComputerInfoType::standard);
    return stringStream.str();
}

void BoardCore::flip(FlipMode flipMode)
{
    
    switch (flipMode) {
        case FlipMode::none:
            return;
        case FlipMode::horizontal: {
            auto mr = size() / columnCount();
            auto halfc = columnCount() / 2;
            for(int r = 0; r < mr; r++) {
                auto pos = r * columnCount();
                for(int f = 0; f < halfc; f++) {
                    std::swap(pieces[pos + f], pieces[pos + columnCount() - 1 - f]);
                }
            }
            hashKey = initHashKey();
            return;
        }
            
        case FlipMode::vertical:
        case FlipMode::rotate: {
            auto halfsz = size() / 2;
            auto mr = size() / columnCount();
            for(int r0 = 0; r0 < halfsz; r0++) {
                auto r1 = flipMode == FlipMode::vertical ? (mr - 1 - r0 / columnCount()) * columnCount() + r0 % columnCount() : size() - 1 - r0;
                std::swap(pieces[r0], pieces[r1]);
                if (!pieces[r0].isEmpty()) {
                    pieces[r0].side = xSide(pieces[r0].side);
                }
                if (!pieces[r1].isEmpty()) {
                    pieces[r1].side = xSide(pieces[r1].side);
                }
            }
            
            side = xSide(side);
            setupPieceCount();

            hashKey = initHashKey();
            return;
        }
    }
}


void BoardCore::flipPieceColors()
{
    for(auto && piece : pieces) {
        if (piece.isEmpty()) {
            continue;
        }

        piece.side = xSide(piece.side);
    }

    side = xSide(side);
    setupPieceCount();
    hashKey = initHashKey();
}

bool BoardCore::sameContent(BoardCore* board) const
{
    auto same = false;
    if (board &&
            variant == board->variant &&
            histList.size() == board->histList.size() &&
            getStartingFen() == board->getStartingFen()) {
        same = true;
        for(size_t i = 0; i < board->histList.size(); i++) {
            auto hist0 = histList.at(i);
            auto hist1 = board->histList.at(i);
            if (hist0.move != hist1.move) {
                same = false;
                break;
            }
        }
    }
    return same;
}
