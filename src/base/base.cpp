/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <iostream>
#include <cctype>
#include <chrono>

#include "base.h"

using namespace banksia;

Piece Piece::emptyPiece(0, Side::none);
Move Move::illegalMove(-1, -1);
MoveFull MoveFull::illegalMove(-1, -1);

void BoardData::pieceList_setupAll()
{
    pieceList_reset((int*)pieceList);

    for (size_t i = 0; i < pieces.size(); ++i) {
        auto p = _getPiece(i);
        _setPiece(i, p);
    }
}

void BoardData::pieceList_reset(int* pieceList)
{
    for (auto i = 0; i < 16; i++) {
        pieceList[i] = pieceList[16 + i] = -1;
    }
}

bool BoardData::pieceList_setEmpty(int* pieceList, int pos)
{
    for (auto sd = 0; sd < 2; sd++) {
        if (pieceList_setEmpty(pieceList, pos, sd)) {
            return true;
        }
    }
    return false;
}

bool BoardData::pieceList_setEmpty(int* pieceList, int pos, int sd)
{
    int d = sd == 0 ? 0 : 16;
    for (auto i = 0; i < 16; i++) {
        if (pieceList[i + d] == pos) {
            pieceList[i + d] = -1;
            return true;
        }
    }

    return false;
}

bool BoardData::pieceList_isDraw() const
{
    return pieceList_isDraw((const int*)pieceList);
}


bool BoardData::pieceList_hasPos(const int* pieceList, int pos)
{
    for (auto i = 0; i < 16; i++) {
        if (pieceList[i] == pos || pieceList[i + 16] == pos) return true;
    }
    return false;
}

bool BoardData::pieceList_hasPos(const int* pieceList, int pos, Side side)
{
    for (auto i = 0, d = side == Side::black ? 0 : 16; i < 16; i++) {
        if (pieceList[i + d] == pos) return true;
    }
    return false;
}

bool BoardData::pieceList_hasDoublePos(const int* pieceList)
{
    for (auto d = 0; d < 17; d += 16) {
        for (auto i = 0; i < 16; i++) {
            if (pieceList[i + d] >= 0) {
                for (auto j = i + 1; j < 16; j++) {
                    if (pieceList[i + d] == pieceList[j + d]) {
                        std::cerr << "Double pos in pieceList i=" << i << ", j=" << j << ", d=" << d
                                  << ", pos=" << pieceList[i + d] << std::endl;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool BoardCore::fromOriginPosition() const
{
    return startFen.empty();
}

std::string BoardCore::getStartingFen() const
{
    return getStartingFen(moveFormat);
}

std::string BoardCore::getStartingFen(const MoveFormat&) const
{
    return startFen;
}

std::string BoardCore::getFen(bool enpassantLegal, bool horizontalFlip) const
{
    return getFen(enpassantLegal, horizontalFlip, moveFormat);
}

std::string BoardCore::getFen(bool enpassantLegal, int halfCount, int fullMoveCount,
                              bool horizontalFlip) const
{
    return getFen(enpassantLegal, halfCount, fullMoveCount, horizontalFlip, moveFormat);
}

std::string BoardCore::getEPD(bool enpassantLegal, bool withRecords, bool horizontalFlip) const
{
    return getEPD(enpassantLegal, withRecords, horizontalFlip, moveFormat);
}

std::string BoardCore::getEPD(bool enpassantLegal, bool withRecords, const Hist& hist,
                              bool horizontalFlip) const
{
    return getEPD(enpassantLegal, withRecords, hist, horizontalFlip, moveFormat);
}

std::map<std::string, std::string> BoardCore::getEPDMap(bool enpassantLegal, const Hist& hist,
                                                        bool horizontalFlip) const
{
    return getEPDMap(enpassantLegal, hist, horizontalFlip, moveFormat);
}


std::string BoardCore::getFen(bool enpassantLegal, bool horizontalFlip, const MoveFormat& mf) const
{
    auto k = std::max<int>(fen_fullMoveCnt, (histList.size() + 1) / 2);
    return getFen(enpassantLegal, quietCnt, k, horizontalFlip, mf);
}

std::string BoardCore::getEPD(bool enpassantLegal, bool withRecords, bool horizontalFlip,
                              const MoveFormat& mf) const
{
    auto hist = histList.empty() ? Hist() : getLastHist();
    return getEPD(enpassantLegal, withRecords, hist, horizontalFlip, mf);
}

std::string BoardCore::getEPD(bool enpassantLegal, bool withRecords, const Hist& hist, bool horizontalFlip,
                              const MoveFormat& mf) const
{
    auto str = getFen(enpassantLegal, -1, -1, horizontalFlip, mf);

    if (withRecords) {
        auto k = std::max<int>(fen_fullMoveCnt, (histList.size() + 1) / 2);

        // hmvc halfmove clock, fmvn fullmove number
        str += " hmvc " + std::to_string(quietCnt) + "; fmvn " + std::to_string(k) + ";";

        if (!hist.esVec.empty()) {
            auto es = hist.esVec.begin();
            str += es->toEPDString();
        }

        if (!hist.comment.empty()) {
            str += "c0 \"" + hist.comment + "\";";
        }
    }

    return str;
}

std::map<std::string, std::string> BoardCore::getEPDMap(bool enpassantLegal, const Hist& hist,
                                                        bool horizontalFlip, const MoveFormat& mf) const
{
    std::map<std::string, std::string> theMap;

    theMap["epd"] = getFen(enpassantLegal, -1, -1, horizontalFlip, mf);

    auto k = std::max<int>(fen_fullMoveCnt, (histList.size() + 1) / 2);

    // hmvc halfmove clock, fmvn fullmove number
    theMap["hmvc"] = std::to_string(quietCnt);
    theMap["fmvn"] = std::to_string(k);

    if (!hist.esVec.empty()) {
        auto es = hist.esVec.front();
        es.addToEPDMap(theMap);
    }

    if (!hist.comment.empty()) {
        theMap["c0"] = hist.comment;
    }

    return theMap;
}

std::string BoardCore::EPDMap2String(const std::map<std::string, std::string>& theMap)
{
    std::string s;
    auto epdIt = theMap.find("epd");
    if (epdIt != theMap.end()) {
        s = epdIt->second;

        for (auto&& it : theMap) {
            if (it.first != "epd") {
                s += " " + it.first + " ";
                if (it.first == "id" || it.first == "c0" || it.first == "c1") {
                    s += "\"" + it.second + "\";";
                } else {
                    s += it.second + ";";
                }
            }
        }
    }

    return s;
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

std::string BoardCore::getUciPositionString(const Move& pondermove, const MoveFormat& mf,
                                           bool forceFen) const
{
    const auto jieqiFen = variant == ChessVariant::jieqi;

    std::string str = "position ";
    if (fromOriginPosition()) {
        str += (forceFen || jieqiFen) ? "fen " + banksia::getOriginFen(variant)
                                      : "startpos";
    } else {
        str += "fen " + getStartingFen(mf);
    }

    if (!histList.empty()) {
        str += " moves";
        for (auto&& hist : histList) {
            str += " " + toString_coordinate(hist.move, mf);
        }
    }

    if (pondermove.isValid()) {
        if (histList.empty()) {
            str += " moves";
        }
        str += " " + toString(pondermove, mf);
    }
    return str;
}

Move BoardCore::moveFromString_coordinate(const std::string& moveString, const MoveFormat& mf) const
{
    if (moveString.length() < 4) return Move::illegalMove;

    int from = coordinateStringToPos(moveString, mf);
    auto str = moveString.substr(coordinateFromLength(from, mf));
    int dest = coordinateStringToPos(str, mf);

    if (!isPositionValid(from) || !isPositionValid(dest)) {
        return Move::illegalMove;
        ;
    }

    auto promotion = EMPTY;
    if (moveString.length() > 4) {
        char ch = moveString.at(4);
        if (ch == '=' && moveString.length() > 5) ch = moveString.at(5);
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }
        auto t = charactorToPieceType(ch);
        if (Move::isValidPromotion(t)) promotion = t;
    }

    return Move(from, dest, promotion);
}

bool BoardCore::newGame(std::string fen)
{
    return newGame(fen, std::vector<Hist>(), false);
}

bool BoardCore::_newGame(std::string fen)
{
    return _newGame(fen, std::vector<Hist>(), false);
}

bool BoardCore::newGame(std::string fen, const std::vector<Hist>& startMoves, bool createSanString)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _newGame(fen, startMoves, createSanString);
}

bool BoardCore::checkMake(const Hist& hist, bool createSanString)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _checkMake(hist, createSanString);
}

void BoardCore::_insertHist(const Hist& hist, int atIdx)
{
    if (atIdx >= static_cast<int>(histList.size())) {
        [[maybe_unused]] auto b = _checkMake(hist, true);
        assert(b);
    } else {
        histList.insert(histList.begin() + atIdx, hist);
    }
}

// Including level
bool BoardCore::_checkMake(const Hist& hist, bool createSanString)
{
    /// OCGDB: with the move's variation (Banksia GUI's core)
    if (_checkMake(hist.move, hist.variationRec, createSanString)) {
        auto lastHist = _getLastHistPointer();
        assert(lastHist);
        lastHist->comment = hist.comment;
        lastHist->esVec = hist.esVec;

        if (lastHist->sanString.empty()) {
            lastHist->sanString = hist.sanString;
        }

        return true;
    }

    return false;
}


/// OCGDB addition (from Banksia GUI's core)
bool BoardCore::checkMake(const Move& move, const VariationRec& vr, bool createSanString)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _checkMake(move, vr, createSanString);
}

/// OCGDB addition (from Banksia GUI's core): the board goes to the variation's position first
bool BoardCore::_checkMake(const Move& move, const VariationRec& vr, bool createSanString)
{
    VariationRec preVr;
    std::vector<Piece> variationPieces;

    if (!histList.empty()) {
        preVr = histList.back().variationRec;
    }
    if (preVr != vr) {
        _adjustBoardToMoveLevel(vr);
        variationPieces = pieces;
    }

    if (_quickCheckMake(move.from, move.dest, move.promotion, createSanString)) {
        auto hist = _getLastHistPointer();
        hist->variationRec = vr;
        if (!variationPieces.empty()) {
            hist->variationPieces = variationPieces;
        }
        return true;
    }
    return false;
}


int BoardCore::_removeHistList(int atPly, int n)
{
    assert(atPly > 0 && static_cast<size_t>(atPly) < histList.size() && n > 0);

    auto cnt = 0;
    auto p = histList.begin() + atPly;

    for (auto i = 0; i < n && histList.size() > static_cast<size_t>(atPly); ++i) {
        histList.erase(p);
        cnt++;
    }

    assert(cnt == n);
    return cnt;
}

bool BoardCore::_newGame(std::string fen, const std::vector<Hist>& startMoves, bool createSanString)
{
    histList.clear();
    firstComment.clear();
    result.reset();

    auto goodFen = _setFen(fen);

    assert(_isHashKeyValid());

    for (auto&& hist : startMoves) {
        _checkMake(hist, createSanString);
    }


    return goodFen && isValid();
}

void BoardCore::_clear()
{
    for (auto&& piece : pieces) {
        piece.setEmpty();
    }
}

void BoardCore::_setFen_basic_2ndpart(const std::vector<std::string>& vec)
{
    side = Side::white;
    if (vec.size() >= 2) {
        auto str = vec.at(1);
        side = str.length() > 0 && str.at(0) == 'w' ? Side::white : Side::black;
    }

    /// Halfmove clock
    quietCnt = 0;
    if (vec.size() >= 5) {
        auto str = vec.at(4);
        try {
            auto k = std::stoi(str);
            if (k >= 0 && k <= 1000) {
                quietCnt = k;
            }
        } catch (const std::exception&) {
        }
    }
    fen_quietCnt = quietCnt;

    /// full move
    fen_fullMoveCnt = 1;
    if (vec.size() >= 6) {
        try {
            auto str = vec.at(5);
            auto k = std::stoi(str);
            if (k >= 1 && k <= 2000) {
                fen_fullMoveCnt = k;
            }
        } catch (const std::exception&) {
        }
    }
}

void BoardCore::setupPieceIndexes()
{
    int idxs[] = { 0, 0 };
    for (auto&& piece : pieces) {
        if (piece.isEmpty()) continue;
        piece.idx = idxs[static_cast<int>(piece.side)]++;
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
    std::cout << "BoardCore::isHashKeyValid, hashKey:" << hashKey << ", hk: " << hk << std::endl;
    printOut("BoardCore::isHashKeyValid, INVALID");
    return false;
}

void BoardCore::setHashKey(u64 key)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    hashKey = key;
}

void BoardCore::setupPlyCounters()
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    _setupPlyCounters();
}

/// OCGDB: a variation's moves are counted from the move it replaces (Banksia GUI's core)
void BoardCore::_setupPlyCounters()
{
    VariationRec vr;
    std::map<int, int> theMap;

    auto start = std::max(0, fen_fullMoveCnt - 1) * 2;
    if (startFen.find(" b") != std::string::npos) {
        start++;
    }
    theMap[0] = start;

    for (auto i = 0, n = static_cast<int>(histList.size()); i < n; ++i) {
        auto hist = &histList[i];

        if (vr != hist->variationRec) {
            if (vr.level < hist->variationRec.level) {
                theMap[hist->variationRec.level] = theMap[vr.level] - 1;
            } else if (vr.level == hist->variationRec.level) {
                assert(hist->variationRec.level > 0);
                theMap[hist->variationRec.level] = theMap[hist->variationRec.level - 1] - 1;
            }
            vr = hist->variationRec;
        }
        hist->plyCounter = theMap[vr.level];
        theMap[vr.level]++;
    }
}


bool BoardCore::equalMoveLists(const BoardCore* oBoard, bool embeded) const
{
    assert(oBoard && startFen == oBoard->startFen);

    /// OCGDB: the main lines -- moves of variations are not the game's
    auto l0 = _mainLineHists(), l1 = oBoard->_mainLineHists();
    auto n0 = l0.size(), n1 = l1.size();
    if (n0 != n1 && !embeded) {
        return false;
    }

    auto n = std::min(n0, n1);
    for (size_t i = 0; i < n; ++i) {
        if (l0.at(i)->move != l1.at(i)->move) {
            return false;
        }
    }

    return true;
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

    if (piece.isEmpty() || piece.side != side
        || (piece.side == cap.side && !canMoveOntoOwnPiece(piece, cap))
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

std::vector<MoveFull> BoardCore::gen(Side attackerSide) const
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    return _gen(attackerSide);
}

void BoardCore::_gen(std::vector<MoveFull>& moves, Side side) const
{
    _gen(moves, side, -1);
}

std::vector<MoveFull> BoardCore::_gen(Side side) const
{
    std::vector<MoveFull> moves;
    _gen(moves, side, -1);
    return moves;
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
    for (auto&& move : moveList) {
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

    /// Only when the caller named one: see movesCarryPromotion().
    auto matchPromotion = movesCarryPromotion();

    for (auto&& move : moveList) {
        if ((from >= 0 && move.from != from) || (dest >= 0 && move.dest != dest)
            || (matchPromotion && promotion > KING && promotion != move.promotion)) {
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

bool BoardCore::_quickCheck_rook(int from, int dest, bool checkMiddle) const
{
    // same row
    if (getRow(from) == getRow(dest)) {
        if (checkMiddle) {
            auto d = from < dest ? +1 : -1;
            for (auto i = from + d, step = 0; i != dest; i += d, step++) {
                assert(step < 8);
                if (!_isEmpty(i)) {
                    return false;
                }
            }
        }
        return true;
    }

    if (getColumn(from) != getColumn(dest)) {
        return false;
    }

    // same file
    if (checkMiddle) {
        auto c = columnCount();
        auto d = from < dest ? c : -c;
        for (auto i = from + d, step = 0; i != dest; i += d, step++) {
            assert(step < 8);
            if (!_isEmpty(i)) {
                return false;
            }
        }
    }

    return true;
}

int BoardCore::findKing(Side side) const
{
    for (auto pos = 0; pos < int(pieces.size()); ++pos) {
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

void BoardCore::flipSide()
{
    side = xSide(side);
    hashKey ^= xorSideHashKey();
}

/// OCGDB: a move made after a variation's move belongs to that variation
void BoardCore::_make(const MoveFull& move)
{
    Hist hist;
    _make(move, hist);
    if (!histList.empty()) {
        hist.variationRec = histList.back().variationRec;
        hist.variationRec.startNewLevel = 0;
    }
    histList.push_back(hist);

    flipSide();
}


/// OCGDB addition (from Banksia GUI's core)
void BoardCore::_adjustBoardToMoveBackLevel(const VariationRec& vr)
{
    assert(!histList.empty());

    auto lastHist = &histList.back();

    auto sz = static_cast<int>(histList.size());
    auto wVr = lastHist->variationRec;

    std::vector<Hist> histVec;
    auto i = sz - 1;

    if (vr.startNewLevel && vr.level == wVr.level && vr.groupIdx != wVr.groupIdx) {
        /// Other variabtion which is same level
        for (; i >= 0; i--) {
            auto hist = &histList.at(i);

            /// Reached the target
            if (hist->variationRec.level < vr.level) {
                break;
            }

            if (hist->variationRec == wVr) {
                histVec.push_back(*hist);
            }
        }
    } else {
        for (; i >= 0; i--) {
            auto hist = &histList.at(i);

            /// Reached the target
            if (hist->variationRec.level == vr.level) {
                _takeBack(*hist);
                side = xSide(side);
                break;
            }
            assert(hist->variationRec.level > vr.level);

            if (hist->variationRec == wVr) {
                histVec.push_back(*hist);
            }

            /// Ignore higher levels
            if (hist->variationRec.level < wVr.level || hist->variationRec == wVr) {
                wVr = hist->variationRec;
            }
        }
    }

    for (int j = histVec.size() - 1; j >= 0; j--) {
        auto h = &histVec.at(j);
        assert(h->variationRec.level >= vr.level);

        auto m = h->move;
        assert(m.piece.side == side);
        [[maybe_unused]] auto b = _quickCheckMake(m.from, m.dest, m.promotion, false);
        assert(b);
        histList.pop_back(); // not for storing
    }
    assert(histList.size() == static_cast<size_t>(sz));
}

/// OCGDB: taking back a variation's first move takes the board back to its parent line
void BoardCore::_takeBack()
{
    if (histList.empty()) {
        return;
    }

    /// Standard takeback
    auto hist = histList.back();
    histList.pop_back();

    _takeBack(hist);
    side = xSide(side);

    /// check on varation
    if (!histList.empty()) {
        auto h = &histList.back();

        if (h->variationRec == hist.variationRec) {
            return;
        }

        if (h->variationRec.level < hist.variationRec.level) {
            auto move = h->move;
            assert(move.piece.side == side);
            [[maybe_unused]] auto b = _quickCheckMake(move.from, move.dest, move.promotion, false);
            assert(b);
            histList.pop_back(); // not to save the hist by _quickCheckMake
        } else {
            _adjustBoardToMoveBackLevel(hist.variationRec);
        }
    }
}

std::chrono::milliseconds::rep now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
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

/// flipflip_x[oMode] is the mode of applying oMode, then x:
///     flip(flip(pos, oMode), x) == flip(pos, flipflip_x[oMode])
#ifdef _CHESS_
/// Chess has the 8 modes of the square board (none, horizontal, vertical, flipVH, flipHV, rotate90,
/// rotate, rotate270, the order of the enum)
static const FlipMode flipflip_h[]
        = { FlipMode::horizontal, FlipMode::none, FlipMode::rotate, FlipMode::rotate90,
            FlipMode::rotate270, FlipMode::flipVH, FlipMode::vertical, FlipMode::flipHV };
static const FlipMode flipflip_v[]
        = { FlipMode::vertical, FlipMode::rotate, FlipMode::none, FlipMode::rotate270,
            FlipMode::rotate90, FlipMode::flipHV, FlipMode::horizontal, FlipMode::flipVH };
static const FlipMode flipflip_r[]
        = { FlipMode::rotate, FlipMode::vertical, FlipMode::horizontal, FlipMode::flipHV,
            FlipMode::flipVH, FlipMode::rotate270, FlipMode::none, FlipMode::rotate90 };
static const FlipMode flipflip_vh[]
        = { FlipMode::flipVH, FlipMode::rotate270, FlipMode::rotate90, FlipMode::none,
            FlipMode::rotate, FlipMode::vertical, FlipMode::flipHV, FlipMode::horizontal };
static const FlipMode flipflip_hv[]
        = { FlipMode::flipHV, FlipMode::rotate90, FlipMode::rotate270, FlipMode::rotate,
            FlipMode::none, FlipMode::horizontal, FlipMode::flipVH, FlipMode::vertical };
static const FlipMode flipflip_r90[]
        = { FlipMode::rotate90, FlipMode::flipHV, FlipMode::flipVH, FlipMode::horizontal,
            FlipMode::vertical, FlipMode::rotate, FlipMode::rotate270, FlipMode::none };
static const FlipMode flipflip_r270[]
        = { FlipMode::rotate270, FlipMode::flipVH, FlipMode::flipHV, FlipMode::vertical,
            FlipMode::horizontal, FlipMode::none, FlipMode::rotate90, FlipMode::rotate };
#else
static const FlipMode flipflip_h[]
        = { FlipMode::horizontal, FlipMode::none, FlipMode::rotate, FlipMode::vertical };
static const FlipMode flipflip_v[]
        = { FlipMode::vertical, FlipMode::rotate, FlipMode::none, FlipMode::horizontal };
static const FlipMode flipflip_r[]
        = { FlipMode::rotate, FlipMode::vertical, FlipMode::horizontal, FlipMode::none };
#endif

FlipMode BoardCore::flip(FlipMode oMode, FlipMode flipMode)
{
    const auto k = static_cast<int>(oMode);
    switch (flipMode) {
        case FlipMode::none:
            break;
        case FlipMode::horizontal:
            return flipflip_h[k];
        case FlipMode::vertical:
            return flipflip_v[k];
        case FlipMode::rotate:
            return flipflip_r[k];
#ifdef _CHESS_
        case FlipMode::flipVH:
            return flipflip_vh[k];
        case FlipMode::flipHV:
            return flipflip_hv[k];
        case FlipMode::rotate90:
            return flipflip_r90[k];
        case FlipMode::rotate270:
            return flipflip_r270[k];
#else
        default:
            break;
#endif
    }
    return oMode;
}

std::string BoardCore::toString(const MoveFull& move)
{
    return toString(move, moveFormat);
}

std::string BoardCore::toString(const Hist& hist, Notation notation) const
{
    return toString(hist, notation, moveFormat);
}

std::string BoardCore::toString(const Move& move)
{
    return toString(move, moveFormat);
}

std::string BoardCore::toString(const Hist& hist, Notation notation, const MoveFormat& mf) const
{
    switch (notation) {
        case Notation::san:
            return toString_san(hist, mf);
        case Notation::lan:
            return toString_lan(hist);
        case Notation::algebraic_coordinate:
            return toString_coordinate(hist.move, mf);
        default:
            break;
    }
    return toString_traditional(hist.move, notation, mf);
}

std::string BoardCore::toString_coordinate(const MoveFull& move) const
{
    return toString_coordinate(move, moveFormat);
}

std::string BoardCore::toString_coordinate(const MoveFull& move, const MoveFormat& mf) const
{
    return toString(move, mf);
}

std::string BoardCore::toString_traditional(const MoveFull& move, Notation, const MoveFormat& mf) const
{
    assert(false);
    return toString(move, mf);
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

    auto isChess = isChessFamily(variant);
    /// a piece's letter from its own variant's table: xiangqi pieces were named by the chess table, so a
    /// cannon (type 5, the chess knight) was written "Nh2e2" (2026-11-27, banksiagui docs/settings-previews.md §5)
    auto letter = [&](int type) {
        auto ch = isChess ? chessPieceType2Char(type) : xqPieceType2Char(type, banksia::moveFormat);
        return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    };
    if ((isChess && move.piece.type != PAWNSTD) || (!isChess && move.piece.type != PAWNXQ)) {
        str += letter(move.piece.type);
    }

    if (isChess) {
        str += chessPosToCoordinateString(move.from) + chessPosToCoordinateString(move.dest);
    } else {
        str += xqPosToCoordinateString(move.from, banksia::moveFormat)
               + xqPosToCoordinateString(move.dest, banksia::moveFormat);
    }

    if (move.promotion) {
        str += "=";
        str += isChess ? chessPieceType2Char(move.promotion) : xqPieceType2Char(move.promotion, banksia::moveFormat);
    }
    return str;
}


Eco BoardCore::commentEco()
{
    for (int64_t n = histList.size(), i = n; i >= 0; i--) {
        auto h = i == n ? hashKey : histList.at(i).hashKey;
        auto eco = getEco(h);
        if (eco.isEmpty()) {
            continue;
        }

        if (i < n) {
            auto& hist = histList[i];
            auto ecoString = eco.toBriefString();

            if (hist.comment.find(ecoString) == std::string::npos) {
                hist.comment += ecoString;
            }
        }

        // Clear all ECOs which are commented before
        for (i--; i >= 0; --i) {
            auto& hist = histList[i];
            if (hist.comment.size() < 5) continue;
            auto p = hist.comment.find(": ");
            if (p != 3) continue;
            auto s = hist.comment.substr(0, 3);

            if (isEcoName(s)) {
                hist.comment = "";
            }
        }
        return eco;
    }
    return Eco();
}

std::vector<HistBasic> BoardCore::_parsePv(const std::string& pvString, bool isCoordinateOnly,
                                           const MoveFormat& mf)
{
    auto vec = banksia::splitString(pvString, ' ');

    std::vector<HistBasic> pvhist;

    for (auto&& s : vec) {
        if (s.empty()) continue;
        auto move = moveFromString_coordinate(s, mf);
        if (!isValid(move) && !isCoordinateOnly) {
            move = moveFromString_san(s, mf);
        }

        if (!isValid(move) || !_checkMake(move.from, move.dest, move.promotion)) {
            break;
        }
        pvhist.push_back(histList.back());
    }

    return pvhist;
}


/// A comment's text as it may stand between "{" and "}": a PGN comment ends at the first "}" and
/// cannot hold another "{" (PGN standard 5). Comments read from other files sometimes do ("e4
/// {trumpets blaring"): written as they were, the rest of the game
static std::string pgnCommentText(std::string s)
{
    for (auto&& ch : s) {
        if (ch == '{') ch = '(';
        else if (ch == '}') ch = ')';
    }
    return s;
}

std::string BoardCore::toMoveListString(Notation notation, int itemPerLine, int fullMoveCnt,
                                        bool withComputingInfo, bool pawnUnit, int precision) const
{
    return toMoveListString(notation, moveFormat, itemPerLine, fullMoveCnt, withComputingInfo, pawnUnit,
                            precision);
}

std::string BoardCore::toMoveListString(Notation notation, const MoveFormat& mf, int itemPerLine,
                                        int fullMoveCnt, bool withComputingInfo, bool pawnUnit,
                                        int precision) const
{
    return toMoveListString(histList, notation, mf, itemPerLine, fullMoveCnt, withComputingInfo, pawnUnit,
                            precision, [this](const MoveFull& move, Notation n, const MoveFormat& f) {
                                switch (n) {
                                    case Notation::lan:
                                        return toString_lan(move, variant);
                                    case Notation::algebraic_coordinate:
                                        return toString_coordinate(move, f);
                                    default:
                                        /// chess has no traditional notation: its board writes coordinates
                                        return toString_traditional(move, n, f);
                                }
                            });
}

/// OCGDB: with the variations, in brackets
std::string BoardCore::toMoveListString(const std::vector<Hist>& histList, Notation notation,
                                        const MoveFormat& mf, int itemPerLine, int fullMoveCnt,
                                        bool withComputingInfo, bool pawnUnit, int precision,
                                        const MoveStringFunc& moveString)
{
    std::string str;

    auto c = 0;
    std::vector<int> countVec, openGroupVec { 0 }; // openGroupVec: the variation open at each level

    VariationRec vr;
    const auto startsWithBlack = !histList.empty() && histList.front().move.piece.side == Side::black;
    for (size_t i = 0, n = histList.size(); i < n; i++) {
        auto hist = histList.at(i);
        const auto lineChanged = vr != hist.variationRec;

        /// Brackets: one "(" per variation entered; on leaving, one ")" per level left, and ")("
        /// when the move starts another variation at the level arrived at, so the brackets balance
        /// whether nested variations end together or the game ends inside one.
        /// a line break, but never an empty line: readers take an empty line for the end of the
        /// game
        auto newLine = [&str]() {
            if (!str.empty() && str.back() != '\n') {
                str += "\n";
            }
        };
        if (vr != hist.variationRec) {
            const auto& nvr = hist.variationRec;
            if (vr.level < nvr.level) {
                newLine();
                str += "(";
            } else {
                for (auto l = vr.level; l > nvr.level; --l) {
                    str += ")";
                }
                auto sibling = nvr.level > 0 && nvr.level < openGroupVec.size()
                               && openGroupVec[nvr.level] != nvr.groupIdx;
                if (sibling) {
                    str += ")";
                    newLine();
                    str += "(";
                } else if (vr.level > nvr.level) {
                    newLine();
                }
                /// move counters: a line that goes on keeps its own; a new variation counts on from
                /// its parent (below)
                const size_t keep = sibling ? nvr.level : nvr.level + 1;
                while (countVec.size() > keep) {
                    countVec.pop_back();
                }
            }

            if (openGroupVec.size() <= nvr.level) {
                openGroupVec.resize(nvr.level + 1, -1);
            }
            openGroupVec[nvr.level] = nvr.groupIdx;
            vr = nvr;
        }

        if (countVec.size() <= vr.level) {
            auto t = countVec.empty() ? 0 : (countVec.at(countVec.size() - 1) - 1);
            countVec.push_back(t);
        }
        countVec[vr.level]++;

        if (c) str += " ";

        /// OCGDB: also the first move of a variation and the first after one ("(5... Nf6)")
        auto moveCounter = fullMoveCnt > 0 && (i == 0 || lineChanged || hist.move.piece.side == Side::white);
        if (moveCounter) {
            /// OCGDB: the number of the move's ply counted from the move list's start: it was
            /// fullMoveCnt + count / 2, right for White's moves only, so a variation starting with
            /// a Black move was numbered one too high ("10...Ncb4" for 9...Ncb4)
            auto ply = countVec[vr.level] + (startsWithBlack ? 1 : 0);
            str += std::to_string(fullMoveCnt + (ply - 1) / 2) + ".";
            if (hist.move.piece.side == Side::black) {
                str += "..";
            }
        }

        switch (notation) {
            case Notation::san:
                str += hist.sanString;
                break;

            default:
                str += moveString(hist.move, notation, mf);
                break;
        }

        // Comment
        auto haveComment = false;
        if (withComputingInfo) {
            auto s = hist.computingString(pawnUnit, false, precision);
            if (!s.empty()) {
                haveComment = true;
                str += " {" + s;
            }
        }

        if (!hist.comment.empty()) {
            str += (haveComment ? "; " : " {");

            haveComment = true;
            str += pgnCommentText(hist.comment);
        }

        if (haveComment) {
            str += "} ";
        }

        c++;
        if (itemPerLine > 0 && !moveCounter && c + 1 >= itemPerLine) {
            c = 0;
            str += "\n";
        }
    }


    /// a game that ends inside a variation
    for (auto l = vr.level; l > 0; --l) {
        str += ")";
    }
    return str;
}


std::string BoardCore::toPgn(const PgnRecord* record) const
{
    std::string headString, eventString, resultString, moveText;
    auto haveFEN = false;

    if (record) {
        for (auto&& it : record->tags) {
            auto tag = it.first, ss = it.second;
            /// a quote or a backslash inside the value is escaped (PGN standard 8.1.1).
            /// (OCGDB writes PGN with toPgnString(), which escapes too.)
            std::string value;
            for (auto ch : ss) {
                if (ch == '"' || ch == '\\') {
                    value += '\\';
                }
                value += ch;
            }
            auto s = "[" + tag + " \"" + value + "\"]\n";

            if (tag == "Event") {
                eventString = s;
            } else {
                headString += s;
                if (tag == "FEN") haveFEN = true;
                if (tag == "Result") resultString = ss;
            }
        }

        moveText = record->moveText ? record->moveText : record->moveString;
    }

    if (eventString.empty()) {
        eventString = "[Event \"\"]\n";
    }

    if (resultString.empty()) {
        resultString = "*";
        headString += "[Result \"*\"]\n";
    }

    std::ostringstream stringStream;
    stringStream << eventString << headString;

    if (variant == ChessVariant::chess960 || variant == ChessVariant::jieqi) {
        stringStream << "[Variant \"" << banksia::chessVariant2String(variant) << "\"]\n";
    }

    if (!haveFEN && !startFen.empty()) {
        stringStream << "[FEN \"" << startFen << "\"]\n[SetUp \"1\"]\n";
    }

    stringStream << "\n";

    auto parsed = false;
    if (moveText.empty()) {
        parsed = true;
        moveText = toMoveListString(Notation::san, moveFormat, 8, 1, true);
    }

    if (!firstComment.empty()) {
        stringStream << "{" << pgnCommentText(firstComment) << "}\n";
    }
    stringStream << moveText;

    if (parsed && !resultString.empty()) {
        stringStream << " " << resultString;
    }

    return stringStream.str();
}

void BoardCore::flip(FlipMode flipMode)
{
    if (flipMode == FlipMode::none) {
        return;
    }

    std::vector<Hist> hlist;
    const auto keepHistory = flipSwapsSides();
    if (keepHistory) {
        hlist = histList;
        while (!histList.empty()) {
            _takeBack();
        }
    } else {
        histList.clear();
    }

    switch (flipMode) {
        case FlipMode::none:
            return;
        case FlipMode::horizontal: {
            auto mr = size() / columnCount();
            auto halfc = columnCount() / 2;
            for (auto r = 0; r < mr; r++) {
                auto pos = r * columnCount();
                for (auto f = 0; f < halfc; f++) {
                    std::swap(pieces[pos + f], pieces[pos + columnCount() - 1 - f]);
                }
            }
            break;
        }

        case FlipMode::vertical:
        case FlipMode::rotate: {
            auto sz = size();
            auto halfsz = sz / 2;
            auto mr = sz / columnCount();
            auto changeSide = flipSwapsSides();
            for (auto r0 = 0; r0 < halfsz; r0++) {
                auto r1 = flipMode == FlipMode::vertical
                                  ? (mr - 1 - r0 / columnCount()) * columnCount() + r0 % columnCount()
                                  : sz - 1 - r0;

                assert(r0 < halfsz && halfsz <= r1);
                std::swap(pieces[r0], pieces[r1]);

                if (changeSide) {
                    if (!pieces[r0].isEmpty()) {
                        pieces[r0].side = xSide(pieces[r0].side);
                    }
                    if (!pieces[r1].isEmpty()) {
                        pieces[r1].side = xSide(pieces[r1].side);
                    }
                }
            }

            if (changeSide) {
                side = xSide(side);
            }
        }

        default:
            break;
    }

    pieceList_setupAll();

    _setFenComplete();

    if (!startFen.empty()) {
        startFen = getFen();
    }

    for (auto&& h : hlist) {
        auto m = flip(h.move, flipMode);
        [[maybe_unused]] auto ok = _quickCheckMake(m.from, m.dest, m.promotion, false);
        assert(ok);
        if (!ok) {
            /// Should not happen for a xiangqi flip, and losing the rest of the
            /// history is better than leaving the board mid-replay.
            break;
        }
    }
}


void BoardCore::flipPieceColors()
{
    for (auto&& piece : pieces) {
        if (piece.isEmpty()) {
            continue;
        }

        piece.side = xSide(piece.side);
    }

    side = xSide(side);
    setupPieceCount();
    hashKey = initHashKey();
}

#define HAVE_COMMENT_BIT (1 << 7)
#define BOARD_MAX_MEMLEN 0x3fff


bool BoardCore::sameContent(BoardCore* board) const
{
    auto same = false;
    if (board && variant == board->variant && histList.size() == board->histList.size()
        && getStartingFen(moveFormat) == board->getStartingFen(moveFormat)) {
        same = true;
        for (size_t i = 0; i < board->histList.size(); i++) {
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

/// Check mate, 50 moves rule
Result BoardCore::rule()
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    assert(_isHashKeyValid());
    Result result;

    /// 50 moves
    if (quietCnt >= 50 * 2) {
        result.result = ResultType::draw;
        result.reason = ReasonType::fiftymoves;
        return result;
    }

    /// Mated or stalemate
    auto haveLegalMove = false;
    std::vector<MoveFull> moveList;
    _gen(moveList, side);
    for (auto&& move : moveList) {
        Hist hist;
        _make(move, hist);
        haveLegalMove = !_isIncheck(side);
        _takeBack(hist);
        if (haveLegalMove) break;
    }

    if (!haveLegalMove) {
        if (_isIncheck(side)) {
            result.result = side == Side::white ? ResultType::loss : ResultType::win;
            result.reason = ReasonType::mate;
        } else if (stalemateLoses()) {
            result.result = side == Side::white ? ResultType::loss : ResultType::win;
            result.reason = ReasonType::stalemate;
        } else {
            result.result = ResultType::draw;
            result.reason = ReasonType::stalemate;
        }
        return result;
    }

    if (quietCnt < repetitionThreatHold * 4) {
        return result;
    }

    auto cnt = 0;
    auto i = int(histList.size()), k = i - quietCnt;
    for (i -= 2; i >= 0 && i >= k; i -= 2) {
        auto hist = histList.at(i);
        if (hist.hashKey == hashKey) {
            cnt++;
            if (cnt >= repetitionThreatHold) {
                auto repeatLen = int(histList.size()) - i;
                return ruleRepetition(repeatLen);
            }
        }
    }

    return result;
}


Result BoardCore::adjudicationDraw(bool oneSide, int minPly, int plyRange, int scoreRange) const
{
    Result result;
    assert(result.isNone());

    auto len = static_cast<int>(histList.size());
    if (len >= minPly && len > plyRange) {
        auto draw = true;
        for (int i = 0, k = len - 1; i < plyRange && k >= 0; i++, k--) {
            bool theSide = (i & 1) == 0;
            if (oneSide && !theSide) {
                continue;
            }

            auto hist = histList.at(k);
            EngineScore es;
            if (!hist.esVec.empty()) {
                es = hist.esVec.front();
            }
            if (es.mating || !hist.cap.isEmpty() || isPawnMove(hist.move)
                || std::abs(es.score) > scoreRange) { // ScoreNull > 9
                draw = false;
                break;
            }
        }

        if (draw) {
            result.result = ResultType::draw;
            result.reason = ReasonType::adjudication_score;
        }
    }

    return result;
}


/// Warning: the current side is not computer side we consider
Result BoardCore::adjudicationScoreResign(bool oneSide, int plyRange, int scoreRange) const
{
    assert(scoreRange < 0);
    Result result;

    auto len = static_cast<int>(histList.size());
    if (scoreRange < 0 && plyRange > 0 && len >= plyRange) {
        auto resign = true;
        for (auto i = 0; i < len; i++) {
            bool theSide = (i & 1) == 0;
            if (oneSide && !theSide) {
                continue;
            }
            auto hist = histList.at(histList.size() - i - 1);
            EngineScore es;
            if (!hist.esVec.empty()) {
                es = hist.esVec.front();
            }

            if (es.score == ScoreNull
                || (es.mating && ((theSide && es.score > 0) || (!theSide && !oneSide && es.score < 0)))
                || (theSide && !es.mating && es.score > scoreRange)
                || (!theSide && !oneSide && !es.mating && es.score < -scoreRange)) {
                resign = false;
                break;
            }

            if (theSide) {
                plyRange--;
                if (plyRange <= 0) {
                    break;
                }
            }
        }

        if (resign && plyRange <= 0) {
            result.result
                    = side == Side::white ? ResultType::win : ResultType::loss; // board->side = opposite
            result.reason = ReasonType::adjudication_score;
        }
    }

    return result;
}


void smartFilterComment(std::string& comment)
{
    auto p = comment.find("[%clk");
    if (p != std::string::npos) {
        auto q = comment.find(']', p + 6);

        if (p <= 2 && q >= comment.size() - 2) {
            comment.clear();
        } else {
            std::string s;

            if (p > 2) {
                s = comment.substr(0, p - 1);
            }


            if (q < comment.size() - 2) {
                s += comment.substr(q + 1);
            }

            banksia::trim(s);
            comment = s;
        }
    }
}

bool BoardCore::fromMoveList(const PgnRecord& record, Notation notation, ParseVariation parseVariation, int flag,
                             PositionCallback shouldStop)
{
    return fromMoveList(record, notation, parseVariation, moveFormat, flag, shouldStop);
}

/// OCGDB: variations are read (ParseVariation::parse), kept as comments or dropped, and the
/// callback sees every position (Banksia GUI's core). Evaluation symbols (!?, ??) are consumed and
/// not kept, as in Banksia.
bool BoardCore::fromMoveList(const PgnRecord& record, Notation notation, ParseVariation parseVariation,
                             const MoveFormat& mf, int flag, PositionCallback shouldStop)
{
    std::lock_guard<std::mutex> dolock(dataMutex);
    parseMoveErrorCnt = 0;

    enum class State { none, move, comment, comment_variation, commentRestOfLine, evalsym, counter };

    auto st = State::none;

    std::vector<u8> groupIdxVec { 0 };
    std::vector<MoveStringLevel> moveStringVec;
    std::map<size_t, std::string> commentMap;

    std::string moveString, comment, esym, variation_as_comment;

    VariationRec vr;
    auto ok = true;
    auto variation_as_comment_level = 0;
    auto variationInBrace = false;
    char ch = 0, prevch = 0;

    const char* p = record.moveText ? record.moveText : record.moveString.c_str();
    for (size_t i = 0, len = strlen(p); i < len && ok; i++) {
        prevch = ch;
        ch = p[i];

        switch (st) {
            case State::none:
                if (isalpha(ch)) { // ch < 128 to avoid some Unicode
                    moveString = ch;
                    st = State::move;
                } else if (ch == '!' || ch == '?') {
                    esym = ch;
                    st = State::evalsym;
                } else if (ch == '{') {
                    // comments by semicolon or escape mechanism % in the header,
                    comment.clear();
                    st = State::comment;
                } else if (ch == ';' || (ch == '%' && (i == 0 || prevch == '\r' || prevch == '\n'))) {
                    // comments by semicolon or escape mechanism % in the header,
                    comment.clear();
                    st = State::commentRestOfLine;
                } else if (ch == '(') {
                    if (parseVariation == ParseVariation::parse) {
                        vr.startNewLevel = 1;
                        vr.level++;
                        if (vr.level >= groupIdxVec.size()) {
                            groupIdxVec.push_back(0);
                        } else {
                            groupIdxVec[vr.level]++;
                        }
                    } else {
                        variation_as_comment_level = 1;
                        variationInBrace = false;
                        comment.clear();
                        st = State::comment_variation;
                    }
                } else if (ch == ')') {
                    if (vr.level > 0) {
                        vr.level--;
                    } else {
                        /// something wrong
                    }
                } else if (isdigit(ch)) {
                    st = State::counter;
                }
                break;

            case State::move:
                if (isalnum(ch) || ch == '=' || ch == '+' || (ch == '-' && (prevch == 'O' || prevch == '0'))
                    || isMoveTextChar(ch)) { // O-O
                    moveString += ch;
                } else {
                    // 17. Qd4 gxf1=Q+
                    if (moveString.length() < 2 || moveString.length() > 8) {
                        ok = false;
                        if (len - i > 20) {
                            parseMoveErrorCnt++;
                        }
                        break;
                    }

                    vr.groupIdx = groupIdxVec[vr.level];
                    moveStringVec.push_back(MoveStringLevel(moveString, vr));
                    vr.startNewLevel = 0;
                    moveString.clear();

                    i--;
                    st = State::none;
                }

                break;

            case State::evalsym:
                if (ch == '!' || ch == '?') {
                    break;
                }

                i--;
                st = State::none;
                break;


            case State::comment_variation:
                if (variation_as_comment_level) {
                    /// OCGDB: brackets inside a comment of the variation, "{(11.a3!?)}", are text,
                    /// not variations: counted, they ended the variation early or never, and the
                    /// rest of the game was read wrong (14 of 19,739 annotated games)
                    if (variationInBrace) {
                        if (ch == '}') {
                            variationInBrace = false;
                        }
                    } else if (ch == '{') {
                        variationInBrace = true;
                    } else if (ch == '(') {
                        variation_as_comment_level++;
                        break;
                    } else if (ch == ')') {
                        variation_as_comment_level--;
                        if (parseVariation == ParseVariation::discard && variation_as_comment_level == 0) {
                            st = State::none;
                            break;
                        }
                    }
                }
            case State::commentRestOfLine:
            case State::comment:
                if ((st == State::comment && ch == '}')
                    || (st == State::comment_variation && variation_as_comment_level == 0)
                    || (st == State::commentRestOfLine && (ch == '\n' || ch == '\r'))) {
                    comment = rtrim(comment);

                    if (flag & ParseMoveListFlag_smartComment) {
                        smartFilterComment(comment);
                    }
                    if ((flag & ParseMoveListFlag_discardComment) == 0 && !comment.empty()) {
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

                if ((flag & ParseMoveListFlag_discardComment) == 0) {
                    if (!comment.empty() || ch < 0 || ch > ' ') { // trim left
                        // remove new line charators in the middle
                        if (ch == '\r' || ch == '\n') {
                            ch = ' ';
                            if (prevch == ' ') {
                                break;
                            }
                        }
                        comment += ch;
                    }
                }
                break;


            case State::counter:
                if (isalnum(ch)) {
                    break;
                }
                if (ch != '.') { // && ch != ')') {
                    i--;
                }

                st = State::none;
                break;

        }
    }

    if (moveString.size() > 1 && moveString.size() < 10) {
        vr.groupIdx = groupIdxVec[vr.level];
        moveStringVec.push_back(MoveStringLevel(moveString, vr));
    }

    if (!comment.empty()) {
        commentMap[moveStringVec.size()] = comment;
    }

    /// a broken variation is dropped and the game parsed again (BoardCore::eraseVariation)
    const auto freshStart = histList.empty();
    for (auto tries = 0;; ++tries) {
        if (_fromMoveList(record, notation, mf, flag, shouldStop, 0, moveStringVec, commentMap)
            || brokenVariationIdx < 0 || !freshStart || tries >= 64) {
            break;
        }
        auto [from, to] = eraseVariation(moveStringVec, brokenVariationIdx);
        auto shift = [from, to](std::map<size_t, std::string>& m) {
            std::map<size_t, std::string> m2;
            for (auto&& [k, v] : m) {
                if (k <= from) {
                    m2[k] = v;
                } else if (k > to) {
                    m2[k - (to - from)] = v;
                }
            }
            m = std::move(m2);
        };
        shift(commentMap);
        _newGame(record.fenText);
    }
    // first comments
    {
        firstComment.clear();
        auto it = commentMap.find(0);
        if (it != commentMap.end()) {
            firstComment = it->second;
        }
    }

    return true;
}


/// Take back on the board only, thus don't use _takeBack() but _takeBack(hist) and xchange the side

/// OCGDB: with variations and the position callback (Banksia GUI's core)
bool BoardCore::_fromMoveList(const PgnRecord& record, Notation notation, MoveFormat mf, int flag,
                              const PositionCallback& shouldStop, int startIdx,
                              const std::vector<MoveStringLevel>& moveStringVec,
                              const std::map<size_t, std::string>& commentMap)
{
    assert(pieceList_isValid());

    auto hit = false;
    auto lastLevel = 0, lastGroupIdx = 0;
    brokenVariationIdx = -1;

    detectMoveFormat(moveStringVec, mf);

    for ([[maybe_unused]] size_t i = startIdx, coordinateCnt = 0; i < moveStringVec.size(); i++) {
        if (maxParsePly > 0 && getHistListSize() >= maxParsePly && moveStringVec.at(i).variationRec.level == 0) {
            break;
        }
        auto r = moveStringVec.at(i);
        auto ss = r.moveString;
        assert(!ss.empty());

        /// Ignore string with $ such as $10 (from Lichess games)
        if (ss.at(0) == '$' && ss.length() > 1 && std::isdigit(ss.at(1))) {
            continue;
        }


        /// level
        if (lastLevel != r.variationRec.level || lastGroupIdx != r.variationRec.groupIdx) {
            _adjustBoardToMoveLevel(r.variationRec);
            lastLevel = r.variationRec.level;
            lastGroupIdx = r.variationRec.groupIdx;
        }

        Move move = Move::illegalMove;

        if (notation == Notation::san) {
            move = moveFromString_san(ss, mf);
        }
        if (!isValid(move)) { /// the xiangqi notations; illegalMove for other variants
            move = moveFromString_xq(ss, mf);
        }
        if (!isValid(move)) {
            move = moveFromString_coordinate(ss, mf);

            if (notation == Notation::san && move != Move::illegalMove) {
                coordinateCnt++;
            }
        }

        if (!isValid(move)) { // move == Move::illegalMove) {
            parseMoveErrorCnt++;
            if (moveStringVec.at(i).variationRec.level > 0) {
                brokenVariationIdx = static_cast<int>(i);
            }
            return false;
        }

        if (!isParsedMoveValid(move)) {
            parseMoveErrorCnt++;
            if (moveStringVec.at(i).variationRec.level > 0) {
                brokenVariationIdx = static_cast<int>(i);
            }
            return false;
        }

        /// Parse comment before making move for parsing pv
        auto hasComment = false;

        Hist tmphist;
        if (!commentMap.empty()) {
            auto it = commentMap.find(i + 1);
            if (it != commentMap.end()) {
                tmphist.comment = it->second;
                hasComment = true;
            }
        }

        std::string fenString;
        if (flag & ParseMoveListFlag_create_fen) {
            fenString = getFen(false);
        }

        BitboardBase* bb = nullptr;
        if ((flag & ParseMoveListFlag_create_bitboard) && shouldStop) {
            bb = posToBitboards();
            assert(bb && !bb->isEmpty());

            if (shouldStop && shouldStop(bb, this, &record)) {
                hit = true;
                delete bb;
                break;
            }
        }


        /// Making move
        if (flag & ParseMoveListFlag_quick_check) {
            if (!_quickCheckMake(move.from, move.dest, move.promotion,
                                 (flag & ParseMoveListFlag_create_san))) {
                parseMoveErrorCnt++;
                if (moveStringVec.at(i).variationRec.level > 0) {
                    brokenVariationIdx = static_cast<int>(i);
                }
                delete bb;
                return false;
            }
        } else {
            if (!_checkMake(move.from, move.dest, move.promotion)) {
                parseMoveErrorCnt++;
                if (moveStringVec.at(i).variationRec.level > 0) {
                    brokenVariationIdx = static_cast<int>(i);
                }
                delete bb;
                return false;
            }
        }

        auto lastHist = &histList.back();
        lastHist->variationRec = r.variationRec;

        assert(!histList.empty());
        if (hasComment) {
            histList.back().comment = tmphist.comment;
            histList.back().esVec = tmphist.esVec;
        }

        if (flag & ParseMoveListFlag_create_fen) {
            histList.back().fenString = fenString;
        }

        if (bb) {
            delete bb;
            bb = nullptr;
        }
    }

    /// A move list that ends inside a variation leaves the board at the end of that variation, not of
    /// the game: back to the main line (the final position, ECO, duplicate keys are the game's).
    if (!hit && !histList.empty() && histList.back().variationRec.level > 0) {
        VariationRec mainLine;
        _adjustBoardToMoveLevel(mainLine);
    }

    // last position
    if (shouldStop && !hit) {
        auto bb = posToBitboards();
        if (shouldStop(bb, this, &record)) {
            hit = true;
        }
        delete bb;
    }

    return true;
}


// Detect if it is correct FEN and variant
/// OCGDB addition (from Banksia GUI's core)
void BoardCore::_removeAllVariations()
{
    while (!histList.empty() && histList.back().variationRec.level > 0) {
        _takeBack();
    }

    for (size_t i = 0; i < histList.size();) {
        if (histList.at(i).variationRec.level > 0) {
            histList.erase(histList.begin() + i);
            continue;
        }
        ++i;
    }
}

/// OCGDB addition (from Banksia GUI's core): takes the board to the position where the
/// variation `vr` starts
/// Take back on the board only, thus don't use _takeBack() but _takeBack(hist) and xchange the side
void BoardCore::_adjustBoardToMoveLevel(const VariationRec& vr)
{
    auto k = static_cast<int>(histList.size());
    if (k == 0) {
        return;
    }

    auto lastHist = &histList.back();
    auto startLevel = lastHist->variationRec.level;


    /// going up: remove the last move/hist
    if (startLevel < vr.level) {
        assert(vr.startNewLevel);
        _takeBack(histList.back());
        side = xSide(side);//takeBackCnt++;
    } else {
        auto recoverLastMove = true;
        auto targetLevel = vr.level;
        if (vr.startNewLevel) {
            targetLevel--;
            recoverLastMove = false;
        }

        auto startGroupIdxLevel = lastHist->variationRec.groupIdx;
        auto workingGroupIdxLevel = startGroupIdxLevel, workingLevel = startLevel;

        std::string allTakeBackMoves, allMakeMoves, allIgnoredMoves;

        /// two cases: going down or going down then up to other same level
        /// we ignore all moves which are higher than startLevel or the same but in other groups
        for (k--; k >= 0; k--) {
            auto hist = histList.at(k);

            /// ignore higher levels or the same level with the last one but from other groups
            if (hist.variationRec.level > workingLevel
                || (hist.variationRec.level == workingLevel
                    && hist.variationRec.groupIdx != workingGroupIdxLevel)) {
                allIgnoredMoves += hist.sanString + " ";
                continue;
            }

            if (hist.variationRec.level < workingLevel) { // downing level
                assert(hist.move.piece.side == side);
                /// Do the move only if it is the last one to finish,
                /// otherwise, ignore to continue
                if (hist.variationRec.level == targetLevel) {
                    if (recoverLastMove) {
                        auto move = hist.move;
                        [[maybe_unused]] auto b = _quickCheckMake(move.from, move.dest, move.promotion, true);
                        assert(b);
                        histList.pop_back(); // make the move but not to store its hist

                        allMakeMoves += hist.sanString + " ";
                    }
                    break;
                }
            } else {
                assert((hist.variationRec.level == workingLevel
                        && hist.variationRec.groupIdx == workingGroupIdxLevel));
                _takeBack(hist);
                side = xSide(side); //takeBackCnt++;
                allTakeBackMoves += hist.sanString + " ";
            }

            workingGroupIdxLevel = hist.variationRec.groupIdx;
            workingLevel = hist.variationRec.level;
        }
    }
}

/// OCGDB addition (from Banksia GUI's core): the moves decoded from a database's binary move field
bool BoardCore::_fromMoveList(const PgnRecord& record, int flag, const PositionCallback& shouldStop,
                              int startIdx, const std::vector<MoveWithLevel>& moveWithLevelVec)
{
    auto hit = false;
    auto lastLevel = 0, lastGroupIdx = 0;
    brokenVariationIdx = -1;

    for (size_t i = startIdx; i < moveWithLevelVec.size(); i++) {
        if (maxParsePly > 0 && getHistListSize() >= maxParsePly && moveWithLevelVec.at(i).variationRec.level == 0) {
            break;
        }
        auto r = moveWithLevelVec.at(i);

        /// level
        if (lastLevel != r.variationRec.level || lastGroupIdx != r.variationRec.groupIdx) {
            _adjustBoardToMoveLevel(r.variationRec);
            lastLevel = r.variationRec.level;
            lastGroupIdx = r.variationRec.groupIdx;
        }

        auto move = r.code ? decode2Bytes(r.code) : r.move;

        if (!isValid(move)) { // move == Move::illegalMove) {
            parseMoveErrorCnt++;
            if (moveWithLevelVec.at(i).variationRec.level > 0) {
                brokenVariationIdx = static_cast<int>(i);
            }
            return false;
        }

        /// Parse comment before making move for parsing pv

        std::string fenString;
        if (flag & ParseMoveListFlag_create_fen) {
            fenString = getFen(false);
        }

        BitboardBase* bb = nullptr;
        if ((flag & ParseMoveListFlag_create_bitboard) && shouldStop) {
            bb = posToBitboards();
            assert(bb && !bb->isEmpty());

            if (shouldStop && shouldStop(bb, this, &record)) {
                hit = true;
                delete bb;
                break;
            }
        }

        completeDecodedMove(move);

        /// Making move
        if (flag & ParseMoveListFlag_quick_check) {
            if (!_quickCheckMake(move.from, move.dest, move.promotion,
                                 (flag & ParseMoveListFlag_create_san))) {
                parseMoveErrorCnt++;
                if (moveWithLevelVec.at(i).variationRec.level > 0) {
                    brokenVariationIdx = static_cast<int>(i);
                }
                delete bb;
                return false;
            }
        } else {
            if (!_checkMake(move.from, move.dest, move.promotion)) {
                parseMoveErrorCnt++;
                if (moveWithLevelVec.at(i).variationRec.level > 0) {
                    brokenVariationIdx = static_cast<int>(i);
                }
                delete bb;
                return false;
            }
        }

        auto lastHist = &histList.back();
        lastHist->variationRec = r.variationRec;

        assert(!histList.empty());

        if (flag & ParseMoveListFlag_create_fen) {
            histList.back().fenString = fenString;
        }

        if (bb) {
            delete bb;
            bb = nullptr;
        }
    }

    /// A move list that ends inside a variation leaves the board at the end of that variation, not of
    /// the game: back to the main line (the final position, ECO, duplicate keys are the game's).
    if (!hit && !histList.empty() && histList.back().variationRec.level > 0) {
        VariationRec mainLine;
        _adjustBoardToMoveLevel(mainLine);
    }

    if (shouldStop && !hit) {
        auto bb = posToBitboards();
        if (shouldStop(bb, this, &record)) {
            hit = true;
        }
        delete bb;
    }

    return true;
}

/// OCGDB addition: the moves of a Moves1 field, decoded and made one at a time
bool BoardCore::_fromMoveList1Byte(const PgnRecord& record, const std::vector<int8_t>& moveVec, int flag,
                                   const PositionCallback& shouldStop)
{
    /// the decoder may read one byte past a move (a 2-byte one): padded
    std::vector<int8_t> padded(moveVec);
    padded.push_back(0);
    padded.push_back(0);

    auto hit = false;
    try {
        for (size_t i = 0; i < moveVec.size();) {
            if (maxParsePly > 0 && getHistListSize() >= maxParsePly) {
                break;
            }

            auto pair = decode1Byte(padded.data() + i);
            if (pair.second < 1) {
                parseMoveErrorCnt++;
                return false;
            }
            i += pair.second;
            auto move = pair.first;
            if (!isValid(move)) {
                parseMoveErrorCnt++;
                return false;
            }

            std::string fenString;
            if (flag & ParseMoveListFlag_create_fen) {
                fenString = getFen(false);
            }

            if ((flag & ParseMoveListFlag_create_bitboard) && shouldStop) {
                auto bb = posToBitboards();
                auto stop = shouldStop(bb, this, &record);
                delete bb;
                if (stop) {
                    hit = true;
                    break;
                }
            }

            completeDecodedMove(move);

            auto ok = (flag & ParseMoveListFlag_quick_check)
                              ? _quickCheckMake(move.from, move.dest, move.promotion, flag & ParseMoveListFlag_create_san)
                              : _checkMake(move.from, move.dest, move.promotion);
            if (!ok) {
                parseMoveErrorCnt++;
                return false;
            }

            if (flag & ParseMoveListFlag_create_fen) {
                histList.back().fenString = fenString;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: fromMoveList aborted on corrupt move blob for gameID " << record.gameID << ": "
                  << e.what() << std::endl;
        return false;
    }

    // last position
    if (shouldStop && !hit) {
        auto bb = posToBitboards();
        shouldStop(bb, this, &record);
        delete bb;
    }
    return true;
}

/// OCGDB addition (from Banksia GUI's core): a database's binary move field. A variation is
/// written between two markers, moves from == dest == 1 (one level up) and 2 (one level down).
bool BoardCore::fromMoveList(const PgnRecord& record, const std::vector<int8_t>& moveVec, int flag,
                             PositionCallback shouldStop)
{
    std::lock_guard<std::mutex> dolock(dataMutex);

    /// OCGDB: a 1-byte move names its piece by the piece's index on the board as it is when the
    /// move is made, so Moves1 is decoded move by move (decoding all moves first, as for Moves2,
    /// read the second and later moves against the start position: only the first move came
    /// out). Moves1 holds the main line only.
    if (flag & ParseMoveListFlag_move_size_1_byte) {
        return _fromMoveList1Byte(record, moveVec, flag, shouldStop);
    }

    std::vector<MoveWithLevel> moveWithLevelVec;

    VariationRec vr;
    std::vector<u8> groupIdxVec { 0 };
    Move move;
    auto mainLineCnt = 0;

    // Corrupt/truncated blobs must not crash the reader. Both decoders may read
    // one byte past the current position (decode2Bytes reads a uint16_t;
    // decode1Byte consumes 2 bytes for a queen move), so we parse from a copy
    // padded with two zero bytes. The loop still stops at the real data size, so
    // padding only ever backs an over-read of the final move - never adds moves.
    const auto moveDataSize = moveVec.size();
    std::vector<int8_t> paddedVec;
    paddedVec.reserve(moveDataSize + 2);
    paddedVec.assign(moveVec.begin(), moveVec.end());
    paddedVec.push_back(0);
    paddedVec.push_back(0);
    const int8_t* moveData = paddedVec.data();

    // The decoders index fixed lookup tables with bits taken directly from the
    // blob; corrupt data can push those indices out of range and throw. Catch
    // it here so a bad game degrades to "as many moves as we could decode"
    // instead of taking the whole application down.
    try {
        for (size_t i = 0; i < moveDataSize;) {
            if (flag & ParseMoveListFlag_move_size_1_byte) {
                auto pair = decode1Byte(moveData + i);
                assert(pair.second == 1 || pair.second == 2);
                if (pair.second < 1) break; // decoder failed - stop on bad data
                move = pair.first;
                i += pair.second;
            } else {
                if (i + 2 > moveDataSize) break; // dangling odd byte - truncated blob
                auto q = reinterpret_cast<const uint16_t*>(moveData + i);
                i += 2;

                /// OCGDB: decoded when its move is made (_fromMoveList)
                if (isBoardDependent2Bytes(*q)) {
                    if (vr.level > 0 && (flag & ParseMoveListFlag_ignore_high_levels)) {
                        continue;
                    }
                    vr.groupIdx = groupIdxVec[vr.level];
                    MoveWithLevel mwl(Move::illegalMove, vr);
                    mwl.code = *q;
                    moveWithLevelVec.push_back(mwl);
                    vr.startNewLevel = 0;
                    if (maxParsePly > 0 && vr.level == 0 && ++mainLineCnt >= maxParsePly) {
                        break;
                    }
                    continue;
                }
                move = decode2Bytes(*q);
            }

            if (!isValid(move)) {
                if (move.from == move.dest) {
                    if (move.from == 1) {
                        vr.startNewLevel = 1;
                        vr.level++;
                        if (vr.level >= groupIdxVec.size()) {
                            groupIdxVec.push_back(0);
                        } else {
                            groupIdxVec[vr.level]++;
                        }
                        continue;
                    }

                    if (move.from == 2) {
                        assert(vr.level > 0);
                        if (vr.level > 0) {
                            vr.level--;
                        }
                        continue;
                    }
                }

                break;
            }

            if (vr.level > 0 && (flag & ParseMoveListFlag_ignore_high_levels)) {
                continue;
            }

            vr.groupIdx = groupIdxVec[vr.level];

            moveWithLevelVec.push_back(MoveWithLevel(move, vr));
            vr.startNewLevel = 0;

            if (maxParsePly > 0 && vr.level == 0 && ++mainLineCnt >= maxParsePly) {
                break;
            }
        }
    } catch (const std::exception& e) {
        assert(false);
        std::cerr << "Warning: fromMoveList aborted on corrupt move blob for gameID " << record.gameID << ": "
                  << e.what() << std::endl;
    }

    /// a broken variation is dropped and the game parsed again (BoardCore::eraseVariation)
    const auto freshStart = histList.empty();
    for (auto tries = 0;; ++tries) {
        auto ok = _fromMoveList(record, flag, shouldStop, 0, moveWithLevelVec);
        if (ok || brokenVariationIdx < 0 || !freshStart || tries >= 64) {
            return ok;
        }
        eraseVariation(moveWithLevelVec, brokenVariationIdx);
        _newGame(record.fenText);
    }
}

/// OCGDB addition (from Banksia GUI's core)
DupRec BoardCore::createCheckingDuplicateRec(int fromPly) const
{
    auto line = _mainLineHists(); /// the game's moves, not its variations'
    auto sz = line.size();
    DupRec rec;

    if (sz > 0 && sz > static_cast<size_t>(fromPly)) {
        auto hist = *line.at(fromPly);

        rec.moves[0] = encode2Bytes(hist);

        if (sz - 1 > static_cast<size_t>(fromPly)) {
            auto hist = *line.at(sz - 1);
            rec.moves[3] = encode2Bytes(hist);

            auto l = (sz - 1 - fromPly) / 3;
            if (l > 0) {
                hist = *line.at(fromPly + l);
                rec.moves[1] = encode2Bytes(hist);

                hist = *line.at(fromPly + 2 * l);
                rec.moves[2] = encode2Bytes(hist);
            }
        }
    }

    return rec;
}

std::set<ChessVariant> BoardCore::detectVariantFromFENString(const std::string& fenString)
{
    std::set<ChessVariant> theSet;

    auto vec = splitString(fenString, ' ');
    auto sz = vec.size();
    if (!sz) return theSet;

    std::vector<char> board;
    [[maybe_unused]] auto slashCnt = 0, jieqiCnt = 0, errCnt = 0;
    for (auto ch : vec.at(0)) {
        if (ch == '/') {
            slashCnt++;
        } else {
            if (std::isalpha(ch)) {
                board.push_back(ch);
                if (ch == 'X' || ch == 'x') jieqiCnt++;
            } else if (std::isdigit(ch)) {
                int k = ch - '0';
                for (auto i = 0; i < k; ++i)
                    board.push_back(' ');
            } else {
                errCnt++;
                if (errCnt > 3) return theSet;
            }
        }
    }

    // not FEN or not known variant
    auto boardsz = board.size();
    if (slashCnt < 4 || (boardsz != 64 && boardsz != 90)) {
        return theSet;
    }


    if (boardsz == 64) {
        // Check castling
        if (sz >= 3 && vec.at(2) != "-") {
            auto s = vec.at(2);

            [[maybe_unused]] auto stdCnt = 0;
            for (auto&& ch : s) {
                [[maybe_unused]] auto v = ChessVariant::standard;
                if (std::isalpha(ch)) {
                    switch (ch) {
                        case 'K':
                            if (board.at(60) != 'K' || board.at(63) != 'R') {
                                theSet.insert(ChessVariant::chess960);
                                return theSet;
                            }
                            stdCnt++;
                            break;
                        case 'Q':
                            if (board.at(60) != 'K' || board.at(56) != 'R') {
                                theSet.insert(ChessVariant::chess960);
                                return theSet;
                            }
                            stdCnt++;
                            break;
                        case 'k':
                            if (board.at(4) != 'k' || board.at(7) != 'r') {
                                theSet.insert(ChessVariant::chess960);
                                return theSet;
                            }
                            stdCnt++;
                            break;
                        case 'q':
                            if (board.at(4) != 'k' || board.at(0) != 'r') {
                                theSet.insert(ChessVariant::chess960);
                                return theSet;
                            }
                            stdCnt++;
                            break;
                        default:
                            theSet.insert(ChessVariant::chess960);
                            return theSet;
                    }
                }
            }
        }

        // Not enough info, both should be OK
        theSet.insert(ChessVariant::standard);
        theSet.insert(ChessVariant::chess960);
        return theSet;
    }

#ifdef _JIEQI_
    theSet.insert(jieqiCnt ? ChessVariant::jieqi : ChessVariant::xq);
#else
    theSet.insert(ChessVariant::xq);
#endif
    return theSet;
}


std::string BoardCore::getEcoBriefString(u64 key) const
{
    return getEco(key).toBriefString();
}


/// OCGDB: of the main line -- a variation's opening is not the game's
Eco BoardCore::getLastEco() const
{
    Eco eco;
    for (auto&& hist : histList) {
        if (hist.variationRec.level > 0) {
            continue;
        }
        auto e = getEco(hist.hashKey);
        if (!e.isEmpty()) {
            eco = e;
        }
    }

    /// OCGDB: hist.hashKey is the key before its move, so the position after the last move is the
    /// board's own one (when that move is on the main line)
    if (!histList.empty() && histList.back().variationRec.level == 0) {
        auto e = getEco(hashKey);
        if (!e.isEmpty()) {
            eco = e;
        }
    }

    return eco;
}


std::vector<MoveFull> BoardCore::probeEgtbAll() const
{
    return std::vector<MoveFull>();
}
