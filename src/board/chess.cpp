/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <random>
#include <iomanip> // for setprecision
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "chess.h"


namespace bslib {

    const char* pieceTypeFullNames[8] = {
        "", "king", "queen", "rook", "bishop", "knight", "pawn", ""
    };
}

using namespace bslib;

extern uint64_t polyglotRandom64[800];
extern uint64_t *RandomCastle;

uint64_t *RandomPiece     = polyglotRandom64;
uint64_t *RandomCastle    = polyglotRandom64 + 768;
uint64_t *RandomEnPassant = polyglotRandom64 + 772;
uint64_t *RandomTurn      = polyglotRandom64 + 780;

ChessBoard::ChessBoard(ChessVariant _variant)
{
    variant = _variant;
    assert(Funcs::isChessFamily(variant));

    pieces.clear();
    for(int i = 0; i < 64; i++) {
        pieces.push_back(Piece::emptyPiece);
    }
}

ChessBoard::ChessBoard(const ChessBoard& other)
{
    _clone(&other);
}

ChessBoard::~ChessBoard()
{
}

int ChessBoard::columnCount() const
{
    return 8;
}

int ChessBoard::getColumn(int pos) const
{
    return pos & 7;
}

int ChessBoard::getRank(int pos) const
{
    return pos >> 3;
}

int ChessBoard::coordinateStringToPos(const std::string& str) const
{
    return Funcs::chessCoordinateStringToPos(str);
}

std::string ChessBoard::posToCoordinateString(int pos) const
{
    return Funcs::chessPosToCoordinateString(pos);
}


Move ChessBoard::chessMoveFromCoordiateString(const std::string& moveString)
{
    return Move(moveString, ChessVariant::standard);
}

bool ChessBoard::canLocatePiece(int type, Side, int pos) const
{
    if (type == static_cast<int>(PieceTypeStd::pawn)) {
        auto r = pos / 8;
        return r > 0 && r < 7;
    }
    return true;
}

bool ChessBoard::isValidCastleRights() const
{
    if (castleRights[B]) {
        if (!_isPiece(castleRights_column_king, KING, Side::black)) {
            return false;
        }
        if (((castleRights[B] & CastleRight_long ) && !_isPiece(castleRights_column_rook_left, static_cast<int>(PieceTypeStd::rook), Side::black)) ||
            ((castleRights[B] & CastleRight_short) && !_isPiece(castleRights_column_rook_right, static_cast<int>(PieceTypeStd::rook), Side::black))) {
            return false;
        }
    }

    if (castleRights[W]) {
        if (!_isPiece(56 + castleRights_column_king, KING, Side::white)) {
            return false;
        }
        if (((castleRights[W] & CastleRight_long ) && !_isPiece(56 + castleRights_column_rook_left, static_cast<int>(PieceTypeStd::rook), Side::white)) ||
            ((castleRights[W] & CastleRight_short) && !_isPiece(56 + castleRights_column_rook_right, static_cast<int>(PieceTypeStd::rook), Side::white))) {
            return false;
        }
    }
    return true;
}

bool ChessBoard::isValid() const
{
    int pieceCout[2][7] = { { 0, 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0, 0} };
    
    for (int i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            continue;
        }
        
        pieceCout[static_cast<int>(piece.side)][static_cast<int>(piece.type)] += 1;
        if (piece.type == static_cast<int>(PieceTypeStd::pawn)) {
            if (i < 8 || i >= 56) {
                return false;
            }
        }
    }
    
    if (castleRights[0] + castleRights[1] && !isValidCastleRights()) {
        return false;
    }
    
    if (enpassant > 0) {
        auto row = getRank(enpassant);
        if (row != 2 && row != 5) {
            return false;
        }
        auto pawnPos = row == 2 ? (enpassant + 8) : (enpassant - 8);
        if (!_isPiece(pawnPos, static_cast<int>(PieceTypeStd::pawn), row == 2 ? Side::black : Side::white)) {
            return false;
        }
    }
    
    bool b =
    pieceCout[0][1] == 1 && pieceCout[1][1] == 1 &&     // king
    pieceCout[0][2] <= 9 && pieceCout[1][2] <= 9 &&     // queen
    pieceCout[0][3] <= 10 && pieceCout[1][3] <= 10 &&     // rook
    pieceCout[0][4] <= 10 && pieceCout[1][4] <= 10 &&     // bishop
    pieceCout[0][5] <= 10 && pieceCout[1][5] <= 10 &&     // knight
    pieceCout[0][6] <= 8 && pieceCout[1][6] <= 8 &&       // pawn
    pieceCout[0][2]+pieceCout[0][3]+pieceCout[0][4]+pieceCout[0][5] + pieceCout[0][6] <= 15 &&
    pieceCout[1][2]+pieceCout[1][3]+pieceCout[1][4]+pieceCout[1][5] + pieceCout[1][6] <= 15;

    return b;
}

std::string ChessBoard::toString() const
{
    
    std::ostringstream stringStream;
    
    stringStream << getFen() << std::endl;
    
    for (int i = 0; i<64; i++) {
        auto piece = _getPiece(i);
        
        stringStream << toString(Piece(piece.type, piece.side)) << " ";
        
        if (i > 0 && getColumn(i) == 7) {
            int row = 8 - getRank(i);
            stringStream << " " << row << "\n";
        }
    }
    
    stringStream << "a b c d e f g h  " << Funcs::side2String(side, false) << std::endl;
    stringStream << "key: " << key() << std::endl;
    
    return stringStream.str();
}

void ChessBoard::checkEnpassant()
{
    if ((enpassant >= 16 && enpassant < 24) || (enpassant >= 40 && enpassant < 48))  {
        return;
    }
    enpassant = -1;
}

void ChessBoard::setFenCastleRights_clear()
{
    castleRights[0] = castleRights[1] = 0;
}

void ChessBoard::setFenCastleRights(const std::string& str) {
    for(auto && ch : str) {
        switch (ch) {
            case 'K':
                castleRights[W] |= CastleRight_short;
                break;
            case 'k':
                castleRights[B] |= CastleRight_short;
                break;
            case 'Q':
                castleRights[W] |= CastleRight_long;
                break;
            case 'q':
                castleRights[B] |= CastleRight_long;
                break;

            default:
                break;
        }
    }
}

void ChessBoard::_setFen(const std::string& fen)
{
    reset();
    
    std::string str = fen;
    startFen = fen;
    auto originalFen = Funcs::getOriginFen(variant);
    if (fen.empty()) {
        str = originalFen;
    } else {
        if (memcmp(fen.c_str(), originalFen.c_str(), originalFen.size()) == 0) {
            startFen = "";
        }
    }
    
    side = Side::none;
    enpassant = -1;
    status = 0;
    setFenCastleRights_clear();
    
    auto vec = Funcs::splitString(str, ' ');
    auto thefen = vec.front();
    
    for (size_t i = 0, pos = 0; i < thefen.length(); i++) {
        char ch = thefen.at(i);
        
        if (ch=='/') {
            //std::string str = fen.substr();
            continue;
        }
        
        if (ch>='0' && ch <= '8') {
            int num = ch - '0';
            pos += num;
            continue;
        }
        
        Side side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }
        
        auto pieceType = charactorToPieceType(ch);
        
        if (pieceType != EMPTY) {
            _setPiece(int(pos), Piece(pieceType, side));
        }
        pos++;
    }
    
    // side
    if (vec.size() >= 2) {
        auto str = vec.at(1);
        side = str.length() > 0 && str.at(0) == 'w' ? Side::white : Side::black;
    }
    
    // castle rights
    if (vec.size() >= 3 && vec.at(2) != "-") {
        auto str = vec.at(2);
        setFenCastleRights(str);
    }
    
    // enpassant
    if (vec.size() >= 4 && vec.at(3).size() >= 2) {
        // enpassant
        auto str = vec.at(3);
        auto pos = coordinateStringToPos(str.c_str());
        if (isPositionValid(pos)) {
            enpassant = pos;
        }
    }
    
    // Half move
    quietCnt = 0;
    if (vec.size() >= 5) {
        // half move
        auto str = vec.at(4);
        auto k = std::atoi(str.c_str());
        if (k >= 0 && k <= 50) {
            quietCnt = k * 2;
            if (side == Side::black) quietCnt++;
        }
    }

    // full move
    if (vec.size() >= 6) {
        auto str = vec.at(5);
        auto k = std::atoi(str.c_str());
        if (k >= 1 && k <= 2000) {
            fullMoveCnt = k * 2;
        }
    }

    checkEnpassant();    
    setupPieceIndexes();

    hashKey = initHashKey();
}

bool ChessBoard::isFenValid(const std::string& fen) const
{
    return isChessFenValid(fen);
}

bool ChessBoard::isChessFenValid(const std::string& fen)
{
    if (fen.length() < 10) {
        return false;
    }

    int pieceCnt[2][10];
    memset(pieceCnt, 0, sizeof (pieceCnt));

    auto vec = Funcs::splitString(fen, ' ');
    auto thefen = vec.front();

    auto errCnt = 0, pos = 0;
    for (size_t i = 0; i < thefen.length(); i++) {
        char ch = thefen.at(i);

        if (ch=='/') {
            continue;
        }

        if (ch>='0' && ch <= '8') {
            int num = ch - '0';
            pos += num;
            continue;
        }

        auto side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }

        auto pieceType = Funcs::chessCharactorToPieceType(ch);
        if (pieceType == EMPTY) {
            errCnt++;
        } else {
            pieceCnt[static_cast<int>(side)][static_cast<int>(pieceType)]++;
        }
        pos++;
    }

    return pos > 50 && pos <= 64 && errCnt < 3 && pieceCnt[0][KING] == 1 && pieceCnt[1][KING] == 1;
}

std::string ChessBoard::getFenCastleRights() const {
    std::string s;
    if (castleRights[W] + castleRights[B]) {
        if (castleRights[W] & CastleRight_short) {
            s += "K";
        }
        if (castleRights[W] & CastleRight_long) {
            s += "Q";
        }
        if (castleRights[B] & CastleRight_short) {
            s += "k";
        }
        if (castleRights[B] & CastleRight_long) {
            s += "q";
        }
    } else {
        s = "-";
    }

    return s;
}

std::string ChessBoard::getFen(int halfCount, int fullMoveCount, FENCharactorSet) const
{
    std::ostringstream stringStream;
    
    int e = 0;
    for (int i=0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            e += 1;
        } else {
            if (e) {
                stringStream << e;
                e = 0;
            }
            stringStream << toString(piece);
        }
        
        if (i % 8 == 7) {
            if (e) {
                stringStream << e;
            }
            if (i < 63) {
                stringStream << "/";
            }
            e = 0;
        }
    }
    
    stringStream << (side == Side::white ? " w " : " b ")
                 << getFenCastleRights() << " ";

    if (enpassant > 0) {
        stringStream << posToCoordinateString(enpassant);
    }
    
    if (halfCount >= 0 && fullMoveCount >= 0) {
        stringStream << " " << halfCount << " " << fullMoveCount;
    }
    
    return stringStream.str();
}

void ChessBoard::gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool captureOnly) const
{
    auto toSide = _getPiece(dest).side;
    auto movingPiece = _getPiece(from);
    auto fromSide = movingPiece.side;
    
    if (fromSide != toSide && (!captureOnly || toSide != Side::none)) {
        moveList.push_back(MoveFull(movingPiece, from, dest));
    }
}

void ChessBoard::gen_addPawnMove(std::vector<MoveFull>& moveList, int from, int dest, bool captureOnly) const
{
    auto toSide = _getPiece(dest).side;
    auto movingPiece = _getPiece(from);
    auto fromSide = movingPiece.side;
    
    assert(movingPiece.type == static_cast<int>(PieceTypeStd::pawn));
    if (fromSide != toSide && (!captureOnly || toSide != Side::none)) {
        if (dest >= 8 && dest < 56) {
            moveList.push_back(MoveFull(movingPiece, from, dest));
        } else {
            moveList.push_back(MoveFull(movingPiece, from, dest, static_cast<int>(PieceTypeStd::queen)));
            moveList.push_back(MoveFull(movingPiece, from, dest, static_cast<int>(PieceTypeStd::rook)));
            moveList.push_back(MoveFull(movingPiece, from, dest, static_cast<int>(PieceTypeStd::bishop)));
            moveList.push_back(MoveFull(movingPiece, from, dest, static_cast<int>(PieceTypeStd::knight)));
        }
    }
}

void ChessBoard::clearCastleRights(int rookPos, Side rookSide) {
    auto col = rookPos % 8;
    if ((col != castleRights_column_rook_left && col != castleRights_column_rook_right)
        || (rookPos > 7 && rookPos < 56)) {
        return;
    }
    auto pos = col + (rookSide == Side::white ? 56 : 0);
    if (pos != rookPos) {
        return;
    }
    auto sd = static_cast<int>(rookSide);
    if (col == castleRights_column_rook_left) {
        castleRights[sd] &= ~CastleRight_long;
    } else if (col == castleRights_column_rook_right) {
        castleRights[sd] &= ~CastleRight_short;
    }
}


bool ChessBoard::_isIncheck(Side beingAttackedSide) const {
    int kingPos = findKing(beingAttackedSide);
    Side attackerSide = xSide(beingAttackedSide);
    return kingPos >= 0 ? beAttacked(kingPos, attackerSide) : false;
}

////////////////////////////////////////////////////////////////////////

void ChessBoard::_gen(std::vector<MoveFull>& moves, Side side) const
{
    moves.reserve(MaxMoveBranch);

    auto captureOnly = false;

    for (int pos = 0; pos < 64; ++pos) {
        auto piece = pieces[pos];

        if (piece.side != side) {
            continue;
        }

        switch (static_cast<PieceTypeStd>(piece.type)) {
            case PieceTypeStd::king:
            {
                genBishop(moves, side, pos, true);
                genRook(moves, side, pos, true);

                if (!captureOnly) {
                    gen_castling(moves, pos);
                }
                break;
            }

            case PieceTypeStd::queen:
            {
                genBishop(moves, side, pos, false);
                genRook(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::bishop:
            {
                genBishop(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::rook: // both queen and rook here
            {
                genRook(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::knight:
            {
                genKnight(moves, side, pos);
                break;
            }

            case PieceTypeStd::pawn:
            {
                genPawn(moves, side, pos);
                break;
            }

            default:
                break;
        }
    }
}

void ChessBoard::genKnight(std::vector<MoveFull>& moves, Side, int pos) const
{
    auto captureOnly = false;
    auto col = getColumn(pos);
    auto y = pos - 6;

    if (y >= 0 && col < 6)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos - 10;
    if (y >= 0 && col > 1)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos - 15;
    if (y >= 0 && col < 7)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos - 17;
    if (y >= 0 && col > 0)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos + 6;
    if (y < 64 && col > 1)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos + 10;
    if (y < 64 && col < 6)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos + 15;
    if (y < 64 && col > 0)
        gen_addMove(moves, pos, y, captureOnly);
    y = pos + 17;
    if (y < 64 && col < 7)
        gen_addMove(moves, pos, y, captureOnly);
}

void ChessBoard::genRook(std::vector<MoveFull>& moves, Side, int pos, bool oneStep) const
{
    auto captureOnly = false;
    auto col = getColumn(pos);
    for (int y = pos - 1; y >= pos - col; y--) { /* go left */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }

    for (int y = pos + 1; y < pos - col + 8; y++) { /* go right */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }

    for (int y = pos - 8; y >= 0; y -= 8) { /* go up */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }

    for (int y = pos + 8; y < 64; y += 8) { /* go down */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
}

void ChessBoard::genBishop(std::vector<MoveFull>& moves, Side, int pos, bool oneStep) const
{
    auto captureOnly = false;
    for (int y = pos - 9; y >= 0 && getColumn(y) != 7; y -= 9) {        /* go left up */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos - 7; y >= 0 && getColumn(y) != 0; y -= 7) {        /* go right up */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos + 9; y < 64 && getColumn(y) != 0; y += 9) {        /* go right down */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos + 7; y < 64 && getColumn(y) != 7; y += 7) {        /* go right down */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
}

void ChessBoard::genPawn(std::vector<MoveFull>& moves, Side side, int pos) const
{
    auto captureOnly = false;
    auto col = getColumn(pos);

    if (side == Side::black) {
        if (!captureOnly && _isEmpty(pos + 8)) {
            gen_addPawnMove(moves, pos, pos + 8, captureOnly);
        }
        if (!captureOnly && pos < 16 && _isEmpty(pos + 8) && _isEmpty(pos + 16)) {
            gen_addMove(moves, pos, pos + 16, captureOnly);
        }

        if (col && (_getPiece(pos + 7).side == Side::white || (pos + 7 == enpassant && _getPiece(pos + 7).side == Side::none))) {
            gen_addPawnMove(moves, pos, pos + 7, captureOnly);
        }
        if (col < 7 && (_getPiece(pos + 9).side == Side::white || (pos + 9 == enpassant && _getPiece(pos + 9).side == Side::none))) {
            gen_addPawnMove(moves, pos, pos + 9, captureOnly);
        }
    } else {
        if (!captureOnly && _isEmpty(pos - 8)) {
            gen_addPawnMove(moves, pos, pos - 8, captureOnly);
        }
        if (!captureOnly && pos >= 48 && _isEmpty(pos - 8) && _isEmpty(pos - 16)) {
            gen_addMove(moves, pos, pos - 16, captureOnly);
        }

        if (col < 7 && (_getPiece(pos - 7).side == Side::black || (pos - 7 == enpassant && _getPiece(pos - 7).side == Side::none)))
            gen_addPawnMove(moves, pos, pos - 7, captureOnly);
        if (col && (_getPiece(pos - 9).side == Side::black || (pos - 9 == enpassant && _getPiece(pos - 9).side == Side::none)))
            gen_addPawnMove(moves, pos, pos - 9, captureOnly);
    }
}

void ChessBoard::gen_castling(std::vector<MoveFull>& moves, int kingPos) const
{
    if ((kingPos != 4 || !castleRights[B]) && (kingPos != 60 || !castleRights[W])) {
        return;
    }

    bool captureOnly = false;

    if (kingPos == 4) {
        if ((castleRights[B] & CastleRight_long) &&
            pieces[1].isEmpty() && pieces[2].isEmpty() && pieces[3].isEmpty() &&
            !beAttacked(2, Side::white) && !beAttacked(3, Side::white) && !beAttacked(4, Side::white)) {
            assert(_isPiece(0, static_cast<int>(PieceTypeStd::rook), Side::black));
            gen_addMove(moves, 4, 2, captureOnly);
        }
        if ((castleRights[B] & CastleRight_short) &&
            pieces[5].isEmpty() && pieces[6].isEmpty() &&
             !beAttacked(4, Side::white) && !beAttacked(5, Side::white) && !beAttacked(6, Side::white)) {
            assert(_isPiece(7, static_cast<int>(PieceTypeStd::rook), Side::black));
            gen_addMove(moves, 4, 6, captureOnly);
        }
    } else {
        if ((castleRights[W] & CastleRight_long) &&
            pieces[57].isEmpty() && pieces[58].isEmpty() && pieces[59].isEmpty() &&
            !beAttacked(58, Side::black) && !beAttacked(59, Side::black) && !beAttacked(60, Side::black)) {
            assert(_isPiece(56, static_cast<int>(PieceTypeStd::rook), Side::white));
            gen_addMove(moves, 60, 58, captureOnly);
        }
        if ((castleRights[W] & CastleRight_short) &&
            pieces[61].isEmpty() && pieces[62].isEmpty() &&
            !beAttacked(60, Side::black) && !beAttacked(61, Side::black) && !beAttacked(62, Side::black)) {
            assert(_isPiece(63, static_cast<int>(PieceTypeStd::rook), Side::white));
            gen_addMove(moves, 60, 62, captureOnly);
        }
    }
}

bool ChessBoard::beAttacked(int pos, Side attackerSide) const
{
    int row = getRank(pos), col = getColumn(pos);
    /* Check attacking of Knight */
    if (col > 0 && row > 1 && _isPiece(pos - 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col < 7 && row > 1 && _isPiece(pos - 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col > 1 && row > 0 && _isPiece(pos - 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col < 6 && row > 0 && _isPiece(pos - 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col > 1 && row < 7 && _isPiece(pos + 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col < 6 && row < 7 && _isPiece(pos + 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col > 0 && row < 6 && _isPiece(pos + 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    if (col < 7 && row < 6 && _isPiece(pos + 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        return true;
    
    /* Check horizontal and vertical lines for attacking of Queen, Rook, King */
    /* go down */
    for (int y = pos + 8; y < 64; y += 8) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::rook) ||
                    (piece.type == static_cast<int>(PieceTypeStd::king) && y == pos + 8)) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go up */
    for (int y = pos - 8; y >= 0; y -= 8) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::rook) ||
                    (piece.type == KING && y == pos - 8)) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go left */
    for (int y = pos - 1; y >= pos - col; y--) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::rook) ||
                    (piece.type == KING && y == pos - 1)) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go right */
    for (int y = pos + 1; y < pos - col + 8; y++) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::rook) ||
                    (piece.type == KING && y == pos + 1)) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* Check diagonal lines for attacking of Queen, Bishop, King, Pawn */
    /* go right down */
    for (int y = pos + 9; y < 64 && getColumn(y) != 0; y += 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::bishop) ||
                    (y == pos + 9 && (piece.type == KING || (piece.type == static_cast<int>(PieceTypeStd::pawn) && piece.side == Side::white)))) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go left down */
    for (int y = pos + 7; y < 64 && getColumn(y) != 7; y += 7) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::bishop) ||
                    (y == pos + 7 && (piece.type == KING || (piece.type == static_cast<int>(PieceTypeStd::pawn) && piece.side == Side::white)))) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go left up */
    for (int y = pos - 9; y >= 0 && getColumn(y) != 7; y -= 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::bishop) ||
                    (y == pos - 9 && (piece.type == KING || (piece.type == static_cast<int>(PieceTypeStd::pawn) && piece.side == Side::black)))) {
                    return true;
                }
            }
            break;
        }
    }
    
    /* go right up */
    for (int y = pos - 7; y >= 0 && getColumn(y) != 0; y -= 7) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen) || piece.type == static_cast<int>(PieceTypeStd::bishop) ||
                    (y == pos - 7 && (piece.type == KING || (piece.type == static_cast<int>(PieceTypeStd::pawn) && piece.side == Side::black)))) {
                    return true;
                }
            }
            break;
        }
    }
    
    return false;
}

void ChessBoard::_make(const MoveFull& move, Hist& hist)
{
    hist.enpassant = enpassant;
    hist.status = status;
    hist.castleRights[0] = castleRights[0];
    hist.castleRights[1] = castleRights[1];
    hist.castled = 0;
    hist.move = move;
    hist.cap = pieces[move.dest];
    hist.hashKey = hashKey;
    hist.quietCnt = quietCnt;
    
    hashKey ^= hashKeyEnpassant(enpassant);
    
    hashKey ^= xorHashKey(move.from);
    if (!hist.cap.isEmpty()) {
        hashKey ^= xorHashKey(move.dest);
    }
    
    auto p = pieces[move.from];
    pieces[move.dest] = p;
    pieces[move.from].setEmpty();
    hist.move.piece = p;
    
    hashKey ^= xorHashKey(move.dest);
    
    quietCnt++;
    enpassant = -1;
    
    if ((castleRights[0] + castleRights[1]) && hist.cap.type == static_cast<int>(PieceTypeStd::rook)) {
        clearCastleRights(move.dest, hist.cap.side);
    }
    
    switch (static_cast<PieceTypeStd>(p.type)) {
        case PieceTypeStd::king: {
            castleRights[static_cast<int>(p.side)] &= ~(CastleRight_long|CastleRight_short);
            if (abs(move.from - move.dest) == 2) { // castle
                auto rookPos = move.from + (move.from < move.dest ? 3 : -4);
                assert(pieces[rookPos].type == static_cast<int>(PieceTypeStd::rook));
                int newRookPos = (move.from + move.dest) / 2;
                assert(pieces[newRookPos].isEmpty());

                hashKey ^= xorHashKey(rookPos);
                pieces[newRookPos] = pieces[rookPos];
                pieces[rookPos].setEmpty();
                hashKey ^= xorHashKey(newRookPos);
                quietCnt = 0;
                hist.castled = move.dest == 2 || move.dest == 56 + 2 ? CastleRight_long : CastleRight_short;
            }
            break;
        }
            
        case PieceTypeStd::rook: {
            if (castleRights[0] + castleRights[1]) {
                clearCastleRights(move.from, p.side);
            }
            break;
        }
            
        case PieceTypeStd::pawn: {
            int d = abs(move.from - move.dest);
            quietCnt = 0;

            if (d == 16) {
                enpassant = (move.from + move.dest) / 2;
            } else if (move.dest == hist.enpassant) {
                int ep = move.dest + (p.side == Side::white ? +8 : -8);
                hist.cap = pieces[ep];
                
                hashKey ^= xorHashKey(ep);
                pieces[ep].setEmpty();
            } else {
                if (move.promotion != EMPTY) {
                    hashKey ^= xorHashKey(move.dest);
                    pieces[move.dest].type = move.promotion;
                    hashKey ^= xorHashKey(move.dest);
                }
            }
            break;
        }
        default:
            break;
    }
    
    if (!hist.cap.isEmpty()) {
        quietCnt = 0;
    }
    
    if (hist.castleRights[W] != castleRights[W]) {
        if ((hist.castleRights[W] & CastleRight_short) != (castleRights[W] & CastleRight_short)) {
            hashKey ^= RandomCastle[0];
            quietCnt = 0;
        }
        if ((hist.castleRights[W] & CastleRight_long) != (castleRights[W] & CastleRight_long)) {
            hashKey ^= RandomCastle[1];
            quietCnt = 0;
        }
    }
    if (hist.castleRights[B] != castleRights[B]) {
        if ((hist.castleRights[B] & CastleRight_short) != (castleRights[B] & CastleRight_short)) {
            hashKey ^= RandomCastle[2];
            quietCnt = 0;
        }
        if ((hist.castleRights[B] & CastleRight_long) != (castleRights[B] & CastleRight_long)) {
            hashKey ^= RandomCastle[3];
            quietCnt = 0;
        }
    }
    
    hashKey ^= hashKeyEnpassant(enpassant);
    assert(hist.move.piece.idx == pieces[move.dest].idx);
}

void ChessBoard::_takeBack(const Hist& hist)
{
//    auto movep = _getPiece(hist.move.dest);
//    _setPiece(hist.move.from, movep);
    
    auto movep = pieces[hist.move.dest];
    pieces[hist.move.from] = movep;

    int capPos = hist.move.dest;

    if (movep.type == static_cast<int>(PieceTypeStd::pawn) && hist.enpassant == hist.move.dest) {
        capPos = hist.move.dest + (movep.side == Side::white ? +8 : -8);
        _setEmpty(hist.move.dest);
    }
//    _setPiece(capPos, hist.cap);
    pieces[capPos] = hist.cap;

    if (movep.type == static_cast<int>(PieceTypeStd::king)) {
        if (abs(hist.move.from - hist.move.dest) == 2) {
            assert(hist.castled == CastleRight_long || hist.castled == CastleRight_short);
            int rookPos = hist.move.from + (hist.move.from < hist.move.dest ? 3 : -4);
            assert(_isEmpty(rookPos));

            int newRookPos = (hist.move.from + hist.move.dest) / 2;
//            _setPiece(rookPos, Piece(static_cast<int>(PieceTypeStd::rook), hist.move.dest < 8 ? Side::black : Side::white));
            pieces[rookPos] = pieces[newRookPos];
            _setEmpty(newRookPos);
        }
    }
    
    if (hist.move.promotion != EMPTY) {
//        _setPiece(hist.move.from, Piece(static_cast<int>(PieceTypeStd::pawn), hist.move.dest < 8 ? Side::white : Side::black));
        pieces[hist.move.from].type = static_cast<int>(PieceTypeStd::pawn);
    }
    
    status = hist.status;
    castleRights[0] = hist.castleRights[0];
    castleRights[1] = hist.castleRights[1];
    enpassant = hist.enpassant;
    quietCnt = hist.quietCnt;
    
    hashKey = hist.hashKey;

    assert(hist.move.piece.idx == pieces[hist.move.from].idx);
    assert(hist.cap.isEmpty() || hist.cap.idx == pieces[capPos].idx);
}

Result ChessBoard::rule()
{
    std::lock_guard<std::mutex> dolock(dataMutex);

    assert(_isHashKeyValid());
    Result result;
    
    // Mated or stalemate
    auto haveLegalMove = false;
    std::vector<MoveFull> moveList;
    _gen(moveList, side);
    for(auto && move : moveList) {
        Hist hist;
        _make(move, hist);
        if (!_isIncheck(side)) {
            haveLegalMove = true;
        }
        _takeBack(hist);
        if (haveLegalMove) break;
    }
    
    if (!haveLegalMove) {
        if (_isIncheck(side)) {
            result.result = side == Side::white ? ResultType::loss : ResultType::win;
            result.reason = ReasonType::mate;
        } else {
            result.result = ResultType::draw;
            result.reason = ReasonType::stalemate;
        }
        return result;
    }
    
    // draw by insufficientmaterial
    int pieceCnt[2][8], bishopColor[2][2];
    memset(pieceCnt, 0, sizeof(pieceCnt));
    memset(bishopColor, 0, sizeof(bishopColor));
    
    auto draw = true;
    for(int i = 0; i < 64; i++) {
        auto p = pieces[i];
        if (p.type <= KING) continue;
        if (p.type != static_cast<int>(PieceTypeStd::bishop) &&
            p.type != static_cast<int>(PieceTypeStd::knight)) {
            draw = false;
            break;
        }

        auto sd = static_cast<int>(p.side);
        auto t = p.type;
        pieceCnt[sd][t]++;

        if (p.type == static_cast<int>(PieceTypeStd::bishop)) {
            auto c = (getColumn(i) + getRank(i)) & 1;
            bishopColor[sd][c]++;
            if ((bishopColor[sd][0] && bishopColor[sd][1]) || pieceCnt[sd][static_cast<int>(PieceTypeStd::knight)]) { // bishops in different colors or a knight + a bishop
                draw = false;
                break;
            }
        } else if (pieceCnt[sd][t]) {
            if (pieceCnt[sd][t] > 1 || pieceCnt[sd][static_cast<int>(PieceTypeStd::bishop)]) { // more than 2 knights or a knight + a bishop
                draw = false;
                break;
            }
        }
    }

    if (draw) {
        result.result = ResultType::draw;
        result.reason = ReasonType::insufficientmaterial;
        return result;
    }

    // 50 moves
    if (quietCnt >= 50 * 2) {
        result.result = ResultType::draw;
        result.reason = ReasonType::fiftymoves;
        return result;
    }

    if (quietCnt >= 2 * 4) {
        auto cnt = 0;
        auto i = int(histList.size()), k = i - quietCnt;
        for(i -= 2; i >= 0 && i >= k; i -= 2) {
            auto hist = histList.at(i);
            if (hist.hashKey == hashKey) {
                cnt++;
            }
        }
        if (cnt >= 2) { // 2 + 1 itself = 3 times
            result.result = ResultType::draw;
            result.reason = ReasonType::repetition;
            return result;
        }
    }
    
    return result;
}

// Check and make the move if it is legal
bool ChessBoard::_checkMake(int from, int dest, int promotion)
{
    Move move(from, dest, promotion);
    if (!move.isValid() || !BoardCore::isValid(move)) {
        return false;
    }
    
    if (!_isHashKeyValid()) {
        assert(false);
        return false;
    }

    auto piece = _getPiece(from), cap = _getPiece(dest);
    if (piece.isEmpty()
        || piece.side != side
        || (piece.side == cap.side && (variant != ChessVariant::chess960 || piece.type != KING || cap.type != static_cast<int>(PieceTypeStd::rook)))
        || !Move::isValidPromotion(promotion)) {
        return false;
    }
    
    std::vector<MoveFull> moveList;
    _gen(moveList, side);
    
    for (auto && move : moveList) {
        assert(_isHashKeyValid());
        if (move.from != from || move.dest != dest || move.promotion != promotion) {
            continue;
        }
        
        auto theSide = side;
        auto fullmove = createFullMove(from, dest, promotion);
        _make(fullmove); assert(side != theSide);
        
        if (_isIncheck(theSide)) {
            _takeBack();
            return false;
        }
        
        createStringForLastMove(moveList);
        assert(isValid());
        return true;
    }
    
    return false;
}

bool ChessBoard::_quickCheck_bishop(int from, int dest, bool checkMiddle) const
{
    auto rf = from / 8, ff = from % 8, rd = dest / 8, fd = dest % 8;

    if (abs(rf - rd) != abs(ff - fd)) {
        return false;
    }

    if (checkMiddle) {
        int d;
        if (rf < rd) {
            d = ff < fd ? +9 : +7;
        } else {
            d = ff < fd ? -7 : -9;
        }

        for(auto i = from + d, step = 0; i != dest; i += d, step++) {
            assert(step < 8);
            if (!_isEmpty(i)) {
                return false;
            }
        }
    }
    return true;
}


bool ChessBoard::_quickCheck_king(int from, int dest, bool checkMore) const
{
    auto d = std::abs(from - dest);

    if (d == 2) { // O-O, O-O-O
        auto rf = getRank(from), rd = getRank(dest);
        if (rf == rd && ((from == 60 && side == Side::white) || (from == 4 && side == Side::black))) {
            if (checkMore) {
                if (from < dest) {  // O-O
                    auto rook = _getPiece(from + 3);
                    return _isEmpty(from + 1) && _isEmpty(from + 2)
                            && rook.type == static_cast<int>(PieceTypeStd::rook)
                            && rook.side == _getPiece(from).side;
                } else {             // O-O-O
                    auto rook = _getPiece(from - 4);
                    return _isEmpty(from - 1) && _isEmpty(from - 2)
                            && rook.type == static_cast<int>(PieceTypeStd::rook)
                            && rook.side == _getPiece(from).side;
                }
            }
            return true;
        }

        return false;
    }

    if (d == 1 || (d >= 7 && d <= 9)) {
        auto rf = getRank(from), rd = getRank(dest);
        if (d == 1) {
            return rf == rd;
        }

        return std::abs(rf - rd) == 1;
    }

    return false;

}

// Check and make the move if it is legal
bool ChessBoard::_quickCheckMake(int from, int dest, int promotion, bool createSanString)
{
    Move move(from, dest, promotion);
    if (!BoardCore::isValid(move)) {
        return false;
    }

    auto piece = _getPiece(from), cap = _getPiece(dest);
    if (piece.isEmpty()
        || piece.side != side
        || (piece.side == cap.side && (variant != ChessVariant::chess960 || piece.type != KING || cap.type != static_cast<int>(PieceTypeStd::rook)))
        || !Move::isValidPromotion(promotion)) {
        return false;
    }

    bool checkMore = true;

    switch (static_cast<PieceTypeStd>(piece.type)) {
        case PieceTypeStd::king:
        {
            if (!_quickCheck_king(from, dest, checkMore)) {
                return false;
            }
            break;
        }

        case PieceTypeStd::queen:
        {
            if (_quickCheck_rook(from, dest, checkMore) || _quickCheck_bishop(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::bishop:
        {
            if (_quickCheck_bishop(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::rook:
        {
            if (_quickCheck_rook(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::knight:
        {
            auto r = from / 8, f = from % 8;

            switch (from - dest) {
            case 17:
                if (f == 0 || r < 2) {
                    return false;
                }
                break;

            case 15:
                if (f == 7 || r < 2) {
                    return false;
                }
                break;

            case 10:
                if (f < 2 || r < 1) {
                    return false;
                }
                break;
            case 6:
                if (f > 5 || r < 1) {
                    return false;
                }
                break;

            case -6:
                if (f < 2 || r > 6) {
                    return false;
                }
                break;

            case -10:
                if (f > 5 || r > 6) {
                    return false;
                }
                break;


            case -15:
                if (f == 0 || r > 5) {
                    return false;
                }
                break;

            case -17:
                if (f == 7 || r > 5) {
                    return false;
                }
                break;
            default:
                return false;
            }
            break;
        }

        case PieceTypeStd::pawn:
        {
            auto d = std::abs(from - dest);
            if (d != 16 && (d < 7 || d > 9)) {
                return false;
            }

            if (side == Side::white) {
                if (from < dest) {
                    return false;
                }
                if (d == 16) {
                    if (from < 48 || from > 55 || !cap.isEmpty()) {
                        return false;
                    }
                    break;
                }
                if (getRank(from) - getRank(dest) != 1) {
                    return false;
                }
            } else {
                if (from > dest) {
                    return false;
                }
                if (d == 16) {
                    if (from < 8 || from > 15 || !cap.isEmpty()) {
                        return false;
                    }
                    break;
                }
                if (getRank(from) - getRank(dest) != -1) {
                    return false;
                }
            }
            break;
        }

        default:
            break;
    }

    auto theSide = side;
    auto fullmove = createFullMove(from, dest, promotion);
    _make(fullmove); assert(side != theSide);

    if (checkMore && _isIncheck(theSide)) {
        _takeBack();
        return false;
    }

    if (createSanString) {
        createSanStringForLastMove();
    }

    assert(isValid());
    return true;
}


// don't penetrate the origin pos (from)
std::vector<int> ChessBoard::_attackByRook(int from, int dest, const Piece& movePiece)
{
    std::vector<int> vec;
    auto col = getColumn(dest);

    /* go down */
    for (int y = dest + 8; y < 64 && y != from; y += 8) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go up */
    for (int y = dest - 8; y >= 0 && y != from; y -= 8) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go left */
    for (int y = dest - 1; y >= dest - col && y != from; y--) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go right */
    for (int y = dest + 1; y < dest - col + 8 && y != from; y++) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    return vec;
}

// don't penetrate the origin pos (from)
std::vector<int> ChessBoard::_attackByBishop(int from, int dest, const Piece& movePiece)
{
    std::vector<int> vec;
//    auto col = getColumn(pos);

    /* go right down */
    for (int y = dest + 9; y < 64 && getColumn(y) != 0 && y != from; y += 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go left down */
    for (int y = dest + 7; y < 64 && getColumn(y) != 7 && y != from; y += 7) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go left up */
    for (int y = dest - 9; y >= 0 && getColumn(y) != 7 && y != from; y -= 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go right up */
    for (int y = dest - 7; y >= 0 && getColumn(y) != 0 && y != from; y -= 7) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    return vec;
}

std::vector<int> ChessBoard::_attackByQueen(int from, int dest, const Piece& movePiece)
{
    auto v0 = _attackByRook(from, dest, movePiece);
    auto v1 = _attackByBishop(from, dest, movePiece);
    v0.insert(v0.end(), v1.begin(), v1.end());
    return v0;
}

std::vector<int> ChessBoard::_attackByPawn(int from, int dest, const Piece& movePiece, const Piece& cap, int enpassant)
{
    std::vector<int> vec;
    auto col = getColumn(dest);

    if (movePiece.side == Side::white) {
        auto p = _getPiece(dest + 8);
        if (p == movePiece) vec.push_back(dest + 8);
        else
        if (p.isEmpty() && dest >= 32 && dest < 40 && dest + 8 != from) {
            if (_getPiece(dest + 16) == movePiece) vec.push_back(dest + 16);
        }

        if (!cap.isEmpty() || (cap.type == PAWNSTD && enpassant == dest)) {
            if (col) {
                if (_getPiece(dest + 7) == movePiece) vec.push_back(dest + 7);
            }
            if (col != 7) {
                if (_getPiece(dest + 9) == movePiece) vec.push_back(dest + 9);
            }
        }
    } else {
        auto p = _getPiece(dest - 8);
        if (p == movePiece) vec.push_back(dest - 8);
        else
        if (p.isEmpty() && dest >= 24 && dest < 32 && dest - 8 != from) {
            if (_getPiece(dest - 16) == movePiece) vec.push_back(dest - 16);
        }

        if (!cap.isEmpty() || (cap.type == PAWNSTD && enpassant == dest)) {
            if (col) {
                if (_getPiece(dest - 9) == movePiece) vec.push_back(dest - 9);
            }
            if (col != 7) {
                if (_getPiece(dest - 7) == movePiece) vec.push_back(dest - 7);
            }
        }

    }

    return vec;
}

std::vector<int> ChessBoard::_attackByKnight(int pos, const Piece& movePiece)
{
    std::vector<int> vec;
    int row = getRank(pos), col = getColumn(pos);
    auto attackerSide = movePiece.side;

    /* Check attacking of Knight */
    if (col > 0 && row > 1 && _isPiece(pos - 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos - 17);

    if (col < 7 && row > 1 && _isPiece(pos - 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos - 15);
    if (col > 1 && row > 0 && _isPiece(pos - 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos - 10);
    if (col < 6 && row > 0 && _isPiece(pos - 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos - 6);
    if (col > 1 && row < 7 && _isPiece(pos + 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos + 6);
    if (col < 6 && row < 7 && _isPiece(pos + 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos + 10);
    if (col > 0 && row < 6 && _isPiece(pos + 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos + 15);
    if (col < 7 && row < 6 && _isPiece(pos + 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        vec.push_back(pos + 17);

    return vec;
}

bool ChessBoard::createSanStringForLastMove()
{
    if (histList.empty()) {
        return false;
    }

    auto hist = &histList.back();

    auto movePiece = hist->move.piece;
    if (movePiece.isEmpty()) {
        return false; // something wrong
    }

    std::string str;

    // special cases - castling moves
    if (movePiece.type == KING && hist->castled) {
        assert(hist->castled == CastleRight_long || hist->castled == CastleRight_short);
        str = hist->castled == CastleRight_long  ? "O-O-O" : "O-O";
    } else {
        auto ambi = false, sameCol = false, sameRow = false;

        std::vector<int> avec;
        auto t = static_cast<PieceTypeStd>(movePiece.type);
        switch (t) {
        case PieceTypeStd::rook:
        {
            avec = _attackByRook(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeStd::bishop:
        {
            avec = _attackByBishop(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeStd::queen:
        {
            avec = _attackByQueen(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeStd::knight:
        {
            avec = _attackByKnight(hist->move.dest, movePiece);
            break;
        }
        case PieceTypeStd::pawn:
        {
            avec = _attackByPawn(hist->move.from, hist->move.dest, movePiece, hist->cap, hist->enpassant);
            break;
        }
        default:
            break;

        }

        if (!avec.empty()) {
            ambi = true;

            for(auto && pos : avec) {
                if (pos / 8 == hist->move.from / 8) {
                    sameRow = true;
                }
                if (pos % 8 == hist->move.from % 8) {
                    sameCol = true;
                }
            }
        }

        if (movePiece.type != static_cast<int>(PieceTypeStd::pawn)) {
            str = char(pieceType2Char(movePiece.type) - 'a' + 'A');
        }
        if (ambi) {
            if (sameCol && sameRow) {
                str += posToCoordinateString(hist->move.from);
            } else if (sameCol) {
                str += std::to_string(8 - hist->move.from / 8);
            } else {
                str += char('a' + hist->move.from % 8);
            }
        }

        if (!hist->cap.isEmpty()) {
            /// When a pawn makes a capture, the file from which the pawn departed is used to
            /// identify the pawn. For example, exd5
            if (str.empty() && movePiece.type == static_cast<int>(PieceTypeStd::pawn)) {
                str += char('a' + hist->move.from % 8);
            }
            str += "x";
        }

        str += posToCoordinateString(hist->move.dest);

        // promotion
        if (hist->move.promotion != EMPTY) {
            str += "=";
            str += char(pieceType2Char(hist->move.promotion) - 'a' + 'A');
        }

    }

    // incheck
    if (_isIncheck(side)) {

        assert(_isHashKeyValid());

        std::vector<MoveFull> moveList;
        _genLegalOnly(moveList, side);
        str += moveList.empty() ? "#" : "+";
    }

//    if (hist->sanString != str) {
//        printOut("board");
//        std::cout << "str: " << str << ", hist->sanString: " << hist->sanString << std::endl;
//        createSanStringForLastMove();
//    }
    hist->sanString = str;
    return true;
}


std::string ChessBoard::chessPiece2String(const Piece& piece, bool alwayLowerCase)
{
    char ch = Funcs::chessPieceType2Char(piece.type);
    if (!alwayLowerCase && piece.side == Side::white) {
        ch += 'A' - 'a';
    }
    return std::string(1, ch);
}

std::string ChessBoard::piece2String(const Piece& piece, bool alwayLowerCase)
{
    return chessPiece2String(piece, alwayLowerCase);
}

char ChessBoard::pieceType2Char(int pieceType) const
{
    return Funcs::chessPieceType2Char(pieceType);
}

std::string ChessBoard::chessPieceType2FullString(int pieceType)
{
    return pieceTypeFullNames[pieceType];
}

std::string ChessBoard::toString(const Piece& piece) const
{
    return chessPiece2String(piece, false);
}

std::string ChessBoard::moveString_coordinate(const Move& move)
{
    std::ostringstream stringStream;
    stringStream << Funcs::chessPosToCoordinateString(move.from) << Funcs::chessPosToCoordinateString(move.dest);
    if (move.promotion > KING && move.promotion < PAWNSTD) {
        stringStream << chessPiece2String(Piece(move.promotion, Side::white), true);
    }
    return stringStream.str();
}

std::string ChessBoard::toString(const Move& move) const
{
    return moveString_coordinate(move);
}

std::string ChessBoard::toString(const MoveFull& move) const
{
    return toString(Move(move));
}

std::string ChessBoard::hist2String(const HistBasic& hist, Notation notation)
{
    switch (notation) {
    case Notation::san:
        return hist.sanString;
    case Notation::lan:
        return toString_lan(hist.move, ChessVariant::standard);
    default:
        return moveString_coordinate(Move(hist.move));
    }
}

bool ChessBoard::createStringForLastMove(const std::vector<MoveFull>& moveList)
{
    if (histList.empty()) {
        return false;
    }
    
    assert(_isHashKeyValid());

    auto hist = &histList.back();
    
    auto movePiece = hist->move.piece;
    if (movePiece.isEmpty()) {
        return false; // something wrong
    }
    
    std::string str;

    // special cases - castling moves
    if (movePiece.type == KING && hist->castled) {
        assert(hist->castled == CastleRight_long || hist->castled == CastleRight_short);
        str = hist->castled == CastleRight_long  ? "O-O-O" : "O-O";
    } else {
        auto ambi = false, sameCol = false, sameRow = false;

        if (movePiece.type != static_cast<int>(PieceTypeStd::king)) {
            for(auto && m : moveList) {
                if (m.dest == hist->move.dest
                    && m.from != hist->move.from
                    && pieces.at(m.from).type == movePiece.type) {
                    ambi = true;
                    if (m.from / 8 == hist->move.from / 8) {
                        sameRow = true;
                    }
                    if (m.from % 8 == hist->move.from % 8) {
                        sameCol = true;
                    }
                }
            }
        }

        if (movePiece.type != static_cast<int>(PieceTypeStd::pawn)) {
            str = char(pieceType2Char(movePiece.type) - 'a' + 'A');
        }
        if (ambi) {
            if (sameCol && sameRow) {
                str += posToCoordinateString(hist->move.from);
            } else if (sameCol) {
                str += std::to_string(8 - hist->move.from / 8);
            } else {
                str += char('a' + hist->move.from % 8);
            }
        }

        if (!hist->cap.isEmpty()) {
            /// When a pawn makes a capture, the file from which the pawn departed is used to
            /// identify the pawn. For example, exd5
            if (str.empty() && movePiece.type == static_cast<int>(PieceTypeStd::pawn)) {
                str += char('a' + hist->move.from % 8);
            }
            str += "x";
        }

        str += posToCoordinateString(hist->move.dest);

        // promotion
        if (hist->move.promotion != EMPTY) {
            str += "=";
            str += char(pieceType2Char(hist->move.promotion) - 'a' + 'A');
        }

    }
    
    
    // incheck
    if (_isIncheck(side)) {
        assert(_isHashKeyValid());

        std::vector<MoveFull> moveList;
        _genLegalOnly(moveList, side);
        str += moveList.empty() ? "#" : "+";
    }
    
    hist->sanString = str;
    return true;
}


int ChessBoard::charactorToPieceType(char ch) const
{
    return Funcs::chessCharactorToPieceType(ch);
}

Move ChessBoard::moveFromString_castling(const std::string& str, Side side) const
{
    assert(str == "O-O" || str == "O-O+" || str == "0-0" || str == "O-O-O" || str == "O-O-O+" || str == "0-0-0");

    auto from = side == Side::black ? 4 : 60;
    auto dest = from + (str.length() < 5 ? 2 : -2);
    return Move(from, dest, EMPTY);
}

Move ChessBoard::moveFromString_san(const std::string& str)
{
    if (str.length() < 2) {
        return Move::illegalMove;
    }

    if (str == "O-O" || str == "O-O+" || str == "0-0" || str == "O-O-O" || str == "O-O-O+" || str == "0-0-0") {
        return moveFromString_castling(str, side);
    }

    std::string s;
    for(auto ch : str) {
        if (ch != '+' && ch != 'x' && ch != '*' && ch != '#' && ch != '-' && ch != '!' && ch != '?') {
            if (ch < ' ' || ch == '.') ch = ' ';
            s += ch;
        }
    }
    
    int from = -1, dest = -1, fromCol = -1, fromRow = -1;
    auto pieceType = static_cast<int>(PieceTypeStd::pawn), promotion = EMPTY;
    
    auto p = s.find("=");
    if (p != std::string::npos) {
        if (s.length() == p + 1) {
            return Move::illegalMove; // something wrong
        }
        char ch = s.at(p + 1);
        promotion = charactorToPieceType(ch);
        if (!Move::isValidPromotion(promotion)) {
            return Move::illegalMove;;
        }

        s = s.substr(0, p);
        if (s.size() < 2 || promotion == EMPTY) {
            return Move::illegalMove;;
        }
    }

    if (s.length() < 2) {
        return Move::illegalMove;
    }

    auto destString = s.substr(s.length() - 2, 2);
    dest = coordinateStringToPos(destString.c_str());

    if (!isPositionValid(dest)) {
        return Move::illegalMove;;
    }

    if (s.length() > 2) {
        auto k = 0;
        char ch = s.at(0);
        if (ch >= 'A' && ch <= 'Z') {
            k++;
            pieceType = charactorToPieceType(ch);

            if (pieceType == EMPTY) {
                return Move::illegalMove;;
            }
        }

        auto left = s.length() - k - 2;
        if (left > 0) {
            s = s.substr(k, left);
            if (left == 2) {
                from = coordinateStringToPos(s.c_str());
            } else {
                char ch = s.at(0);
                if (isdigit(ch)) {
                    fromRow = 8 - ch + '0';
                } else if (ch >= 'a' && ch <= 'z') {
                    fromCol = ch - 'a';
                }
            }
        }
    }

    if (from < 0) {
        std::vector<MoveFull> moveList;
        _gen(moveList, side);

        std::vector<Move> goodMoves;
        for (auto && m : moveList) {
            if (m.dest != dest || m.promotion != promotion ||
                _getPiece(m.from).type != pieceType) {
                continue;
            }

            if ((fromRow < 0 && fromCol < 0) ||
                (fromRow >= 0 && getRank(m.from) == fromRow) ||
                (fromCol >= 0 && getColumn(m.from) == fromCol)) {
                goodMoves.push_back(m);
                from = m.from;
            }
        }

        if (goodMoves.size() > 1) {
            for(auto && m : goodMoves) {
                MoveFull move = createFullMove(m.from, dest, promotion);
                Hist hist;
                _make(move, hist);
                auto incheck = _isIncheck(side);
                _takeBack(hist);
                if (!incheck) {
                    from = m.from;
                    break;
                }
            }
        }
    }
    
    assert(Move::isValidPromotion(promotion));
    return Move(from, dest, promotion);
}

std::vector<Move> ChessBoard::coordinateString2Moves(const std::string& str)
{
    std::vector<Move> moves;
    auto ss = Funcs::replaceString(str, ".", " ");
    auto list = Funcs::splitString(ss, ' ');
    for(auto && s : list) {
        auto ch = s.at(0);
        if (isdigit(ch)) continue;
        
        Move m = ChessBoard::chessMoveFromCoordiateString(s);
        moves.push_back(m);
    }
    
    return moves;
}

std::vector<Move> ChessBoard::getTransitionList(Hist hist) const
{
    auto vec = BoardCore::getTransitionList(hist);

    if (hist.castled) {
        assert(hist.move.piece.type == KING && std::abs(hist.move.from - hist.move.dest) == 2);
        int from = 0, dest = 3;
        if (hist.castled & CastleRight_short) {
            from = 7; dest = 5;
        }
        if (hist.move.from > 32) {
            from += 56; dest += 56;
        }

        vec.push_back(Move(from, dest));
    }

    return vec;
}

void ChessBoard::_clone(const BoardData* oboard)
{
    BoardData::_clone(oboard);
    assert(Funcs::isChessFamily(oboard->variant));
    auto ob = static_cast<const ChessBoard*>(oboard);
    enpassant = ob->enpassant;
    castleRights[0] = ob->castleRights[0];
    castleRights[1] = ob->castleRights[1];

    castleRights_column_king = ob->castleRights_column_king;
    castleRights_column_rook_left = ob->castleRights_column_rook_left;
    castleRights_column_rook_right = ob->castleRights_column_rook_right;
}

#define HAVE_COMMENT_BIT    (1 << 15)

int16_t ChessBoard::move2i16(int from, int dest, int promotion, bool haveComment) const
{
    int data = from | dest << 6 | promotion << 12;
    if (haveComment) data |= HAVE_COMMENT_BIT;
    return data;
}

void ChessBoard::i16ToMove(int data, int& from, int& dest, int& promotion, bool& haveComment) const
{
    from = data & 0x3f;
    dest = data >> 6 & 0x3f;
    promotion = (data >> 12) & 0x7;
    haveComment = (data & HAVE_COMMENT_BIT) != 0;
}

int ChessBoard::toPieceCount(int* pieceCnt) const
{
    if (pieceCnt) {
        memset(pieceCnt, 0, 14 * sizeof(int));
    }
    auto totalCnt = 0;;
    for(int i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) continue;
        totalCnt++;
        auto sd = static_cast<int>(piece.side), type = static_cast<int>(piece.type);
        if (pieceCnt) {
            pieceCnt[sd * 7 + type]++;
        }
    }

    assert(totalCnt >= 2 && totalCnt <= 32);
    return totalCnt;
}

void ChessBoard::createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const
{
    moveList.push_back(m);
}


int ChessBoard::getRawMaterialScore() const
{
    static const int rawMaterials[] = { 0, 0, 9, 5, 3, 3, 1 };
    auto score = 0;
    for(int i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) continue;
        auto s = rawMaterials[static_cast<int>(piece.type)];
        score += piece.side == side ? s : -s;
    }
    return score;
}


bool ChessBoard::isValueSmaller(int type0, int type1) const
{
    /// empty, king, queen, rook, bishop, knight, pawn
    static int pieceCompareValues[] = {
        0, 10000, 100, 50, 20, 20, 1, 1
    };

    return pieceCompareValues[type0] < pieceCompareValues[type1];
}

uint64_t ChessBoard::xorHashKey(int pos) const
{
    assert(isPositionValid(pos));
    assert(!pieces[pos].isEmpty());
    
    auto sd = static_cast<int>(pieces[pos].side);
    auto kind_of_piece = (6 - static_cast<int>(pieces[pos].type)) * 2 + sd; assert(kind_of_piece >= 0 && kind_of_piece <= 11);
    
    auto file = getColumn(pos), row = 7 - getRank(pos);
    auto offset_piece = 64 * kind_of_piece + 8 * row + file;
    
    return RandomPiece[offset_piece];
}

uint64_t ChessBoard::hashKeyEnpassant(int enpassant) const
{
    uint64_t key = 0;
    
    if (enpassant > 0 && enpassant < 64) {
        
        auto col = getColumn(enpassant), row = getRank(enpassant);
        auto ok = false;
        if (row == 2) {
            ok = (col > 0 && _isPiece(enpassant + 7, static_cast<int>(PieceTypeStd::pawn), Side::white))
            || (col < 7 && _isPiece(enpassant + 9, static_cast<int>(PieceTypeStd::pawn), Side::white));
        } else {
            ok = (col > 0 && _isPiece(enpassant - 9, static_cast<int>(PieceTypeStd::pawn), Side::black))
            || (col < 7 && _isPiece(enpassant - 7, static_cast<int>(PieceTypeStd::pawn), Side::black));
        }
        if (ok) key ^= RandomEnPassant[col];
    }
    
    return key;
}

uint64_t ChessBoard::xorSideHashKey() const
{
    return *RandomTurn;
}

uint64_t ChessBoard::initHashKey() const
{
    uint64_t key = 0;
    for(int i = 0, n = static_cast<int>(pieces.size()); i < n; i++) {
        if (!pieces[i].isEmpty()) {
            key ^= xorHashKey(i);
        }
    }
    
    if (side == Side::white) {
        key ^= *RandomTurn;
    }
    
    if (castleRights[0] | castleRights[1]) {
        /// white can castle short     0
        /// white can castle long      1
        /// black can castle short     2
        /// black can castle long      3
        
        if (castleRights[W] & CastleRight_short) {
            key ^= RandomCastle[0];
        }
        if (castleRights[W] & CastleRight_long) {
            key ^= RandomCastle[1];
        }
        if (castleRights[B] & CastleRight_short) {
            key ^= RandomCastle[2];
        }
        if (castleRights[B] & CastleRight_long) {
            key ^= RandomCastle[3];
        }
    }

    key ^= hashKeyEnpassant(enpassant);
    return key;
}

// http://hardy.uhasselt.be/Toga/book_format.html

//uint64_t polyglotRandom64[781] = {
uint64_t polyglotRandom64[800] = {
    uint64_t(0x9D39247E33776D41), uint64_t(0x2AF7398005AAA5C7), uint64_t(0x44DB015024623547), uint64_t(0x9C15F73E62A76AE2),
    uint64_t(0x75834465489C0C89), uint64_t(0x3290AC3A203001BF), uint64_t(0x0FBBAD1F61042279), uint64_t(0xE83A908FF2FB60CA),
    uint64_t(0x0D7E765D58755C10), uint64_t(0x1A083822CEAFE02D), uint64_t(0x9605D5F0E25EC3B0), uint64_t(0xD021FF5CD13A2ED5),
    uint64_t(0x40BDF15D4A672E32), uint64_t(0x011355146FD56395), uint64_t(0x5DB4832046F3D9E5), uint64_t(0x239F8B2D7FF719CC),
    uint64_t(0x05D1A1AE85B49AA1), uint64_t(0x679F848F6E8FC971), uint64_t(0x7449BBFF801FED0B), uint64_t(0x7D11CDB1C3B7ADF0),
    uint64_t(0x82C7709E781EB7CC), uint64_t(0xF3218F1C9510786C), uint64_t(0x331478F3AF51BBE6), uint64_t(0x4BB38DE5E7219443),
    uint64_t(0xAA649C6EBCFD50FC), uint64_t(0x8DBD98A352AFD40B), uint64_t(0x87D2074B81D79217), uint64_t(0x19F3C751D3E92AE1),
    uint64_t(0xB4AB30F062B19ABF), uint64_t(0x7B0500AC42047AC4), uint64_t(0xC9452CA81A09D85D), uint64_t(0x24AA6C514DA27500),
    uint64_t(0x4C9F34427501B447), uint64_t(0x14A68FD73C910841), uint64_t(0xA71B9B83461CBD93), uint64_t(0x03488B95B0F1850F),
    uint64_t(0x637B2B34FF93C040), uint64_t(0x09D1BC9A3DD90A94), uint64_t(0x3575668334A1DD3B), uint64_t(0x735E2B97A4C45A23),
    uint64_t(0x18727070F1BD400B), uint64_t(0x1FCBACD259BF02E7), uint64_t(0xD310A7C2CE9B6555), uint64_t(0xBF983FE0FE5D8244),
    uint64_t(0x9F74D14F7454A824), uint64_t(0x51EBDC4AB9BA3035), uint64_t(0x5C82C505DB9AB0FA), uint64_t(0xFCF7FE8A3430B241),
    uint64_t(0x3253A729B9BA3DDE), uint64_t(0x8C74C368081B3075), uint64_t(0xB9BC6C87167C33E7), uint64_t(0x7EF48F2B83024E20),
    uint64_t(0x11D505D4C351BD7F), uint64_t(0x6568FCA92C76A243), uint64_t(0x4DE0B0F40F32A7B8), uint64_t(0x96D693460CC37E5D),
    uint64_t(0x42E240CB63689F2F), uint64_t(0x6D2BDCDAE2919661), uint64_t(0x42880B0236E4D951), uint64_t(0x5F0F4A5898171BB6),
    uint64_t(0x39F890F579F92F88), uint64_t(0x93C5B5F47356388B), uint64_t(0x63DC359D8D231B78), uint64_t(0xEC16CA8AEA98AD76),
    uint64_t(0x5355F900C2A82DC7), uint64_t(0x07FB9F855A997142), uint64_t(0x5093417AA8A7ED5E), uint64_t(0x7BCBC38DA25A7F3C),
    uint64_t(0x19FC8A768CF4B6D4), uint64_t(0x637A7780DECFC0D9), uint64_t(0x8249A47AEE0E41F7), uint64_t(0x79AD695501E7D1E8),
    uint64_t(0x14ACBAF4777D5776), uint64_t(0xF145B6BECCDEA195), uint64_t(0xDABF2AC8201752FC), uint64_t(0x24C3C94DF9C8D3F6),
    uint64_t(0xBB6E2924F03912EA), uint64_t(0x0CE26C0B95C980D9), uint64_t(0xA49CD132BFBF7CC4), uint64_t(0xE99D662AF4243939),
    uint64_t(0x27E6AD7891165C3F), uint64_t(0x8535F040B9744FF1), uint64_t(0x54B3F4FA5F40D873), uint64_t(0x72B12C32127FED2B),
    uint64_t(0xEE954D3C7B411F47), uint64_t(0x9A85AC909A24EAA1), uint64_t(0x70AC4CD9F04F21F5), uint64_t(0xF9B89D3E99A075C2),
    uint64_t(0x87B3E2B2B5C907B1), uint64_t(0xA366E5B8C54F48B8), uint64_t(0xAE4A9346CC3F7CF2), uint64_t(0x1920C04D47267BBD),
    uint64_t(0x87BF02C6B49E2AE9), uint64_t(0x092237AC237F3859), uint64_t(0xFF07F64EF8ED14D0), uint64_t(0x8DE8DCA9F03CC54E),
    uint64_t(0x9C1633264DB49C89), uint64_t(0xB3F22C3D0B0B38ED), uint64_t(0x390E5FB44D01144B), uint64_t(0x5BFEA5B4712768E9),
    uint64_t(0x1E1032911FA78984), uint64_t(0x9A74ACB964E78CB3), uint64_t(0x4F80F7A035DAFB04), uint64_t(0x6304D09A0B3738C4),
    uint64_t(0x2171E64683023A08), uint64_t(0x5B9B63EB9CEFF80C), uint64_t(0x506AACF489889342), uint64_t(0x1881AFC9A3A701D6),
    uint64_t(0x6503080440750644), uint64_t(0xDFD395339CDBF4A7), uint64_t(0xEF927DBCF00C20F2), uint64_t(0x7B32F7D1E03680EC),
    uint64_t(0xB9FD7620E7316243), uint64_t(0x05A7E8A57DB91B77), uint64_t(0xB5889C6E15630A75), uint64_t(0x4A750A09CE9573F7),
    uint64_t(0xCF464CEC899A2F8A), uint64_t(0xF538639CE705B824), uint64_t(0x3C79A0FF5580EF7F), uint64_t(0xEDE6C87F8477609D),
    uint64_t(0x799E81F05BC93F31), uint64_t(0x86536B8CF3428A8C), uint64_t(0x97D7374C60087B73), uint64_t(0xA246637CFF328532),
    uint64_t(0x043FCAE60CC0EBA0), uint64_t(0x920E449535DD359E), uint64_t(0x70EB093B15B290CC), uint64_t(0x73A1921916591CBD),
    uint64_t(0x56436C9FE1A1AA8D), uint64_t(0xEFAC4B70633B8F81), uint64_t(0xBB215798D45DF7AF), uint64_t(0x45F20042F24F1768),
    uint64_t(0x930F80F4E8EB7462), uint64_t(0xFF6712FFCFD75EA1), uint64_t(0xAE623FD67468AA70), uint64_t(0xDD2C5BC84BC8D8FC),
    uint64_t(0x7EED120D54CF2DD9), uint64_t(0x22FE545401165F1C), uint64_t(0xC91800E98FB99929), uint64_t(0x808BD68E6AC10365),
    uint64_t(0xDEC468145B7605F6), uint64_t(0x1BEDE3A3AEF53302), uint64_t(0x43539603D6C55602), uint64_t(0xAA969B5C691CCB7A),
    uint64_t(0xA87832D392EFEE56), uint64_t(0x65942C7B3C7E11AE), uint64_t(0xDED2D633CAD004F6), uint64_t(0x21F08570F420E565),
    uint64_t(0xB415938D7DA94E3C), uint64_t(0x91B859E59ECB6350), uint64_t(0x10CFF333E0ED804A), uint64_t(0x28AED140BE0BB7DD),
    uint64_t(0xC5CC1D89724FA456), uint64_t(0x5648F680F11A2741), uint64_t(0x2D255069F0B7DAB3), uint64_t(0x9BC5A38EF729ABD4),
    uint64_t(0xEF2F054308F6A2BC), uint64_t(0xAF2042F5CC5C2858), uint64_t(0x480412BAB7F5BE2A), uint64_t(0xAEF3AF4A563DFE43),
    uint64_t(0x19AFE59AE451497F), uint64_t(0x52593803DFF1E840), uint64_t(0xF4F076E65F2CE6F0), uint64_t(0x11379625747D5AF3),
    uint64_t(0xBCE5D2248682C115), uint64_t(0x9DA4243DE836994F), uint64_t(0x066F70B33FE09017), uint64_t(0x4DC4DE189B671A1C),
    uint64_t(0x51039AB7712457C3), uint64_t(0xC07A3F80C31FB4B4), uint64_t(0xB46EE9C5E64A6E7C), uint64_t(0xB3819A42ABE61C87),
    uint64_t(0x21A007933A522A20), uint64_t(0x2DF16F761598AA4F), uint64_t(0x763C4A1371B368FD), uint64_t(0xF793C46702E086A0),
    uint64_t(0xD7288E012AEB8D31), uint64_t(0xDE336A2A4BC1C44B), uint64_t(0x0BF692B38D079F23), uint64_t(0x2C604A7A177326B3),
    uint64_t(0x4850E73E03EB6064), uint64_t(0xCFC447F1E53C8E1B), uint64_t(0xB05CA3F564268D99), uint64_t(0x9AE182C8BC9474E8),
    uint64_t(0xA4FC4BD4FC5558CA), uint64_t(0xE755178D58FC4E76), uint64_t(0x69B97DB1A4C03DFE), uint64_t(0xF9B5B7C4ACC67C96),
    uint64_t(0xFC6A82D64B8655FB), uint64_t(0x9C684CB6C4D24417), uint64_t(0x8EC97D2917456ED0), uint64_t(0x6703DF9D2924E97E),
    uint64_t(0xC547F57E42A7444E), uint64_t(0x78E37644E7CAD29E), uint64_t(0xFE9A44E9362F05FA), uint64_t(0x08BD35CC38336615),
    uint64_t(0x9315E5EB3A129ACE), uint64_t(0x94061B871E04DF75), uint64_t(0xDF1D9F9D784BA010), uint64_t(0x3BBA57B68871B59D),
    uint64_t(0xD2B7ADEEDED1F73F), uint64_t(0xF7A255D83BC373F8), uint64_t(0xD7F4F2448C0CEB81), uint64_t(0xD95BE88CD210FFA7),
    uint64_t(0x336F52F8FF4728E7), uint64_t(0xA74049DAC312AC71), uint64_t(0xA2F61BB6E437FDB5), uint64_t(0x4F2A5CB07F6A35B3),
    uint64_t(0x87D380BDA5BF7859), uint64_t(0x16B9F7E06C453A21), uint64_t(0x7BA2484C8A0FD54E), uint64_t(0xF3A678CAD9A2E38C),
    uint64_t(0x39B0BF7DDE437BA2), uint64_t(0xFCAF55C1BF8A4424), uint64_t(0x18FCF680573FA594), uint64_t(0x4C0563B89F495AC3),
    uint64_t(0x40E087931A00930D), uint64_t(0x8CFFA9412EB642C1), uint64_t(0x68CA39053261169F), uint64_t(0x7A1EE967D27579E2),
    uint64_t(0x9D1D60E5076F5B6F), uint64_t(0x3810E399B6F65BA2), uint64_t(0x32095B6D4AB5F9B1), uint64_t(0x35CAB62109DD038A),
    uint64_t(0xA90B24499FCFAFB1), uint64_t(0x77A225A07CC2C6BD), uint64_t(0x513E5E634C70E331), uint64_t(0x4361C0CA3F692F12),
    uint64_t(0xD941ACA44B20A45B), uint64_t(0x528F7C8602C5807B), uint64_t(0x52AB92BEB9613989), uint64_t(0x9D1DFA2EFC557F73),
    uint64_t(0x722FF175F572C348), uint64_t(0x1D1260A51107FE97), uint64_t(0x7A249A57EC0C9BA2), uint64_t(0x04208FE9E8F7F2D6),
    uint64_t(0x5A110C6058B920A0), uint64_t(0x0CD9A497658A5698), uint64_t(0x56FD23C8F9715A4C), uint64_t(0x284C847B9D887AAE),
    uint64_t(0x04FEABFBBDB619CB), uint64_t(0x742E1E651C60BA83), uint64_t(0x9A9632E65904AD3C), uint64_t(0x881B82A13B51B9E2),
    uint64_t(0x506E6744CD974924), uint64_t(0xB0183DB56FFC6A79), uint64_t(0x0ED9B915C66ED37E), uint64_t(0x5E11E86D5873D484),
    uint64_t(0xF678647E3519AC6E), uint64_t(0x1B85D488D0F20CC5), uint64_t(0xDAB9FE6525D89021), uint64_t(0x0D151D86ADB73615),
    uint64_t(0xA865A54EDCC0F019), uint64_t(0x93C42566AEF98FFB), uint64_t(0x99E7AFEABE000731), uint64_t(0x48CBFF086DDF285A),
    uint64_t(0x7F9B6AF1EBF78BAF), uint64_t(0x58627E1A149BBA21), uint64_t(0x2CD16E2ABD791E33), uint64_t(0xD363EFF5F0977996),
    uint64_t(0x0CE2A38C344A6EED), uint64_t(0x1A804AADB9CFA741), uint64_t(0x907F30421D78C5DE), uint64_t(0x501F65EDB3034D07),
    uint64_t(0x37624AE5A48FA6E9), uint64_t(0x957BAF61700CFF4E), uint64_t(0x3A6C27934E31188A), uint64_t(0xD49503536ABCA345),
    uint64_t(0x088E049589C432E0), uint64_t(0xF943AEE7FEBF21B8), uint64_t(0x6C3B8E3E336139D3), uint64_t(0x364F6FFA464EE52E),
    uint64_t(0xD60F6DCEDC314222), uint64_t(0x56963B0DCA418FC0), uint64_t(0x16F50EDF91E513AF), uint64_t(0xEF1955914B609F93),
    uint64_t(0x565601C0364E3228), uint64_t(0xECB53939887E8175), uint64_t(0xBAC7A9A18531294B), uint64_t(0xB344C470397BBA52),
    uint64_t(0x65D34954DAF3CEBD), uint64_t(0xB4B81B3FA97511E2), uint64_t(0xB422061193D6F6A7), uint64_t(0x071582401C38434D),
    uint64_t(0x7A13F18BBEDC4FF5), uint64_t(0xBC4097B116C524D2), uint64_t(0x59B97885E2F2EA28), uint64_t(0x99170A5DC3115544),
    uint64_t(0x6F423357E7C6A9F9), uint64_t(0x325928EE6E6F8794), uint64_t(0xD0E4366228B03343), uint64_t(0x565C31F7DE89EA27),
    uint64_t(0x30F5611484119414), uint64_t(0xD873DB391292ED4F), uint64_t(0x7BD94E1D8E17DEBC), uint64_t(0xC7D9F16864A76E94),
    uint64_t(0x947AE053EE56E63C), uint64_t(0xC8C93882F9475F5F), uint64_t(0x3A9BF55BA91F81CA), uint64_t(0xD9A11FBB3D9808E4),
    uint64_t(0x0FD22063EDC29FCA), uint64_t(0xB3F256D8ACA0B0B9), uint64_t(0xB03031A8B4516E84), uint64_t(0x35DD37D5871448AF),
    uint64_t(0xE9F6082B05542E4E), uint64_t(0xEBFAFA33D7254B59), uint64_t(0x9255ABB50D532280), uint64_t(0xB9AB4CE57F2D34F3),
    uint64_t(0x693501D628297551), uint64_t(0xC62C58F97DD949BF), uint64_t(0xCD454F8F19C5126A), uint64_t(0xBBE83F4ECC2BDECB),
    uint64_t(0xDC842B7E2819E230), uint64_t(0xBA89142E007503B8), uint64_t(0xA3BC941D0A5061CB), uint64_t(0xE9F6760E32CD8021),
    uint64_t(0x09C7E552BC76492F), uint64_t(0x852F54934DA55CC9), uint64_t(0x8107FCCF064FCF56), uint64_t(0x098954D51FFF6580),
    uint64_t(0x23B70EDB1955C4BF), uint64_t(0xC330DE426430F69D), uint64_t(0x4715ED43E8A45C0A), uint64_t(0xA8D7E4DAB780A08D),
    uint64_t(0x0572B974F03CE0BB), uint64_t(0xB57D2E985E1419C7), uint64_t(0xE8D9ECBE2CF3D73F), uint64_t(0x2FE4B17170E59750),
    uint64_t(0x11317BA87905E790), uint64_t(0x7FBF21EC8A1F45EC), uint64_t(0x1725CABFCB045B00), uint64_t(0x964E915CD5E2B207),
    uint64_t(0x3E2B8BCBF016D66D), uint64_t(0xBE7444E39328A0AC), uint64_t(0xF85B2B4FBCDE44B7), uint64_t(0x49353FEA39BA63B1),
    uint64_t(0x1DD01AAFCD53486A), uint64_t(0x1FCA8A92FD719F85), uint64_t(0xFC7C95D827357AFA), uint64_t(0x18A6A990C8B35EBD),
    uint64_t(0xCCCB7005C6B9C28D), uint64_t(0x3BDBB92C43B17F26), uint64_t(0xAA70B5B4F89695A2), uint64_t(0xE94C39A54A98307F),
    uint64_t(0xB7A0B174CFF6F36E), uint64_t(0xD4DBA84729AF48AD), uint64_t(0x2E18BC1AD9704A68), uint64_t(0x2DE0966DAF2F8B1C),
    uint64_t(0xB9C11D5B1E43A07E), uint64_t(0x64972D68DEE33360), uint64_t(0x94628D38D0C20584), uint64_t(0xDBC0D2B6AB90A559),
    uint64_t(0xD2733C4335C6A72F), uint64_t(0x7E75D99D94A70F4D), uint64_t(0x6CED1983376FA72B), uint64_t(0x97FCAACBF030BC24),
    uint64_t(0x7B77497B32503B12), uint64_t(0x8547EDDFB81CCB94), uint64_t(0x79999CDFF70902CB), uint64_t(0xCFFE1939438E9B24),
    uint64_t(0x829626E3892D95D7), uint64_t(0x92FAE24291F2B3F1), uint64_t(0x63E22C147B9C3403), uint64_t(0xC678B6D860284A1C),
    uint64_t(0x5873888850659AE7), uint64_t(0x0981DCD296A8736D), uint64_t(0x9F65789A6509A440), uint64_t(0x9FF38FED72E9052F),
    uint64_t(0xE479EE5B9930578C), uint64_t(0xE7F28ECD2D49EECD), uint64_t(0x56C074A581EA17FE), uint64_t(0x5544F7D774B14AEF),
    uint64_t(0x7B3F0195FC6F290F), uint64_t(0x12153635B2C0CF57), uint64_t(0x7F5126DBBA5E0CA7), uint64_t(0x7A76956C3EAFB413),
    uint64_t(0x3D5774A11D31AB39), uint64_t(0x8A1B083821F40CB4), uint64_t(0x7B4A38E32537DF62), uint64_t(0x950113646D1D6E03),
    uint64_t(0x4DA8979A0041E8A9), uint64_t(0x3BC36E078F7515D7), uint64_t(0x5D0A12F27AD310D1), uint64_t(0x7F9D1A2E1EBE1327),
    uint64_t(0xDA3A361B1C5157B1), uint64_t(0xDCDD7D20903D0C25), uint64_t(0x36833336D068F707), uint64_t(0xCE68341F79893389),
    uint64_t(0xAB9090168DD05F34), uint64_t(0x43954B3252DC25E5), uint64_t(0xB438C2B67F98E5E9), uint64_t(0x10DCD78E3851A492),
    uint64_t(0xDBC27AB5447822BF), uint64_t(0x9B3CDB65F82CA382), uint64_t(0xB67B7896167B4C84), uint64_t(0xBFCED1B0048EAC50),
    uint64_t(0xA9119B60369FFEBD), uint64_t(0x1FFF7AC80904BF45), uint64_t(0xAC12FB171817EEE7), uint64_t(0xAF08DA9177DDA93D),
    uint64_t(0x1B0CAB936E65C744), uint64_t(0xB559EB1D04E5E932), uint64_t(0xC37B45B3F8D6F2BA), uint64_t(0xC3A9DC228CAAC9E9),
    uint64_t(0xF3B8B6675A6507FF), uint64_t(0x9FC477DE4ED681DA), uint64_t(0x67378D8ECCEF96CB), uint64_t(0x6DD856D94D259236),
    uint64_t(0xA319CE15B0B4DB31), uint64_t(0x073973751F12DD5E), uint64_t(0x8A8E849EB32781A5), uint64_t(0xE1925C71285279F5),
    uint64_t(0x74C04BF1790C0EFE), uint64_t(0x4DDA48153C94938A), uint64_t(0x9D266D6A1CC0542C), uint64_t(0x7440FB816508C4FE),
    uint64_t(0x13328503DF48229F), uint64_t(0xD6BF7BAEE43CAC40), uint64_t(0x4838D65F6EF6748F), uint64_t(0x1E152328F3318DEA),
    uint64_t(0x8F8419A348F296BF), uint64_t(0x72C8834A5957B511), uint64_t(0xD7A023A73260B45C), uint64_t(0x94EBC8ABCFB56DAE),
    uint64_t(0x9FC10D0F989993E0), uint64_t(0xDE68A2355B93CAE6), uint64_t(0xA44CFE79AE538BBE), uint64_t(0x9D1D84FCCE371425),
    uint64_t(0x51D2B1AB2DDFB636), uint64_t(0x2FD7E4B9E72CD38C), uint64_t(0x65CA5B96B7552210), uint64_t(0xDD69A0D8AB3B546D),
    uint64_t(0x604D51B25FBF70E2), uint64_t(0x73AA8A564FB7AC9E), uint64_t(0x1A8C1E992B941148), uint64_t(0xAAC40A2703D9BEA0),
    uint64_t(0x764DBEAE7FA4F3A6), uint64_t(0x1E99B96E70A9BE8B), uint64_t(0x2C5E9DEB57EF4743), uint64_t(0x3A938FEE32D29981),
    uint64_t(0x26E6DB8FFDF5ADFE), uint64_t(0x469356C504EC9F9D), uint64_t(0xC8763C5B08D1908C), uint64_t(0x3F6C6AF859D80055),
    uint64_t(0x7F7CC39420A3A545), uint64_t(0x9BFB227EBDF4C5CE), uint64_t(0x89039D79D6FC5C5C), uint64_t(0x8FE88B57305E2AB6),
    uint64_t(0xA09E8C8C35AB96DE), uint64_t(0xFA7E393983325753), uint64_t(0xD6B6D0ECC617C699), uint64_t(0xDFEA21EA9E7557E3),
    uint64_t(0xB67C1FA481680AF8), uint64_t(0xCA1E3785A9E724E5), uint64_t(0x1CFC8BED0D681639), uint64_t(0xD18D8549D140CAEA),
    uint64_t(0x4ED0FE7E9DC91335), uint64_t(0xE4DBF0634473F5D2), uint64_t(0x1761F93A44D5AEFE), uint64_t(0x53898E4C3910DA55),
    uint64_t(0x734DE8181F6EC39A), uint64_t(0x2680B122BAA28D97), uint64_t(0x298AF231C85BAFAB), uint64_t(0x7983EED3740847D5),
    uint64_t(0x66C1A2A1A60CD889), uint64_t(0x9E17E49642A3E4C1), uint64_t(0xEDB454E7BADC0805), uint64_t(0x50B704CAB602C329),
    uint64_t(0x4CC317FB9CDDD023), uint64_t(0x66B4835D9EAFEA22), uint64_t(0x219B97E26FFC81BD), uint64_t(0x261E4E4C0A333A9D),
    uint64_t(0x1FE2CCA76517DB90), uint64_t(0xD7504DFA8816EDBB), uint64_t(0xB9571FA04DC089C8), uint64_t(0x1DDC0325259B27DE),
    uint64_t(0xCF3F4688801EB9AA), uint64_t(0xF4F5D05C10CAB243), uint64_t(0x38B6525C21A42B0E), uint64_t(0x36F60E2BA4FA6800),
    uint64_t(0xEB3593803173E0CE), uint64_t(0x9C4CD6257C5A3603), uint64_t(0xAF0C317D32ADAA8A), uint64_t(0x258E5A80C7204C4B),
    uint64_t(0x8B889D624D44885D), uint64_t(0xF4D14597E660F855), uint64_t(0xD4347F66EC8941C3), uint64_t(0xE699ED85B0DFB40D),
    uint64_t(0x2472F6207C2D0484), uint64_t(0xC2A1E7B5B459AEB5), uint64_t(0xAB4F6451CC1D45EC), uint64_t(0x63767572AE3D6174),
    uint64_t(0xA59E0BD101731A28), uint64_t(0x116D0016CB948F09), uint64_t(0x2CF9C8CA052F6E9F), uint64_t(0x0B090A7560A968E3),
    uint64_t(0xABEEDDB2DDE06FF1), uint64_t(0x58EFC10B06A2068D), uint64_t(0xC6E57A78FBD986E0), uint64_t(0x2EAB8CA63CE802D7),
    uint64_t(0x14A195640116F336), uint64_t(0x7C0828DD624EC390), uint64_t(0xD74BBE77E6116AC7), uint64_t(0x804456AF10F5FB53),
    uint64_t(0xEBE9EA2ADF4321C7), uint64_t(0x03219A39EE587A30), uint64_t(0x49787FEF17AF9924), uint64_t(0xA1E9300CD8520548),
    uint64_t(0x5B45E522E4B1B4EF), uint64_t(0xB49C3B3995091A36), uint64_t(0xD4490AD526F14431), uint64_t(0x12A8F216AF9418C2),
    uint64_t(0x001F837CC7350524), uint64_t(0x1877B51E57A764D5), uint64_t(0xA2853B80F17F58EE), uint64_t(0x993E1DE72D36D310),
    uint64_t(0xB3598080CE64A656), uint64_t(0x252F59CF0D9F04BB), uint64_t(0xD23C8E176D113600), uint64_t(0x1BDA0492E7E4586E),
    uint64_t(0x21E0BD5026C619BF), uint64_t(0x3B097ADAF088F94E), uint64_t(0x8D14DEDB30BE846E), uint64_t(0xF95CFFA23AF5F6F4),
    uint64_t(0x3871700761B3F743), uint64_t(0xCA672B91E9E4FA16), uint64_t(0x64C8E531BFF53B55), uint64_t(0x241260ED4AD1E87D),
    uint64_t(0x106C09B972D2E822), uint64_t(0x7FBA195410E5CA30), uint64_t(0x7884D9BC6CB569D8), uint64_t(0x0647DFEDCD894A29),
    uint64_t(0x63573FF03E224774), uint64_t(0x4FC8E9560F91B123), uint64_t(0x1DB956E450275779), uint64_t(0xB8D91274B9E9D4FB),
    uint64_t(0xA2EBEE47E2FBFCE1), uint64_t(0xD9F1F30CCD97FB09), uint64_t(0xEFED53D75FD64E6B), uint64_t(0x2E6D02C36017F67F),
    uint64_t(0xA9AA4D20DB084E9B), uint64_t(0xB64BE8D8B25396C1), uint64_t(0x70CB6AF7C2D5BCF0), uint64_t(0x98F076A4F7A2322E),
    uint64_t(0xBF84470805E69B5F), uint64_t(0x94C3251F06F90CF3), uint64_t(0x3E003E616A6591E9), uint64_t(0xB925A6CD0421AFF3),
    uint64_t(0x61BDD1307C66E300), uint64_t(0xBF8D5108E27E0D48), uint64_t(0x240AB57A8B888B20), uint64_t(0xFC87614BAF287E07),
    uint64_t(0xEF02CDD06FFDB432), uint64_t(0xA1082C0466DF6C0A), uint64_t(0x8215E577001332C8), uint64_t(0xD39BB9C3A48DB6CF),
    uint64_t(0x2738259634305C14), uint64_t(0x61CF4F94C97DF93D), uint64_t(0x1B6BACA2AE4E125B), uint64_t(0x758F450C88572E0B),
    uint64_t(0x959F587D507A8359), uint64_t(0xB063E962E045F54D), uint64_t(0x60E8ED72C0DFF5D1), uint64_t(0x7B64978555326F9F),
    uint64_t(0xFD080D236DA814BA), uint64_t(0x8C90FD9B083F4558), uint64_t(0x106F72FE81E2C590), uint64_t(0x7976033A39F7D952),
    uint64_t(0xA4EC0132764CA04B), uint64_t(0x733EA705FAE4FA77), uint64_t(0xB4D8F77BC3E56167), uint64_t(0x9E21F4F903B33FD9),
    uint64_t(0x9D765E419FB69F6D), uint64_t(0xD30C088BA61EA5EF), uint64_t(0x5D94337FBFAF7F5B), uint64_t(0x1A4E4822EB4D7A59),
    uint64_t(0x6FFE73E81B637FB3), uint64_t(0xDDF957BC36D8B9CA), uint64_t(0x64D0E29EEA8838B3), uint64_t(0x08DD9BDFD96B9F63),
    uint64_t(0x087E79E5A57D1D13), uint64_t(0xE328E230E3E2B3FB), uint64_t(0x1C2559E30F0946BE), uint64_t(0x720BF5F26F4D2EAA),
    uint64_t(0xB0774D261CC609DB), uint64_t(0x443F64EC5A371195), uint64_t(0x4112CF68649A260E), uint64_t(0xD813F2FAB7F5C5CA),
    uint64_t(0x660D3257380841EE), uint64_t(0x59AC2C7873F910A3), uint64_t(0xE846963877671A17), uint64_t(0x93B633ABFA3469F8),
    uint64_t(0xC0C0F5A60EF4CDCF), uint64_t(0xCAF21ECD4377B28C), uint64_t(0x57277707199B8175), uint64_t(0x506C11B9D90E8B1D),
    uint64_t(0xD83CC2687A19255F), uint64_t(0x4A29C6465A314CD1), uint64_t(0xED2DF21216235097), uint64_t(0xB5635C95FF7296E2),
    uint64_t(0x22AF003AB672E811), uint64_t(0x52E762596BF68235), uint64_t(0x9AEBA33AC6ECC6B0), uint64_t(0x944F6DE09134DFB6),
    uint64_t(0x6C47BEC883A7DE39), uint64_t(0x6AD047C430A12104), uint64_t(0xA5B1CFDBA0AB4067), uint64_t(0x7C45D833AFF07862),
    uint64_t(0x5092EF950A16DA0B), uint64_t(0x9338E69C052B8E7B), uint64_t(0x455A4B4CFE30E3F5), uint64_t(0x6B02E63195AD0CF8),
    uint64_t(0x6B17B224BAD6BF27), uint64_t(0xD1E0CCD25BB9C169), uint64_t(0xDE0C89A556B9AE70), uint64_t(0x50065E535A213CF6),
    uint64_t(0x9C1169FA2777B874), uint64_t(0x78EDEFD694AF1EED), uint64_t(0x6DC93D9526A50E68), uint64_t(0xEE97F453F06791ED),
    uint64_t(0x32AB0EDB696703D3), uint64_t(0x3A6853C7E70757A7), uint64_t(0x31865CED6120F37D), uint64_t(0x67FEF95D92607890),
    uint64_t(0x1F2B1D1F15F6DC9C), uint64_t(0xB69E38A8965C6B65), uint64_t(0xAA9119FF184CCCF4), uint64_t(0xF43C732873F24C13),
    uint64_t(0xFB4A3D794A9A80D2), uint64_t(0x3550C2321FD6109C), uint64_t(0x371F77E76BB8417E), uint64_t(0x6BFA9AAE5EC05779),
    uint64_t(0xCD04F3FF001A4778), uint64_t(0xE3273522064480CA), uint64_t(0x9F91508BFFCFC14A), uint64_t(0x049A7F41061A9E60),
    uint64_t(0xFCB6BE43A9F2FE9B), uint64_t(0x08DE8A1C7797DA9B), uint64_t(0x8F9887E6078735A1), uint64_t(0xB5B4071DBFC73A66),
    uint64_t(0x230E343DFBA08D33), uint64_t(0x43ED7F5A0FAE657D), uint64_t(0x3A88A0FBBCB05C63), uint64_t(0x21874B8B4D2DBC4F),
    uint64_t(0x1BDEA12E35F6A8C9), uint64_t(0x53C065C6C8E63528), uint64_t(0xE34A1D250E7A8D6B), uint64_t(0xD6B04D3B7651DD7E),
    uint64_t(0x5E90277E7CB39E2D), uint64_t(0x2C046F22062DC67D), uint64_t(0xB10BB459132D0A26), uint64_t(0x3FA9DDFB67E2F199),
    uint64_t(0x0E09B88E1914F7AF), uint64_t(0x10E8B35AF3EEAB37), uint64_t(0x9EEDECA8E272B933), uint64_t(0xD4C718BC4AE8AE5F),
    uint64_t(0x81536D601170FC20), uint64_t(0x91B534F885818A06), uint64_t(0xEC8177F83F900978), uint64_t(0x190E714FADA5156E),
    uint64_t(0xB592BF39B0364963), uint64_t(0x89C350C893AE7DC1), uint64_t(0xAC042E70F8B383F2), uint64_t(0xB49B52E587A1EE60),
    uint64_t(0xFB152FE3FF26DA89), uint64_t(0x3E666E6F69AE2C15), uint64_t(0x3B544EBE544C19F9), uint64_t(0xE805A1E290CF2456),
    uint64_t(0x24B33C9D7ED25117), uint64_t(0xE74733427B72F0C1), uint64_t(0x0A804D18B7097475), uint64_t(0x57E3306D881EDB4F),
    uint64_t(0x4AE7D6A36EB5DBCB), uint64_t(0x2D8D5432157064C8), uint64_t(0xD1E649DE1E7F268B), uint64_t(0x8A328A1CEDFE552C),
    uint64_t(0x07A3AEC79624C7DA), uint64_t(0x84547DDC3E203C94), uint64_t(0x990A98FD5071D263), uint64_t(0x1A4FF12616EEFC89),
    uint64_t(0xF6F7FD1431714200), uint64_t(0x30C05B1BA332F41C), uint64_t(0x8D2636B81555A786), uint64_t(0x46C9FEB55D120902),
    uint64_t(0xCCEC0A73B49C9921), uint64_t(0x4E9D2827355FC492), uint64_t(0x19EBB029435DCB0F), uint64_t(0x4659D2B743848A2C),
    uint64_t(0x963EF2C96B33BE31), uint64_t(0x74F85198B05A2E7D), uint64_t(0x5A0F544DD2B1FB18), uint64_t(0x03727073C2E134B1),
    uint64_t(0xC7F6AA2DE59AEA61), uint64_t(0x352787BAA0D7C22F), uint64_t(0x9853EAB63B5E0B35), uint64_t(0xABBDCDD7ED5C0860),
    uint64_t(0xCF05DAF5AC8D77B0), uint64_t(0x49CAD48CEBF4A71E), uint64_t(0x7A4C10EC2158C4A6), uint64_t(0xD9E92AA246BF719E),
    uint64_t(0x13AE978D09FE5557), uint64_t(0x730499AF921549FF), uint64_t(0x4E4B705B92903BA4), uint64_t(0xFF577222C14F0A3A),
    uint64_t(0x55B6344CF97AAFAE), uint64_t(0xB862225B055B6960), uint64_t(0xCAC09AFBDDD2CDB4), uint64_t(0xDAF8E9829FE96B5F),
    uint64_t(0xB5FDFC5D3132C498), uint64_t(0x310CB380DB6F7503), uint64_t(0xE87FBB46217A360E), uint64_t(0x2102AE466EBB1148),
    uint64_t(0xF8549E1A3AA5E00D), uint64_t(0x07A69AFDCC42261A), uint64_t(0xC4C118BFE78FEAAE), uint64_t(0xF9F4892ED96BD438),
    uint64_t(0x1AF3DBE25D8F45DA), uint64_t(0xF5B4B0B0D2DEEEB4), uint64_t(0x962ACEEFA82E1C84), uint64_t(0x046E3ECAAF453CE9),
    uint64_t(0xF05D129681949A4C), uint64_t(0x964781CE734B3C84), uint64_t(0x9C2ED44081CE5FBD), uint64_t(0x522E23F3925E319E),
    uint64_t(0x177E00F9FC32F791), uint64_t(0x2BC60A63A6F3B3F2), uint64_t(0x222BBFAE61725606), uint64_t(0x486289DDCC3D6780),
    uint64_t(0x7DC7785B8EFDFC80), uint64_t(0x8AF38731C02BA980), uint64_t(0x1FAB64EA29A2DDF7), uint64_t(0xE4D9429322CD065A),
    uint64_t(0x9DA058C67844F20C), uint64_t(0x24C0E332B70019B0), uint64_t(0x233003B5A6CFE6AD), uint64_t(0xD586BD01C5C217F6),
    uint64_t(0x5E5637885F29BC2B), uint64_t(0x7EBA726D8C94094B), uint64_t(0x0A56A5F0BFE39272), uint64_t(0xD79476A84EE20D06),
    uint64_t(0x9E4C1269BAA4BF37), uint64_t(0x17EFEE45B0DEE640), uint64_t(0x1D95B0A5FCF90BC6), uint64_t(0x93CBE0B699C2585D),
    uint64_t(0x65FA4F227A2B6D79), uint64_t(0xD5F9E858292504D5), uint64_t(0xC2B5A03F71471A6F), uint64_t(0x59300222B4561E00),
    uint64_t(0xCE2F8642CA0712DC), uint64_t(0x7CA9723FBB2E8988), uint64_t(0x2785338347F2BA08), uint64_t(0xC61BB3A141E50E8C),
    uint64_t(0x150F361DAB9DEC26), uint64_t(0x9F6A419D382595F4), uint64_t(0x64A53DC924FE7AC9), uint64_t(0x142DE49FFF7A7C3D),
    uint64_t(0x0C335248857FA9E7), uint64_t(0x0A9C32D5EAE45305), uint64_t(0xE6C42178C4BBB92E), uint64_t(0x71F1CE2490D20B07),
    uint64_t(0xF1BCC3D275AFE51A), uint64_t(0xE728E8C83C334074), uint64_t(0x96FBF83A12884624), uint64_t(0x81A1549FD6573DA5),
    uint64_t(0x5FA7867CAF35E149), uint64_t(0x56986E2EF3ED091B), uint64_t(0x917F1DD5F8886C61), uint64_t(0xD20D8C88C8FFE65F),
    uint64_t(0x31D71DCE64B2C310), uint64_t(0xF165B587DF898190), uint64_t(0xA57E6339DD2CF3A0), uint64_t(0x1EF6E6DBB1961EC9),
    uint64_t(0x70CC73D90BC26E24), uint64_t(0xE21A6B35DF0C3AD7), uint64_t(0x003A93D8B2806962), uint64_t(0x1C99DED33CB890A1),
    uint64_t(0xCF3145DE0ADD4289), uint64_t(0xD0E4427A5514FB72), uint64_t(0x77C621CC9FB3A483), uint64_t(0x67A34DAC4356550B),
    uint64_t(0xF8D626AAAF278509),
};


// embede game length and result
uint64_t ChessBoard::getHashKeyForCheckingDuplicates(int h) const
{
    auto hk = hashKey;

    for(size_t i = 0, n = histList.size(); i < n; i += 5) {
        hk ^= histList.at(i).hashKey;
    }
    return hk;
}


static const std::unordered_map<uint64_t, std::string> ecoMap
{
    {17746977930792137ULL, "D08;QGD;Albin counter-gambit"},{18828346148881476ULL, "D20;QGA;Linares variation"},{22186996187002557ULL, "D57;QGD;Lasker defence, Bernstein variation"},{34585945880640199ULL, "C55;two knights;Max Lange attack, Loman defence"},
    {50097418745942009ULL, "B35;Sicilian;accelerated fianchetto, modern variation with Bc4"},{75353901211836495ULL, "D44;QGD semi-Slav;5.Bg5 dc"},{81088336057533745ULL, "B72;Sicilian;dragon, classical, Amsterdam variation"},{92768586179066125ULL, "B15;Caro-Kann;Forgacs variation"},
    {93070569194681828ULL, "C85;Ruy Lopez;Exchange variation doubly deferred (DERLD)"},{108429426421640589ULL, "D26;QGA;classical variation"},{126429164633435350ULL, "E73;King's Indian;Semi-Averbakh system"},{129146609016689986ULL, "C17;French;Winawer, advance variation"},
    {159864834071171027ULL, "C44;Ponziani;Leonhardt variation"},{162638068193660275ULL, "A83;Dutch;Staunton gambit, Alekhine variation"},{179980765225139740ULL, "B00;Reversed Grob (Borg/Basman defence/macho Grob)"},{199895047165661787ULL, "C67;Ruy Lopez;open Berlin defence, 5...Be7"},
    {205915005492623802ULL, "A25;English;closed, 5.Rb1 Taimanov variation"},{232093606237214974ULL, "B58;Sicilian;classical"},{234967103162873998ULL, "C40;Latvian;corkscrew counter-gambit"},{241663449853490667ULL, "C49;Four knights;symmetrical, Blake variation"},
    {253434895135994229ULL, "A46;Queen's pawn game"},{257080320327475777ULL, "D26;QGA;4...e6"},{279026104961640647ULL, "C23;Bishop's opening;Lewis gambit"},{280250126274238020ULL, "A05;Reti;King's Indian attack"},
    {285876849302516213ULL, "E10;Blumenfeld counter-gambit, Dus-Chotimursky variation"},{298977628231912710ULL, "B00;Corn stalk defence"},{311127211448405766ULL, "A52;Budapest;Rubinstein variation"},{315624687282521176ULL, "A52;Budapest;Adler variation"},
    {319201036411485236ULL, "A52;Budapest;Alekhine, Abonyi variation"},{328650827399128808ULL, "A25;English;closed, Hort variation"},{341056394438190790ULL, "C64;Ruy Lopez;Cordel gambit"},{348315363220493419ULL, "C39;KGA;Kieseritsky, Brentano defence, Caro variation"},
    {350634523777882434ULL, "D51;QGD"},{380089877063268657ULL, "C75;Ruy Lopez;modern Steinitz defence"},{385148300217002622ULL, "C14;French;classical, Rubinstein variation"},{393152370340217610ULL, "D52;QGD;Cambridge Springs defence, Yugoslav variation"},
    {396290913584433096ULL, "D94;Gruenfeld;5.e3"},{399843181927194204ULL, "C68;Ruy Lopez;exchange, Keres variation"},{407516167164878681ULL, "C42;Petrov;classical attack, Marshall trap"},{414049029354388728ULL, "A22;English;Bremen, Smyslov system"},
    {418883936221838899ULL, "D80;Gruenfeld;Stockholm variation"},{430812066210874728ULL, "C70;Ruy Lopez;Classical defence deferred"},{431955103591521506ULL, "A86;Dutch;Leningrad variation"},{455376332548283648ULL, "D76;Neo-Gruenfeld, 6.cd Nxd5, 7.O-O Nb6"},
    {460664201775194104ULL, "D00;Queen's pawn game"},{465376829582353757ULL, "B73;Sicilian;dragon, classical, Zollner gambit"},{469334283514939506ULL, "E72;King's Indian;Pomar system"},{483181647023448010ULL, "C86;Ruy Lopez;Worrall attack, solid line"},
    {485829848405717316ULL, "C12;French;MacCutcheon, Tartakower variation"},{494565756031241763ULL, "C39;KGA;Kieseritsky, Berlin defence, Riviere variation"},{524487525825825669ULL, "E08;Catalan;closed, 7.Qc2"},{524871813099724732ULL, "B06;Robatsch defence;two knights variation"},
    {528813709611831216ULL, "B01;Scandinavian (centre counter) defence"},{536094197026543967ULL, "A00;Novosibirsk opening"},{540153139077332825ULL, "E60;King's Indian;Anti-Gruenfeld"},{547344062837359238ULL, "C33;KGA;bishop's gambit, classical defence, Cozio attack"},
    {551671236195985012ULL, "D31;QGD;3.Nc3"},{565658692004087728ULL, "C49;Four knights;Nimzovich (Paulsen) variation"},{588096204713577160ULL, "D20;QGA;Schwartz defence"},{595762792459712928ULL, "C20;King's pawn game"},
    {600066260902050104ULL, "C38;KGA;Philidor gambit"},{614493163470654996ULL, "C37;KGA;double Muzio gambit"},{621185887674936484ULL, "C77;Ruy Lopez;four knights (Tarrasch) variation"},{628624351417951702ULL, "D05;Queen's pawn game"},
    {631151290902954998ULL, "C73;Ruy Lopez;modern Steinitz defence, Richter variation"},{631859276947830251ULL, "C00;Lengfellner system"},{643333130994854636ULL, "A84;Dutch defence;Bladel variation"},{646620524679408395ULL, "C52;Evans gambit"},
    {662060445888131199ULL, "A04;Reti v Dutch"},{672579445205631238ULL, "A30;English;symmetrical, hedgehog system"},{674717977503001677ULL, "C00;French;Labourdonnais variation"},{675096894328159602ULL, "D06;QGD;Grau (Sahovic) defence"},
    {680310526679900176ULL, "E81;King's Indian;Saemisch, Byrne variation"},{681928074798639592ULL, "A40;Queen's pawn;Charlick (Englund) gambit"},{682228701176355193ULL, "B12;Caro-Kann;3.Nd2"},{689080851086102453ULL, "A47;Queen's Indian;Marienbad system, Berg variation"},
    {694158040231612290ULL, "B90;Sicilian;Najdorf"},{721395857107837663ULL, "E23;Nimzo-Indian;Spielmann, San Remo variation"},{724460668866907971ULL, "C43;Petrov;modern attack, Trifunovic variation"},{725524491860315360ULL, "C29;Vienna gambit;Heyde variation"},
    {751917504558729420ULL, "D23;QGA;Mannheim variation"},{762862098804734081ULL, "E33;Nimzo-Indian;classical, 4...Nc6"},{779854564108071530ULL, "B57;Sicilian;Magnus Smith trap"},{788855954930342825ULL, "D97;Gruenfeld;Russian, Byrne (Simagin) variation"},
    {795744134644371949ULL, "C07;French;Tarrasch, open variation"},{804760128311633266ULL, "C64;Ruy Lopez;classical defence, Boden variation"},{807339499089473653ULL, "C49;Four knights;Gunsberg counter-attack"},{814347751279441725ULL, "C44;Inverted Hungarian"},
    {818525519879500580ULL, "C06;French;Tarrasch, Leningrad variation"},{832199735984625890ULL, "D32;QGD;Tarrasch defence"},{842393318939455268ULL, "B01;Scandinavian, Mieses-Kotrvc gambit"},{846827105509111349ULL, "C65;Ruy Lopez;Berlin defence, Duras variation"},
    {866375219429131639ULL, "C41;Philidor;Nimzovich, Sokolsky variation"},{867479473878117778ULL, "D44;QGD semi-Slav;anti-Meran, Alatortsev system"},{868531555974334813ULL, "B40;Sicilian;Pin, Jaffe variation"},{871235757116333305ULL, "B03;Alekhine's defence"},
    {880046537159376374ULL, "B13;Caro-Kann;exchange, Rubinstein variation"},{888709716576523635ULL, "B84;Sicilian;Scheveningen (Paulsen), classical variation"},{895709612063626592ULL, "E60;Queen's pawn;Mengarini attack"},{912637919962264521ULL, "E36;Nimzo-Indian;classical, Noa variation, main line"},
    {931362614758719732ULL, "D32;QGD;Tarrasch defence, Tarrasch gambit"},{960596089312316969ULL, "C78;Ruy Lopez;Archangelsk (counterthrust) variation"},{977883100703674841ULL, "E45;Nimzo-Indian;4.e3, Bronstein (Byrne) variation"},{987724949187538097ULL, "B15;Caro-Kann;Rasa-Studier gambit"},
    {1002969794419342257ULL, "C33;KGA;bishop's gambit, classical defence"},{1010695331855392686ULL, "E29;Nimzo-Indian;Saemisch, main line"},{1015385342199689752ULL, "C44;Ponziani opening"},{1023252430469836185ULL, "A17;English;Nimzo-English opening"},
    {1023411585908033318ULL, "C13;French;Albin-Alekhine-Chatard attack, Teichmann variation"},{1031987402074780453ULL, "E00;Catalan opening"},{1040093037382535697ULL, "B81;Sicilian;Scheveningen, Keres attack"},{1055438252803405641ULL, "C25;Vienna;Hamppe-Muzio gambit"},
    {1060448323978272683ULL, "C54;Giuoco Piano;Moeller, bayonet attack"},{1067272237275351576ULL, "E15;Queen's Indian;4.g3 Bb7"},{1070270989520718321ULL, "B01;Scandinavian defence, Lasker variation"},{1131072876945541033ULL, "C50;Hungarian defence"},
    {1132226622186259064ULL, "C86;Ruy Lopez;Worrall attack"},{1136836702355326304ULL, "C38;KGA;Greco gambit"},{1136978638517649080ULL, "A00;Grob;spike attack"},{1142913488445854493ULL, "C00;French defence"},
    {1145901931914667389ULL, "D83;Gruenfeld;Gruenfeld gambit"},{1149175244901990878ULL, "A86;Dutch;Hort-Antoshin system"},{1160452292026599543ULL, "A25;English;Sicilian reversed"},{1166270510937167433ULL, "C66;Ruy Lopez;closed Berlin defence, Wolf variation"},
    {1170815197003505062ULL, "B26;Sicilian;closed, 6.Be3"},{1172755318016414253ULL, "B10;Caro-Kann defence"},{1177199845200899959ULL, "E86;King's Indian;Saemisch, orthodox, 7.Nge2 c6"},{1180666540095889894ULL, "E20;Nimzo-Indian defence"},
    {1181250935507069662ULL, "C30;KGD;classical, Heath variation"},{1193758262380112005ULL, "C13;French;Albin-Alekhine-Chatard attack, Maroczy variation"},{1210289857017219203ULL, "B00;Carr's defence"},{1224207606016225163ULL, "C65;Ruy Lopez;Berlin defence"},
    {1249912266087144751ULL, "A12;English;Caro-Kann defensive system"},{1258046245507792577ULL, "A82;Dutch;Staunton gambit"},{1261560438379621426ULL, "C66;Ruy Lopez;Berlin defence, Tarrasch trap"},{1264623597534322299ULL, "B48;Sicilian;Taimanov variation"},
    {1272977499960854521ULL, "C67;Ruy Lopez;Berlin defence, Trifunovic variation"},{1276707437135211721ULL, "E60;King's Indian;3.g3, counterthrust variation"},{1284147639525193715ULL, "A92;Dutch;stonewall variation"},{1304027934931214015ULL, "C44;Scotch gambit"},
    {1311565328533576976ULL, "A40;Queen's pawn;Keres defence"},{1312248645721223042ULL, "B01;Scandinavian;Kiel variation"},{1315429205381238931ULL, "A82;Dutch;Staunton gambit, Tartakower variation"},{1339735568848143725ULL, "B22;Sicilian;Alapin's variation (2.c3)"},
    {1349958639296937669ULL, "A59;Benko gambit;main line"},{1372393055999362299ULL, "B57;Sicilian;Sozin, not Scheveningen"},{1379015976990969684ULL, "C30;KGD;Mafia defence"},{1380558341248856209ULL, "C46;Four knights;Gunsberg variation"},
    {1399385826846608101ULL, "D31;QGD;Charousek (Petrosian) variation"},{1408323380557517765ULL, "A12;English;Capablanca's variation"},{1423382238700933920ULL, "C08;French;Tarrasch, open, 4.ed ed"},{1442869919908556634ULL, "C88;Ruy Lopez;Noah's ark trap"},
    {1465344066789954130ULL, "A80;Dutch, Spielmann gambit"},{1466304982533473175ULL, "B25;Sicilian;closed, 6.f4 e5 (Botvinnik)"},{1471912402293780975ULL, "C11;French defence"},{1496460521747091157ULL, "B30;Sicilian defence"},
    {1502867967586405366ULL, "C42;Petrov;classical attack, Maroczy variation"},{1507987825295848730ULL, "C37;KGA;Herzfeld gambit"},{1508548639351325909ULL, "D32;QGD;Tarrasch defence, Marshall gambit"},{1510405001424029068ULL, "C12;French;MacCutcheon, Grigoriev variation"},
    {1512040066571124506ULL, "D70;Neo-Gruenfeld (Kemeri) defence"},{1518847897104562604ULL, "C52;Evans gambit;compromised defence, Potter variation"},{1526583892611647346ULL, "E35;Nimzo-Indian;classical, Noa variation, 5.cd ed"},{1537939680406787428ULL, "D43;QGD semi-Slav"},
    {1538071123435312970ULL, "C44;Scotch;Relfsson gambit ('MacLopez')"},{1583003373283768694ULL, "A53;Old Indian;Janowski variation"},{1590610831583401658ULL, "C41;Philidor;Hanham, Schlechter variation"},{1591867025046500185ULL, "D10;QGD Slav defence;exchange variation"},
    {1592720841146794740ULL, "C50;Giuoco Piano;four knights variation"},{1604062147375403530ULL, "C57;Two knights defence"},{1607385025151380924ULL, "C39;KGA;Kieseritsky, Polerio defence"},{1608908532765781064ULL, "B00;Fred"},
    {1618184157921709863ULL, "B89;Sicilian;Sozin, 7.Be3"},{1627803748211667875ULL, "E28;Nimzo-Indian;Saemisch variation"},{1630878823951420481ULL, "E40;Nimzo-Indian;4.e3, Taimanov variation"},{1631053602592081292ULL, "A84;Dutch defence, Rubinstein variation"},
    {1636784041574637333ULL, "C52;Evans gambit;Sanders-Alapin variation"},{1657360759272219330ULL, "A13;English;Romanishin gambit"},{1658310324164644960ULL, "E53;Nimzo-Indian;4.e3, Gligoric system with 7...Nbd7"},{1672481660104987289ULL, "C46;Three knights;Winawer defence (Gothic defence)"},
    {1684873114507344896ULL, "A80;Dutch"},{1715879502989747150ULL, "C50;Giuoco Piano;Jerome gambit"},{1717091390203273011ULL, "C49;Four knights;Svenonius variation"},{1717325794186869341ULL, "D46;QGD semi-Slav;Romih variation"},
    {1725519647867546498ULL, "C41;Philidor;Nimzovich, Klein variation"},{1744398265243390855ULL, "A06;Reti opening"},{1746808887790790468ULL, "D10;QGD Slav defence"},{1785559229527161455ULL, "C50;Giuoco Pianissimo"},
    {1788265499167208713ULL, "B24;Sicilian;closed"},{1792042390923808664ULL, "D82;Gruenfeld;4.Bf4"},{1793586942414925842ULL, "E23;Nimzo-Indian;Spielmann, 4...c5, 5.dc Nc6"},{1796539845786600646ULL, "D90;Gruenfeld;Three knights variation"},
    {1805885792066369292ULL, "B10;Caro-Kann;anti-anti-Caro-Kann defence"},{1830617611963094072ULL, "B09;Pirc;Austrian attack, Ljubojevic variation"},{1850388987101744975ULL, "C39;KGA;Kieseritsky, long whip defence, Jaenisch variation"},{1872072547769390565ULL, "D53;QGD;Lasker variation"},
    {1910433956715525929ULL, "D30;QGD Slav"},{1919800305447462728ULL, "C35;KGA;Cunningham, three pawns gambit"},{1923358500586013480ULL, "E19;Queen's Indian;old main line, 9.Qxc3"},{1926837741170268791ULL, "C11;French;Burn variation"},
    {1935189363312352905ULL, "C44;Ponziani;Romanishin variation"},{1944688136365447025ULL, "C45;Scotch;Schmidt variation"},{1946315100406634607ULL, "A52;Budapest;Alekhine variation"},{1962737561213147529ULL, "E24;Nimzo-Indian;Saemisch, Botvinnik variation"},
    {1978899623154770437ULL, "C12;French;MacCutcheon variation"},{1981041941088873272ULL, "C50;King's pawn game"},{1988593045685185669ULL, "B07;Pirc;Sveshnikov system"},{1990893558917600402ULL, "C41;Philidor;Hanham, Steiner variation"},
    {1994394394814707257ULL, "C32;KGD;Falkbeer, Reti variation"},{2013878079646683206ULL, "C43;Petrov;modern attack, Steinitz variation"},{2020799347915760497ULL, "D15;QGD Slav;5.e3 (Alekhine variation)"},{2024041864677819526ULL, "A17;English opening"},
    {2030964369916079850ULL, "C42;Petrov;classical attack, Krause variation"},{2035424808263537117ULL, "A83;Dutch;Staunton gambit, Nimzovich variation"},{2039213063597150590ULL, "C41;Philidor;Hanham variation"},{2051294465054361358ULL, "E90;King's Indian;Zinnowitz variation"},
    {2052027514390166263ULL, "B03;Alekhine's defence;four pawns attack, Ilyin-Genevsky var."},{2059872162009066911ULL, "C51;Evans gambit declined"},{2059997614103988429ULL, "A54;Old Indian;Ukrainian variation"},{2068596486689581677ULL, "C11;French;Steinitz, Brodsky-Jones variation"},
    {2078644914092671997ULL, "C33;King's gambit accepted"},{2080795885257602142ULL, "C20;Alapin's opening"},{2097703482034446891ULL, "A04;Reti;Herrstroem gambit"},{2103673058385110530ULL, "B59;Sicilian;Boleslavsky variation, 7.Nb3"},
    {2109438605471607432ULL, "C41;Philidor;Lopez counter-gambit"},{2120154829327691706ULL, "D31;QGD;semi-Slav, Junge variation"},{2125347545205345665ULL, "C71;Ruy Lopez;modern Steinitz defence, Duras (Keres) variation"},{2126109872811153195ULL, "B43;Sicilian;Kan, 5.Nc3"},
    {2126161864446612371ULL, "C47;Four knights;Scotch variation"},{2162249090246653237ULL, "B06;Robatsch (modern) defence"},{2162955240635845655ULL, "C60;Ruy Lopez;Pollock defence"},{2163567240120417538ULL, "B42;Sicilian;Kan, Polugaievsky variation"},
    {2173170278717979762ULL, "C10;French;Rubinstein variation"},{2214322346111028096ULL, "A45;Trompovsky attack (Ruth, Opovcensky opening)"},{2223374119297668089ULL, "C44;Scotch gambit;Dubois-Reti defence"},{2223942618361702557ULL, "E13;Queen's Indian;4.Nc3, main line"},
    {2249760953699594720ULL, "E65;King's Indian;fianchetto, Yugoslav, 7.O-O"},{2253230049510824430ULL, "E70;King's Indian;Kramer system"},{2263610586285050206ULL, "C32;KGD;Falkbeer, Charousek gambit"},{2265040909915973734ULL, "C33;KGA;bishop's gambit, Bryan counter-gambit"},
    {2266851738050482146ULL, "C40;Damiano's defence"},{2288102930969254608ULL, "C29;Vienna gambit;Bardeleben variation"},{2289943406559540382ULL, "E92;King's Indian;Petrosian system, Stein variation"},{2295843770440425562ULL, "C39;KGA;Kieseritsky, long whip (Stockwhip, classical) defence"},
    {2296494139237993314ULL, "C42;Petrov;classical attack, Marshall variation"},{2331020027700877189ULL, "C40;Latvian;Nimzovich variation"},{2333359986265694190ULL, "C78;Ruy Lopez;Moeller defence"},{2347326972927828175ULL, "B23;Sicilian;chameleon variation"},
    {2355780979565715112ULL, "B00;KP;Neo-Mongoloid defence"},{2358031062763949778ULL, "E18;Queen's Indian;old main line, 7.Nc3"},{2365883673103208305ULL, "C21;Danish gambit"},{2371979716560691035ULL, "D26;QGA;classical, Steinitz variation"},
    {2381833518429266825ULL, "C33;KGA;bishop's gambit, McDonnell attack"},{2394272165581800200ULL, "B06;Robatsch defence;Pseudo-Austrian attack"},{2404418440598849819ULL, "D25;QGA, Flohr variation"},{2425993657540841980ULL, "D07;QGD;Chigorin defence"},
    {2431048915374324798ULL, "B01;Scandinavian;Marshall variation"},{2450883582250789737ULL, "C44;Inverted Hanham"},{2462118728632427888ULL, "C49;Four knights;double Ruy Lopez"},{2467160663667405245ULL, "E30;Nimzo-Indian;Leningrad, ...b5 gambit"},
    {2472801579185751424ULL, "A02;Bird;From gambit, Lasker variation"},{2491355942899484032ULL, "C41;Philidor;Nimzovich, Locock variation"},{2495019143410765144ULL, "D09;QGD;Albin counter-gambit, 5.g3"},{2498784733088028592ULL, "A22;English;Bellon gambit"},
    {2500870339942132509ULL, "E00;Queen's pawn game"},{2501790430741336322ULL, "A52;Budapest;Alekhine variation, Balogh gambit"},{2510128105812459938ULL, "C41;Philidor;Philidor counter-gambit, Berger variation"},{2545808394961734075ULL, "A64;Benoni;fianchetto, 11...Re8"},
    {2548189741011138670ULL, "D24;QGA, 4.Nc3"},{2551003664581051957ULL, "E05;Catalan;open, classical line"},{2553115037390445370ULL, "B20;Sicilian;wing gambit, Carlsbad variation"},{2556580720565404303ULL, "C51;Evans gambit"},
    {2570528886522758608ULL, "A10;English opening"},{2582099668255125075ULL, "E92;King's Indian;classical variation"},{2582274310287723088ULL, "D41;QGD;Semi-Tarrasch, Kmoch variation"},{2599071765067502500ULL, "E34;Nimzo-Indian;classical, Noa variation"},
    {2600605092184677156ULL, "C13;French;Albin-Alekhine-Chatard attack, Breyer variation"},{2615602418204527596ULL, "C37;KGA;King's knight's gambit"},{2631602280294716621ULL, "C33;KGA;bishop's gambit, Cozio (Morphy) defence"},{2638431053419136172ULL, "B96;Sicilian;Najdorf, Polugayevsky, Simagin variation"},
    {2647674393327811461ULL, "C87;Ruy Lopez;closed, Averbach variation"},{2649562838474808318ULL, "B80;Sicilian;Scheveningen, English variation"},{2659932886517243498ULL, "D51;QGD;Alekhine variation"},{2660229462653485338ULL, "C63;Ruy Lopez;Schliemann defence, Berger variation"},
    {2665195696870204389ULL, "C40;Latvian gambit, 3.Bc4"},{2667184447269435805ULL, "C44;Scotch;Sea-cadet mate"},{2678556546271027818ULL, "D49;QGD semi-Slav;Meran, Rellstab attack"},{2685445338569594810ULL, "B91;Sicilian;Najdorf, Zagreb (fianchetto) variation"},
    {2687458460693112465ULL, "A24;English;Bremen system with ...g6"},{2735638027298909633ULL, "A45;Canard opening"},{2763626317136184615ULL, "D94;Gruenfeld;Opovcensky variation"},{2812291553088293928ULL, "E72;King's Indian with e4 & g3"},
    {2815982706962157532ULL, "C99;Ruy Lopez;closed, Chigorin, 12...c5d4"},{2817757228276456790ULL, "B30;Sicilian;Nimzovich-Rossolimo attack (without ...d6)"},{2822413784242141877ULL, "A32;English;symmetrical variation"},{2826593791094353162ULL, "A58;Benko gambit;fianchetto variation"},
    {2830361496126759084ULL, "C30;KGD;classical, Svenonius variation"},{2832768202418866193ULL, "C56;two knights defence;Canal variation"},{2837305562891736562ULL, "D88;Gruenfeld;Spassky variation, main line, 10...cd, 11.cd"},{2841158655986296342ULL, "C57;two knights defence;Pincus variation"},
    {2863396486199434096ULL, "C50;Hungarian defence;Tartakower variation"},{2879667783145695991ULL, "A16;English;Anglo-Gruenfeld, Czech defense"},{2888833624220833499ULL, "D30;QGD;Capablanca-Duras variation"},{2897938674514273345ULL, "C27;Boden-Kieseritsky gambit;Lichtenhein defence"},
    {2902438695036763718ULL, "C17;French;Winawer, advance, Bogolyubov variation"},{2921270632758580205ULL, "A00;Gedult's opening"},{2946305577838015709ULL, "A40;Queen's pawn;Lundin (Kevitz-Mikenas) defence"},{2948852210967959229ULL, "B13;Caro-Kann;Panov-Botvinnik attack"},
    {2968645618076842591ULL, "A84;Dutch;Staunton gambit deferred"},{2979783405033386484ULL, "D53;QGD;4.Bg5 Be7, 5.e3 O-O"},{2981456423775869669ULL, "D30;QGD Slav"},{2990032682361723683ULL, "C11;French;Steinitz, Bradford attack"},
    {3010415883518821944ULL, "C76;Ruy Lopez;modern Steinitz defence, fianchetto (Bronstein) variation"},{3013264061438682261ULL, "B00;KP;Nimzovich defence"},{3026723190906502796ULL, "C33;KGA;bishop's gambit, Anderssen variation"},{3054009285210292312ULL, "A02;Bird's opening, Swiss gambit"},
    {3070102882874378496ULL, "E22;Nimzo-Indian;Spielmann variation"},{3073651427149315025ULL, "C44;Irish (Chicago) gambit"},{3108475269225842843ULL, "C23;Bishop's opening;Lopez gambit"},{3109732619945325308ULL, "E10;Dzindzikhashvili defence"},
    {3116052724643026085ULL, "C51;Evans gambit declined, Vasquez variation"},{3145523769726179243ULL, "C41;Philidor;Boden variation"},{3150446019692949450ULL, "C21;Centre game"},{3150683338262800183ULL, "C80;Ruy Lopez;open, Friess attack"},
    {3153365364445755383ULL, "D04;Queen's pawn game"},{3187673536852556384ULL, "D68;QGD;Orthodox defence, classical, 13.d1c2 (Vidmar)"},{3199321298215767711ULL, "B50;Sicilian;wing gambit deferred"},{3200327134666644710ULL, "A30;English;symmetrical variation"},
    {3200482480922671350ULL, "A48;King's Indian;Torre attack"},{3203274557100633268ULL, "D78;Neo-Gruenfeld, 6.O-O c6"},{3206567841353787247ULL, "E87;King's Indian;Saemisch, orthodox, 7.d5"},{3218234731427562774ULL, "C71;Ruy Lopez;Noah's ark trap"},
    {3229560124354561166ULL, "E62;King's Indian;fianchetto with ...Nc6"},{3237006084650278028ULL, "A01;Nimzovich-Larsen attack;English variation"},{3260638949772522974ULL, "C14;French;classical variation"},{3286571885173183468ULL, "C55;two knights;Max Lange attack, Marshall variation"},
    {3295663981079209527ULL, "C88;Ruy Lopez;closed"},{3309037463525757013ULL, "E41;Nimzo-Indian;e3, Huebner variation"},{3310964812804139346ULL, "A00;Ware (Meadow Hay) opening"},{3311456926882595607ULL, "C25;Vienna;Hamppe-Muzio, Dubois variation"},
    {3311637314013528902ULL, "A00;Amar (Paris) opening"},{3360384705350063804ULL, "C12;French;MacCutcheon, Dr. Olland (Dutch) variation"},{3369037455672973163ULL, "C29;Vienna gambit;Breyer variation"},{3381955357466325640ULL, "C67;Ruy Lopez;open Berlin defence, l'Hermet variation"},
    {3397419662239306889ULL, "B08;Pirc;classical system, 5.Be2"},{3400083129654161657ULL, "C00;French defence"},{3425771686832658330ULL, "C79;Ruy Lopez;Steinitz defence deferred, Lipnitsky variation"},{3436182914828844210ULL, "C41;Philidor's defence"},
    {3436509828836897644ULL, "C49;Four knights;Janowski variation"},{3443667478371923534ULL, "D74;Neo-Gruenfeld, 6.cd Nxd5, 7.O-O"},{3444443340009735810ULL, "A90;Dutch defence"},{3455050800116356188ULL, "C80;Ruy Lopez;open, Richter variation"},
    {3464972395278514486ULL, "A55;Old Indian;main line"},{3465823137605430928ULL, "C24;Bishop's opening;Berlin defence"},{3480779434815118776ULL, "D12;QGD Slav;exchange variation"},{3483784087682611621ULL, "C44;Ponziani;Reti variation"},
    {3499690135436263938ULL, "E11;Bogo-Indian defence"},{3513683231833345484ULL, "C65;Ruy Lopez;Berlin defence, Nyholm attack"},{3516124427892773502ULL, "C80;Ruy Lopez;open (Tarrasch) defence"},{3522217882952004378ULL, "A00;Saragossa opening"},
    {3530847393903079170ULL, "A56;Benoni defence, Hromodka system"},{3544017447312054389ULL, "B03;Alekhine's defence;four pawns attack, fianchetto variation"},{3549917949731431014ULL, "C52;Evans gambit"},{3554997480672540963ULL, "A02;Bird;From gambit"},
    {3557964634786027309ULL, "B12;Caro-Kann;advance variation"},{3559203162104236381ULL, "A48;King's Indian;London system"},{3566365796726406278ULL, "C00;French defence"},{3571118859891905707ULL, "D52;QGD;Cambridge Springs defence, Capablanca variation"},
    {3576683546092135311ULL, "C53;Giuoco Piano;Bird's attack"},{3596528991321544034ULL, "C55;two knights;Max Lange attack, Steinitz variation"},{3596843149529594596ULL, "B00;KP;Nimzovich defence, Marshall gambit"},{3597072943876122875ULL, "D41;QGD;Semi-Tarrasch, San Sebastian variation"},
    {3621453900593129592ULL, "E39;Nimzo-Indian;classical, Pirc variation"},{3628609804068937085ULL, "E70;King's Indian;4.e4"},{3653206553450064789ULL, "A56;Vulture defence"},{3666360726497518869ULL, "B70;Sicilian;dragon variation"},
    {3671236527783054716ULL, "D85;Gruenfeld;modern exchange variation"},{3687340422363297828ULL, "B13;Caro-Kann;Panov-Botvinnik, normal variation"},{3703543577092189457ULL, "D97;Gruenfeld;Russian, Szabo (Boleslavsky) variation"},{3721589565649642306ULL, "B72;Sicilian;dragon, classical, Grigoriev variation"},
    {3726715068746055811ULL, "B20;Sicilian;wing gambit, Santasiere variation"},{3734118374127251724ULL, "B64;Sicilian;Richter-Rauzer, Rauzer attack, 7...Be7 defence, 9.f4"},{3740165472307974932ULL, "C24;Bishop's opening;Urusov gambit, Panov variation"},{3751965621765903377ULL, "A07;Reti;King's Indian attack, Yugoslav variation"},
    {3762628565848649009ULL, "B23;Sicilian;closed, 2...Nc6"},{3783705115732780590ULL, "C43;Petrov;modern attack, Bardeleben variation"},{3802908241449248960ULL, "C25;Vienna;Zhuravlev countergambit"},{3806636502397750207ULL, "A07;Reti;King's Indian attack (Barcza system)"},
    {3807851409047975369ULL, "C32;KGD;Falkbeer, 5.de"},{3814813002764170537ULL, "C22;Centre game;Paulsen attack"},{3819772972804705162ULL, "C69;Ruy Lopez;exchange, Bronstein variation"},{3821741953746476915ULL, "B23;Sicilian;Grand Prix attack, Schofman variation"},
    {3832183348399703749ULL, "D95;Gruenfeld with e3 & Qb3"},{3837904794895176119ULL, "C70;Ruy Lopez;Graz variation"},{3841411053941445633ULL, "C02;French;advance, Paulsen attack"},{3844589628336227365ULL, "C23;Bishop's opening;Lewis counter-gambit"},
    {3849582592859879241ULL, "B01;Scandinavian;Anderssen counter-attack, Goteborg system"},{3849936855723689714ULL, "C48;Four knights;Rubinstein counter-gambit Maroczy variation"},{3862527020462264139ULL, "D11;QGD Slav;4.e3"},{3866024668317335372ULL, "C86;Ruy Lopez;Worrall attack, sharp line"},
    {3867468011839634872ULL, "D24;QGA, Bogolyubov variation"},{3876573693251775862ULL, "D94;Gruenfeld;Flohr defence"},{3881171046466963235ULL, "B03;Alekhine's defence;four pawns attack, Tartakower variation"},{3895288739435754094ULL, "C30;KGD;classical, Soldatenkov variation"},
    {3927413919025463946ULL, "D14;QGD Slav;exchange, Trifunovic variation"},{3927995000504146748ULL, "C41;Philidor;Nimzovich, Larobok variation"},{3933807376954504116ULL, "A41;Old Indian defence"},{3947480876875128000ULL, "B21;Sicilian;Smith-Morra gambit, Chicago defence"},
    {3966435633848507489ULL, "B64;Sicilian;Richter-Rauzer, Rauzer attack, Geller variation"},{3995230347371832435ULL, "C23;Bishop's opening;Lisitsyn variation"},{3998736422981301437ULL, "B07;Pirc defence"},{4011176468539861668ULL, "E04;Catalan;open, 5.Nf3"},
    {4017136971149588899ULL, "B01;Scandinavian;Anderssen counter-attack, Collijn variation"},{4022988733700015731ULL, "A00;Dunst (Sleipner, Heinrichsen) opening"},{4063555261012250655ULL, "E62;King's Indian;fianchetto variation"},{4070662662287390519ULL, "A04;Reti;Lisitsin gambit deferred"},
    {4077066735607626904ULL, "B24;Sicilian;closed, Smyslov variation"},{4131808793369033543ULL, "D35;QGD;exchange, positional line, 5...c6"},{4146739034373061775ULL, "A46;Doery defence"},{4154168095160724798ULL, "A58;Benko gambit accepted"},
    {4199217885640555335ULL, "D55;QGD;Neo-orthodox variation, 7.Bh4"},{4210317648896181505ULL, "C44;Ponziani;Fraser defence"},{4213706124815023803ULL, "B69;Sicilian;Richter-Rauzer, Rauzer attack, 7...a6 defence, 11.Bxf6"},{4215527647041457256ULL, "D45;QGD semi-Slav;Stoltz variation"},
    {4242222728099927581ULL, "C39;KGA;Kieseritsky, Berlin defence, 6.Bc4"},{4247389930724436162ULL, "C41;Philidor;Larsen variation"},{4251799473742031379ULL, "A91;Dutch defence"},{4252322073387629097ULL, "A15;English orang-utan"},
    {4252860505818453445ULL, "D28;QGA;classical, 7.Qe2"},{4256355402684739834ULL, "E03;Catalan;open, Alekhine variation"},{4258953488573273747ULL, "E83;King's Indian;Saemisch, Ruban variation"},{4263075511403063322ULL, "A00;Crab opening"},
    {4267285596553988160ULL, "C39;KGA;Kieseritsky, Kolisch defence"},{4268124199589483592ULL, "A85;Dutch with c4 & Nc3"},{4269883630390312678ULL, "D00;Queen's pawn, Mason variation, Steinitz counter-gambit"},{4282390844095610527ULL, "A02;Bird;Hobbs gambit"},
    {4283388330662294968ULL, "D02;Queen's pawn game, Krause variation"},{4289507578016638008ULL, "A81;Dutch defence"},{4290212893285750606ULL, "A00;Valencia opening"},{4305944704350272943ULL, "A00;Benko's opening;reversed Alekhine"},
    {4313968002181128084ULL, "B18;Caro-Kann;classical, 6.h4"},{4320784181224469278ULL, "C44;Ponziani counter-gambit, Cordel variation"},{4321508285821858838ULL, "C57;two knights defence;Fegatello attack, Polerio defence"},{4322622836849963117ULL, "C44;Scotch gambit;Cochrane-Shumov defence"},
    {4324021463537657648ULL, "B88;Sicilian;Sozin, Fischer variation"},{4324281355407905230ULL, "A18;English;Mikenas-Carls, Flohr variation"},{4328487689847855205ULL, "C51;Evans gambit declined, Cordel variation"},{4329989041891798515ULL, "C48;Four knights;Rubinstein counter-gambit"},
    {4336157998433906588ULL, "A81;Dutch defence"},{4337567615646812953ULL, "C39;KGA;Kieseritsky, Rice gambit"},{4346052675426232985ULL, "B37;Sicilian;accelerated fianchetto, Maroczy bind, 5...Bg7"},{4360657307224062430ULL, "E20;Nimzo-Indian;Romanishin-Kasparov (Steiner) system"},
    {4361514140196667959ULL, "A38;English;symmetrical variation"},{4361929241513247285ULL, "C70;Ruy Lopez;Schliemann defence deferred"},{4368719611914305575ULL, "B40;Sicilian;Pin, Koch variation"},{4382522623502217114ULL, "A21;English, Keres variation"},
    {4395465316795432678ULL, "D27;QGA;classical, Geller variation"},{4437826854576508447ULL, "C53;Giuoco Piano"},{4448812646389194872ULL, "A28;English;four knights, 4.e3"},{4482448187239699208ULL, "A28;English;four knights, Capablanca variation"},
    {4487776562310319923ULL, "A03;Bird's opening"},{4494605331110015729ULL, "A19;English;Mikenas-Carls, Sicilian variation"},{4497651300911151586ULL, "E02;Catalan;open, 5.Qa4"},{4515031458725109304ULL, "B12;Caro-Kann;Tartakower (fantasy) variation"},
    {4523965308953814394ULL, "E01;Catalan;closed"},{4526000532982363500ULL, "B16;Caro-Kann;Bronstein-Larsen variation"},{4532632041934401398ULL, "C36;KGA;Abbazia defence, modern variation"},{4581294444429833503ULL, "E14;Queen's Indian;4.e3"},
    {4587579720298564820ULL, "A50;Queen's Indian accelerated"},{4594899153392532465ULL, "D86;Gruenfeld;exchange, Simagin's improved variation"},{4621037608101072510ULL, "B23;Sicilian;closed, Korchnoi variation"},{4624267103951611333ULL, "C83;Ruy Lopez;open, Breslau variation"},
    {4636376774916772789ULL, "C41;Philidor;Philidor counter-gambit, Zukertort variation"},{4638709225839374253ULL, "A20;English opening"},{4652262204718639790ULL, "C55;Two knights defence"},{4662374139822761115ULL, "D03;Torre attack (Tartakower variation)"},
    {4670904698516336487ULL, "E69;King's Indian;fianchetto, classical main line"},{4675208541548386076ULL, "C05;French;Tarrasch, closed variation"},{4676423091427569607ULL, "A01;Nimzovich-Larsen attack;modern variation"},{4689055884960126695ULL, "C45;Scotch game"},
    {4694118728023989990ULL, "C33;KGA;bishop's gambit, Steinitz defence"},{4707275678600330656ULL, "C70;Ruy Lopez;fianchetto defence deferred"},{4711021577879597980ULL, "C11;French;Swiss variation"},{4714262423950890762ULL, "D31;QGD;semi-Slav, Koomen variation"},
    {4732887142486940272ULL, "B19;Caro-Kann;classical, Spassky variation"},{4738724342590890927ULL, "B84;Sicilian;Scheveningen, classical, Nd7 system"},{4746026874478777170ULL, "A00;Durkin's attack"},{4749393576208236187ULL, "B72;Sicilian;dragon, 6.Be3"},
    {4772180600060764635ULL, "B15;Caro-Kann;Gurgenidze system"},{4783470570693506787ULL, "C48;Four knights;Rubinstein counter-gambit, Henneberger variation"},{4816065901379497190ULL, "C36;KGA;Abbazia defence (classical defence, modern defence[!])"},{4826736649977810939ULL, "A16;English;Anglo-Gruenfeld, Smyslov defense"},
    {4832383203404758935ULL, "D45;QGD semi-Slav;stonewall defence"},{4839575596358482937ULL, "C50;Giuoco Pianissimo"},{4844196730419256037ULL, "C11;French;Steinitz variation"},{4853002718121989783ULL, "D95;Gruenfeld;Botvinnik variation"},
    {4889461856522516979ULL, "A34;English;symmetrical, three knights system"},{4894525041876144027ULL, "C32;KGD;Falkbeer, Tarrasch variation"},{4936617192026594496ULL, "C45;Scotch;Steinitz variation"},{4938006364797596735ULL, "C15;French;Winawer, Alekhine gambit, Alatortsev variation"},
    {4943260268173326737ULL, "B96;Sicilian;Najdorf, 7.f4"},{4971655603413990942ULL, "B07;Pirc defence"},{4972118107558260842ULL, "C41;Philidor;Berger variation"},{4976184481658280499ULL, "C45;Scotch;Gottschall variation"},
    {4983319750871719767ULL, "D47;QGD semi-Slav;7.Bc4"},{4986951600073511535ULL, "C44;Scotch gambit"},{5012966471566167240ULL, "C19;French;Winawer, advance, positional main line"},{5027806176587780159ULL, "C02;French;advance variation"},
    {5048815475934533219ULL, "A09;Reti accepted"},{5058574645595803197ULL, "B05;Alekhine's defence;modern, Vitolins attack"},{5069443153056472989ULL, "E12;Queen's Indian;4.Nc3, Botvinnik variation"},{5071476046405423122ULL, "D01;Richter-Veresov attack, Veresov variation"},
    {5074037839292285597ULL, "C42;Petrov;Paulsen attack"},{5077350706810526724ULL, "A00;Grob;Romford counter-gambit"},{5077560836872849361ULL, "C48;Four knights;Spanish, classical defence"},{5086539114019471251ULL, "C03;French;Tarrasch, Guimard variation"},
    {5098153148094690099ULL, "C22;Centre game;Kupreichik variation"},{5105251104846307979ULL, "D08;QGD;Albin counter-gambit, Krenosz variation"},{5107180496752749533ULL, "E61;King's Indian defence, 3.Nc3"},{5109897129714317523ULL, "C47;Four knights;Belgrade gambit"},
    {5118959200894703359ULL, "E38;Nimzo-Indian;classical, 4...c5"},{5122943116650574796ULL, "B33;Sicilian;Pelikan, Bird variation"},{5123515637629592430ULL, "A26;English;closed system"},{5130594583304821476ULL, "C55;two knights;Max Lange attack, Krause variation"},
    {5138353693252056387ULL, "C40;Latvian counter-gambit"},{5142681590128225157ULL, "C50;Giuoco Pianissimo;Canal variation"},{5147216496282773330ULL, "C42;Petrov;classical attack, Berger variation"},{5170449531384424670ULL, "B09;Pirc;Austrian attack, dragon formation"},
    {5173852007167802458ULL, "B36;Sicilian;accelerated fianchetto, Gurgenidze variation"},{5179676595142728407ULL, "E17;Queen's Indian;Opovcensky variation"},{5190929836226653021ULL, "C55;Two knights defence, Perreux variation"},{5228302592079845296ULL, "B52;Sicilian;Canal-Sokolsky attack, Bronstein gambit"},
    {5236544798189523667ULL, "B20;Sicilian;Steinitz variation"},{5239273496979456045ULL, "D37;QGD;classical variation (5.Bf4)"},{5251092285754341111ULL, "C40;Gunderam defence"},{5252282417176811558ULL, "B02;Alekhine's defence;two pawns' attack, Mikenas variation"},
    {5285967602387090444ULL, "C58;Two knights defence"},{5302342550187735804ULL, "A22;English;Bremen, reverse dragon"},{5311329172596058976ULL, "C54;Giuoco Piano;Cracow variation"},{5320287364485628302ULL, "C33;KGA;Schurig gambit"},
    {5321481444137079893ULL, "A65;Benoni;6.e4"},{5327093386548254680ULL, "E12;Queen's Indian;Miles variation"},{5341429674127318660ULL, "A14;English;Neo-Catalan declined"},{5368169678711972943ULL, "C45;Scotch;Horwitz attack"},
    {5373587376344037168ULL, "A15;English opening"},{5389245081934247656ULL, "C39;KGA;Allgaier, Blackburne gambit"},{5395219822743411674ULL, "B18;Caro-Kann;classical variation"},{5409798013178080797ULL, "C60;Ruy Lopez (Spanish opening)"},
    {5411160537556934944ULL, "A43;Old Benoni;Mujannah formation"},{5420989584607029409ULL, "C55;Giuoco piano"},{5422099271762560124ULL, "C31;KGD;Falkbeer, Nimzovich variation"},{5440644662471895769ULL, "A57;Benko gambit half accepted"},
    {5480348324255519222ULL, "D87;Gruenfeld;exchange, Seville variation"},{5493233342706585834ULL, "C69;Ruy Lopez;exchange, Gligoric variation"},{5498148765782426369ULL, "B34;Sicilian;accelerated fianchetto, exchange variation"},{5498927606932965815ULL, "C11;French;Steinitz variation"},
    {5501661765285859324ULL, "C52;Evans gambit;Sokolsky variation"},{5526166482892371277ULL, "B07;Pirc;Ufimtsev-Pytel variation"},{5548295499966826709ULL, "C02;French;advance, Steinitz variation"},{5577885269227554223ULL, "C33;KGA;bishop's gambit, Jaenisch variation"},
    {5603521530905652164ULL, "D52;QGD"},{5617058603479604432ULL, "A56;Czech Benoni defence"},{5619195683159520329ULL, "C43;Petrov;modern (Steinitz) attack"},{5650219941567813353ULL, "B00;Barnes defence"},
    {5650996174306987773ULL, "D38;QGD;Ragozin variation"},{5652908616881281782ULL, "B54;Sicilian;Prins (Moscow) variation"},{5665062503953452852ULL, "C31;KGD;Falkbeer, Rubinstein variation"},{5711698471952648180ULL, "C58;two knights defence;Kieseritsky variation"},
    {5717596860866146583ULL, "B38;Sicilian;accelerated fianchetto, Maroczy bind, 6.Be3"},{5729560030123667577ULL, "C10;French;Paulsen variation"},{5731307959646489143ULL, "E53;Nimzo-Indian;4.e3, Keres variation"},{5732591433978421808ULL, "B74;Sicilian;dragon, classical, Alekhine variation"},
    {5736153422875946945ULL, "E42;Nimzo-Indian;4.e3 c5, 5.Ne2 (Rubinstein)"},{5750363881038078227ULL, "C29;Vienna gambit"},{5762307724021389783ULL, "A01;Nimzovich-Larsen attack;classical variation"},{5769053880403187078ULL, "A42;Modern defence;Averbakh system"},
    {5771779641808056649ULL, "B12;Caro-Kann defence"},{5779345104891271813ULL, "B82;Sicilian;Scheveningen, 6.f4"},{5786507872850986441ULL, "C44;Scotch gambit;Cochrane variation"},{5788690032641332480ULL, "B99;Sicilian;Najdorf, 7...Be7 main line"},
    {5790638177660259151ULL, "C44;Scotch;Goering gambit"},{5798106155953508724ULL, "E62;King's Indian;fianchetto, Simagin variation"},{5798535670722779724ULL, "C50;Giuoco Pianissimo;Dubois variation"},{5813606686620497083ULL, "C50;Blackburne shilling gambit"},
    {5821864192058177866ULL, "B00;St. George (Baker) defence"},{5827040109257176541ULL, "E92;King's Indian;Gligoric-Taimanov system"},{5833748746659013380ULL, "A36;English;symmetrical variation"},{5838783503509070836ULL, "D51;QGD;Rochlin variation"},
    {5849724810919392531ULL, "D79;Neo-Gruenfeld, 6.O-O, main line"},{5850131367858392284ULL, "D59;QGD;Tartakower variation"},{5852578008285075270ULL, "C68;Ruy Lopez;exchange, Alekhine variation"},{5853697096517941516ULL, "C23;Bishop's opening;Calabrese counter-gambit, Jaenisch variation"},
    {5895810824034534397ULL, "D22;QGA;Alekhine defence"},{5895985014191127558ULL, "C00;French;Two knights variation"},{5898042839492937925ULL, "D31;QGD;semi-Slav, Marshall gambit"},{5906829219494182654ULL, "C44;Scotch gambit;Benima defence"},
    {5930847147724336314ULL, "D85;Gruenfeld;exchange variation"},{5936660378379842559ULL, "D35;QGD;exchange variation"},{5953590742962939627ULL, "E25;Nimzo-Indian;Saemisch, Keres variation"},{5961361799116378439ULL, "D16;QGD Slav;Smyslov variation"},
    {5972113319062271350ULL, "C40;Latvian;Fraser defence"},{5972362251016759277ULL, "E74;King's Indian;Averbakh, 6...c5"},{5980743501959660485ULL, "C53;Giuoco Piano;Ghulam Kassim variation"},{5983653490386634226ULL, "C49;Four knights;symmetrical, Tarrasch variation"},
    {6013925796646747904ULL, "B40;Sicilian defence"},{6036302638362420037ULL, "A00;Amsterdam attack"},{6040275869598273310ULL, "C34;KGA;Gianutio counter-gambit"},{6058714903045502924ULL, "C57;two knights defence;Fegatello attack"},
    {6062871470292195748ULL, "B60;Sicilian;Richter-Rauzer, Larsen variation"},{6064972452962018726ULL, "C44;Konstantinopolsky opening"},{6070784754735186381ULL, "B29;Sicilian;Nimzovich-Rubinstein; Rubinstein counter-gambit"},{6081264954949499945ULL, "E17;Queen's Indian;anti-Queen's Indian system"},
    {6084539597394906959ULL, "C17;French;Winawer, advance, Russian variation"},{6095363148772283862ULL, "A28;English;four knights, Nimzovich variation"},{6095701817556160340ULL, "D33;QGD;Tarrasch, Folkestone (Swedish) variation"},{6107745739976031055ULL, "A50;Queen's pawn game"},
    {6110797276000854555ULL, "B07;Pirc;Holmov system"},{6117173846449741581ULL, "B62;Sicilian;Richter-Rauzer, Podvebrady variation"},{6146132670802340821ULL, "C34;KGA;Becker defence"},{6171364007588460789ULL, "B02;Alekhine's defence;Steiner variation"},
    {6195100837149750158ULL, "C44;Ponziani;Jaenisch counter-attack"},{6201375540919042295ULL, "C21;Centre game"},{6217885557868369595ULL, "C40;QP counter-gambit (elephant gambit)"},{6218581640986178665ULL, "C33;KGA;bishop's gambit, Gifford variation"},
    {6231099701134353500ULL, "C45;Scotch;Paulsen, Gunsberg defence"},{6236394156190649432ULL, "C49;Four knights;symmetrical, Capablanca variation"},{6241414395725855133ULL, "B27;Sicilian;Hungarian variation"},{6242338578712876410ULL, "D45;QGD semi-Slav;Rubinstein (anti-Meran) system"},
    {6245513145779750995ULL, "D70;Neo-Gruenfeld defence"},{6251451985926300970ULL, "D54;QGD;Anti-neo-orthodox variation"},{6259062146484008115ULL, "C49;Four knights;Alatortsev variation"},{6273481356649437353ULL, "B07;Pirc;Chinese variation"},
    {6280293399004593488ULL, "D08;QGD;Albin counter-gambit, Alapin variation"},{6283276850867617270ULL, "C66;Ruy Lopez;Berlin defence, 4.O-O, d6"},{6290721443678847361ULL, "B73;Sicilian;dragon, classical, 8.O-O"},{6306446059879214257ULL, "C39;KGA;Allgaier, Urusov attack"},
    {6307309093674428262ULL, "E17;Queen's Indian;Euwe variation"},{6308451898672685321ULL, "B52;Sicilian;Canal-Sokolsky attack, 3...Bd7"},{6334025961278190377ULL, "B01;Scandinavian;Richter variation"},{6334249440066308105ULL, "C31;KGD;Falkbeer counter-gambit"},
    {6351288074473923746ULL, "C30;KGD;classical counter-gambit"},{6358611373085662842ULL, "C25;Vienna game, Max Lange defence"},{6359431977275087388ULL, "D20;Queen's gambit accepted"},{6361969807843992632ULL, "C31;KGD;Falkbeer, Morphy gambit"},
    {6363492045068660394ULL, "C30;KGD;classical variation"},{6368904418521154592ULL, "A11;English;Caro-Kann defensive system"},{6402221538235987380ULL, "B03;Alekhine's defence;four pawns attack, Planinc variation"},{6423668614710621179ULL, "E12;Queen's Indian;Petrosian system"},
    {6424775486022812964ULL, "A21;English opening"},{6443757371687682235ULL, "C39;KGA;Kieseritsky, Berlin defence"},{6450273619039049769ULL, "B05;Alekhine's defence;modern variation, 4...Bg4"},{6469212515891295493ULL, "C66;Ruy Lopez;closed Berlin defence, Bernstein variation"},
    {6482377080795658134ULL, "D17;QGD Slav;Czech defence"},{6495854904188427978ULL, "C52;Evans gambit;compromised defence, Paulsen variation"},{6498748208932256726ULL, "D39;QGD;Ragozin, Vienna variation"},{6515488400815299427ULL, "B18;Caro-Kann;classical, Maroczy attack"},
    {6527264100130055868ULL, "E53;Nimzo-Indian;4.e3, main line with ...c5"},{6534617444637590986ULL, "B42;Sicilian;Kan, Swiss cheese variation"},{6546725781751688272ULL, "D94;Gruenfeld with e3    Bd3"},{6552643675630037520ULL, "C00;French;King's Indian attack"},
    {6557619347084634360ULL, "D29;QGA;classical, Smyslov variation"},{6560662821814150812ULL, "A28;English;four knights, Marini variation"},{6562963218509321489ULL, "C37;KGA;Soerensen gambit"},{6574695712796336703ULL, "E41;Nimzo-Indian;4.e3 c5"},
    {6574927938774977037ULL, "B33;Sicilian;Sveshnikov variation"},{6577068094911340631ULL, "C53;Giuoco Piano;close variation"},{6578595350346402940ULL, "C29;Vienna gambit;Paulsen attack"},{6579815801524606659ULL, "B04;Alekhine's defence;modern variation"},
    {6594355908568466355ULL, "B03;Alekhine's defence;four pawns attack, Trifunovic variation"},{6598917171220123837ULL, "C48;Four knights;Marshall variation"},{6627796108611554254ULL, "A21;English, Smyslov defence"},{6650087000524810210ULL, "D35;QGD;3...Nf6"},
    {6666975158674814142ULL, "C84;Ruy Lopez;closed, centre attack"},{6667867333910869488ULL, "D31;QGD;semi-Slav, Abrahams variation"},{6676178355782868522ULL, "B08;Pirc;classical (two knights) system"},{6700620952203027281ULL, "A13;English;Kurajica defence"},
    {6718078670181270713ULL, "D51;QGD;Capablanca anti-Cambridge Springs variation"},{6736150822331169687ULL, "A66;Benoni;pawn storm variation"},{6746408475736405315ULL, "E94;King's Indian;orthodox variation"},{6750119798957118889ULL, "C77;Ruy Lopez;Anderssen variation"},
    {6752289087529265061ULL, "D05;Queen's pawn game"},{6752378593011862599ULL, "D41;QGD;Semi-Tarrasch with e3"},{6753892168587302192ULL, "D02;Queen's bishop game"},{6762675640370030147ULL, "B40;Sicilian;Pin variation (Sicilian counter-attack)"},
    {6772691998642214538ULL, "C33;KGA;bishop's gambit, Bryan counter-gambit"},{6781274111496349704ULL, "C30;KGD;Norwalde variation, Buecker gambit"},{6801197100818309865ULL, "C53;Giuoco Piano"},{6807032120198199877ULL, "A10;English;Anglo-Dutch defense"},
    {6810985866626099041ULL, "A23;English;Bremen system, Keres variation"},{6811374486606600018ULL, "C13;French;classical, Anderssen-Richter variation"},{6817217589488142063ULL, "B14;Caro-Kann;Panov-Botvinnik attack, 5...e6"},{6819832776770770342ULL, "D35;QGD;exchange, Saemisch variation"},
    {6823499964665479935ULL, "C33;KGA;bishop's gambit, Boren-Svenonius variation"},{6836300679574163603ULL, "C37;KGA;Muzio gambit"},{6841690030353988143ULL, "A01;Nimzovich-Larsen attack;Dutch variation"},{6869687573498338270ULL, "B60;Sicilian;Richter-Rauzer, Bondarevsky variation"},
    {6908246189074563724ULL, "C60;Ruy Lopez;Lucena defence"},{6916952113207929941ULL, "A43;Old Benoni defence"},{6937760897433503615ULL, "D01;Richter-Veresov attack"},{6938410237285987036ULL, "D33;QGD;Tarrasch, Wagner variation"},
    {6939059899761812818ULL, "C65;Ruy Lopez;Berlin defence, Kaufmann variation"},{6945627628053072202ULL, "C28;Vienna game"},{6966639604551846121ULL, "C55;Two knights defence"},{6973677066992584934ULL, "D80;Gruenfeld;Spike gambit"},
    {6974376703060367836ULL, "D55;QGD;Neo-orthodox variation"},{6978671897178958629ULL, "B03;Alekhine's defence;four pawns attack, Korchnoi variation"},{6985652387556296880ULL, "B62;Sicilian;Richter-Rauzer, Keres variation"},{6989458932283835270ULL, "C55;Giuoco piano;Holzhausen attack"},
    {6994004677912480002ULL, "B53;Sicilian, Chekhover variation"},{6994394603070131689ULL, "C01;French;exchange, Bogolyubov variation"},{7011465385173920496ULL, "E64;King's Indian;fianchetto, Yugoslav system"},{7011749011887917961ULL, "C27;Vienna;Alekhine variation"},
    {7015737115117358498ULL, "D40;QGD;Semi-Tarrasch defence"},{7022575765118916682ULL, "B04;Alekhine's defence;modern, Larsen variation"},{7032132308313236182ULL, "C67;Ruy Lopez;Berlin defence, Cordel variation"},{7032595338853982543ULL, "A81;Dutch;Leningrad, Basman system"},
    {7036487489174284233ULL, "C70;Ruy Lopez;Alapin's defence deferred"},{7069167869364593085ULL, "D00;Blackmar-Diemer gambit"},{7074198645352686755ULL, "A61;Benoni;Nimzovich (knight's tour) variation"},{7075484820843423532ULL, "A04;Reti;Wade defence"},
    {7078673455679606840ULL, "A80;Dutch, Von Pretzel gambit"},{7081374044964004201ULL, "D12;QGD Slav;Amsterdam variation"},{7083570967237593664ULL, "C00;French;Chigorin variation"},{7092856595585369542ULL, "D05;Queen's pawn game, Rubinstein (Colle-Zukertort) variation"},
    {7098908428682528245ULL, "B47;Sicilian;Taimanov (Bastrikov) variation"},{7126767772992061310ULL, "A00;Mieses opening"},{7130057037637819627ULL, "B56;Sicilian;Venice attack"},{7132048838067974059ULL, "B15;Caro-Kann;Tartakower (Nimzovich) variation"},
    {7134556233272653668ULL, "A41;Old Indian;Tartakower (Wade) variation"},{7134755565314023497ULL, "B32;Sicilian;Labourdonnais-Loewenthal variation"},{7142754967990650166ULL, "D15;QGD Slav;4.Nc3"},{7149228955831092776ULL, "B25;Sicilian;closed"},
    {7154507381683607326ULL, "C09;French;Tarrasch, open variation, main line"},{7172689112368504459ULL, "C57;two knights defence;Fritz variation"},{7179175872436434095ULL, "C26;Vienna;Mengarini variation"},{7188982508406003798ULL, "C13;French;classical, Anderssen variation"},
    {7227515431820872427ULL, "B20;Sicilian defence"},{7229204650735859880ULL, "B03;Alekhine's defence;exchange variation"},{7249489987118123764ULL, "B45;Sicilian;Taimanov variation"},{7267749434246727220ULL, "A28;English;four knights, Romanishin variation"},
    {7274745545138794072ULL, "C33;KGA;Tumbleweed gambit"},{7275873140549038602ULL, "C33;KGA;Lopez-Gianutio counter-gambit, Hein variation"},{7282165319408252409ULL, "C77;Ruy Lopez;Wormald (Alapin) attack"},{7295538063539664028ULL, "B01;Scandinavian gambit"},
    {7300362707947354977ULL, "E80;King's Indian;Saemisch variation"},{7312801291203024041ULL, "B88;Sicilian;Sozin, Leonhardt variation"},{7313678793504184995ULL, "A43;Old Benoni defence"},{7318372330597714294ULL, "C25;Vienna;Steinitz gambit, Zukertort defence"},
    {7355413720908220473ULL, "E60;King's Indian, 3.Nf3"},{7375263774632184409ULL, "B27;Sicilian;Stiletto (Althouse) variation"},{7383852431027124735ULL, "D95;Gruenfeld;Pachman variation"},{7396829383128827316ULL, "C02;French;advance, Nimzovich variation"},
    {7411458088805247094ULL, "B00;KP;Colorado counter"},{7418964792166995665ULL, "C20;KP;King's head opening"},{7419539248577321959ULL, "C46;Four knights;Schultze-Mueller gambit"},{7439229866371340403ULL, "D99;Gruenfeld defence;Smyslov, Yugoslav variation"},
    {7445032275338516667ULL, "A08;Reti;King's Indian attack"},{7450628771625593441ULL, "C11;French;Henneberger variation"},{7451490310770541473ULL, "A37;English;symmetrical variation"},{7456668500844184164ULL, "E60;King's Indian;Danube gambit"},
    {7457190075334030071ULL, "C68;Ruy Lopez;exchange, Romanovsky variation"},{7457822330391328050ULL, "C60;Ruy Lopez;Cozio defence, Paulsen variation"},{7458169087727242492ULL, "E48;Nimzo-Indian;4.e3 O-O, 5.Bd3 d5"},{7485866353644668772ULL, "B01;Scandinavian defence"},
    {7497599272119518264ULL, "C17;French;Winawer, advance, Rauzer variation"},{7542778785734164780ULL, "C21;Centre game, Kieseritsky variation"},{7543170342822010953ULL, "C48;Four knights;Bardeleben variation"},{7544252731494041013ULL, "C48;Four knights;Ranken variation"},
    {7547727908582792018ULL, "A57;Benko gambit"},{7557462324838829258ULL, "B07;Pirc;bayonet (Mariotti) attack"},{7560744022399469294ULL, "A47;Queen's Indian defence"},{7579097574750617442ULL, "E12;Queen's Indian;4.Nc3"},
    {7614016266946362074ULL, "C54;Giuoco Piano"},{7625431472983547307ULL, "D10;QGD Slav;Winawer counter-gambit"},{7626334951562825999ULL, "A31;English;symmetrical, Benoni formation"},{7638067310961601074ULL, "E11;Bogo-Indian defence, Gruenfeld variation"},
    {7643049641021233444ULL, "B90;Sicilian;Najdorf, Lipnitzky attack"},{7653289415459028725ULL, "B41;Sicilian;Kan, Maroczy bind (Reti variation)"},{7665449183234921684ULL, "A16;English opening"},{7667007229414863483ULL, "B44;Sicilian, Szen (`anti-Taimanov') variation"},
    {7675040924522249863ULL, "C47;Four knights;Scotch, Krause variation"},{7677908098343806236ULL, "D45;QGD semi-Slav;accelerated Meran (Alekhine variation)"},{7690717602231305109ULL, "A40;Modern defence"},{7695041882494837855ULL, "C71;Ruy Lopez;modern Steinitz defence, Three knights variation"},
    {7700360802280547492ULL, "A00;Benko's opening"},{7705356561013435660ULL, "C46;Three knights;Steinitz variation"},{7728321055871408449ULL, "C66;Ruy Lopez;closed Berlin defence, Chigorin variation"},{7739294773879529221ULL, "C33;KGA;bishop's gambit, Grimm attack"},
    {7748663110719689693ULL, "B06;Robatsch (modern) defence"},{7778754406085558885ULL, "C77;Ruy Lopez;Treybal (Bayreuth) variation (exchange var. deferred)"},{7779729978194095878ULL, "C23;Bishop's opening"},{7787168218719262297ULL, "B32;Sicilian;Nimzovich variation"},
    {7790970916446661025ULL, "C23;Bishop's opening;Wing gambit"},{7797654190461042562ULL, "D06;QGD;symmetrical (Austrian) defence"},{7819324300744909204ULL, "E50;Nimzo-Indian;4.e3 e8g8, 5.Nf3, without ...d5"},{7832314102501495047ULL, "D63;QGD;Orthodox defence, Swiss, Karlsbad variation"},
    {7840387628275896815ULL, "D31;QGD;Alapin variation"},{7856481405824098726ULL, "A43;Old Benoni;Franco-Benoni defence"},{7872991781814926785ULL, "A00;Polish;Tuebingen variation"},{7878299222988119633ULL, "C45;Scotch;Blumenfeld attack"},
    {7880270224824253100ULL, "A56;Czech Benoni;King's Indian system"},{7888481367444779554ULL, "A53;Old Indian defence"},{7913773859066904812ULL, "A10;English;Adorjan defence"},{7916861932887397130ULL, "B28;Sicilian;O'Kelly variation"},
    {7921657554596920162ULL, "A14;English;Symmetrical, Keres defence"},{7928866389593363620ULL, "D91;Gruenfeld;5.Bg5"},{7930270216542121861ULL, "D05;Queen's pawn game, Zukertort variation"},{7938857499757673409ULL, "A28;English;Nenarokov variation"},
    {7939317022744864152ULL, "C29;Vienna gambit, Wurzburger trap"},{7945323489636587663ULL, "E96;King's Indian;orthodox, 7...Nbd7, main line"},{7962625418600475433ULL, "C80;Ruy Lopez;open, Riga variation"},{7964679028221047963ULL, "C65;Ruy Lopez;Berlin defence, 4.O-O"},
    {8000911618470075185ULL, "C52;Evans gambit;Waller attack"},{8002799843668143288ULL, "B40;Sicilian;Anderssen variation"},{8007053531317466303ULL, "C22;Centre game;Berger variation"},{8018625224883908510ULL, "B00;Hippopotamus defence"},
    {8023258875909097822ULL, "C33;KGA;Lesser bishop's (Petroff-Jaenisch-Tartakower) gambit"},{8031743812832729535ULL, "D41;QGD;Semi-Tarrasch, 5.cd"},{8036761736095371394ULL, "C10;French;Fort Knox variation"},{8068467579803144842ULL, "E81;King's Indian;Saemisch, 5...O-O"},
    {8083928517957771789ULL, "C64;Ruy Lopez;classical defence, Charousek variation"},{8086437448154991119ULL, "C31;KGD;Falkbeer counter-gambit"},{8105565574373274019ULL, "D83;Gruenfeld;Gruenfeld gambit, Capablanca variation"},{8122255254093681245ULL, "B56;Sicilian"},
    {8125818152849686881ULL, "D61;QGD;Orthodox defence, Rubinstein variation"},{8135601126010369329ULL, "C25;Vienna;Pierce gambit, Rushmere attack"},{8136211434359834389ULL, "B20;Sicilian;Keres variation (2.Ne2)"},{8141109845037296819ULL, "C51;Evans gambit;5...Be7"},
    {8164618968910027045ULL, "E84;King's Indian;Saemisch, Panno main line"},{8164933841884850045ULL, "E15;Queen's Indian;Nimzovich variation (exaggerated fianchetto)"},{8174362165339909682ULL, "C55;Two knights defence, Keidanz variation"},{8211131781023620498ULL, "C49;Four knights;symmetrical, Maroczy system"},
    {8215281760737616692ULL, "B39;Sicilian;accelerated fianchetto, Breyer variation"},{8228065072346609800ULL, "A41;Robatsch defence;Rossolimo variation"},{8235051482789618506ULL, "D50;QGD;Semi-Tarrasch"},{8239429129341162895ULL, "D87;Gruenfeld;exchange, Spassky variation"},
    {8246934666706334679ULL, "A00;Venezolana opening"},{8250308691353737795ULL, "E16;Queen's Indian;Capablanca variation"},{8262382109317900144ULL, "C62;Ruy Lopez;old Steinitz defence"},{8265918214663234902ULL, "A21;English, Kramnik-Shirov counterattack"},
    {8271912480832568717ULL, "B04;Alekhine's defence;modern, Schmid variation"},{8279220457820072431ULL, "C30;KGD;classical, Hanham variation"},{8288396069743961359ULL, "D92;Gruenfeld;5.Bf4"},{8311766651675220946ULL, "C42;Petrov three knights game"},
    {8331958931828021228ULL, "D30;QGD;Vienna variation"},{8332962638114174457ULL, "B33;Sicilian;Pelikan, Chelyabinsk variation"},{8333106306578148288ULL, "C44;Scotch;Lolli variation"},{8338406761690665195ULL, "B06;Robatsch defence;Gurgenidze variation"},
    {8344203320467668907ULL, "B03;Alekhine's defence;exchange, Karpov variation"},{8352133721626920566ULL, "D97;Gruenfeld;Russian, Alekhine (Hungarian) variation"},{8352138203754464478ULL, "C74;Ruy Lopez;modern Steinitz defence, siesta variation"},{8355798640179168395ULL, "B10;Caro-Kann;Hillbilly attack"},
    {8357558241943952527ULL, "A18;English;Mikenas-Carls, Kevitz variation"},{8372981459752258726ULL, "B20;Sicilian;wing gambit, Marshall variation"},{8376432327206879586ULL, "B85;Sicilian;Scheveningen, classical main line"},{8377243767393615140ULL, "C41;Philidor;Nimzovich (Jaenisch) variation"},
    {8384683665005776262ULL, "D15;QGD Slav;Tolush-Geller gambit"},{8385589048420166136ULL, "C41;Philidor;Philidor counter-gambit, del Rio attack"},{8388032173451352435ULL, "C39;KGA;Allgaier, Thorold variation"},{8419734707596445250ULL, "C25;Vienna;Paulsen variation"},
    {8436471858628941707ULL, "C11;French;Steinitz variation"},{8440059844475688671ULL, "D15;QGD Slav;Slav gambit"},{8453113386396944556ULL, "B83;Sicilian;modern Scheveningen"},{8456856425285445024ULL, "C60;Ruy Lopez;Cozio defence"},
    {8495661371629662739ULL, "D25;QGA, 4.e3"},{8502096716139935690ULL, "B73;Sicilian;dragon, classical, Richter variation"},{8513933480973391558ULL, "C22;Centre game;Hall variation"},{8518916294944754522ULL, "C27;Vienna game"},
    {8526886967310536871ULL, "D34;QGD;Tarrasch, Stoltz variation"},{8537608491875127911ULL, "A09;Reti;advance variation"},{8542267209002381339ULL, "C52;Evans gambit"},{8553990710221647837ULL, "A40;Beefeater defence"},
    {8569698848803085254ULL, "D02;Queen's pawn game, Chigorin variation"},{8579477275768447482ULL, "C69;Ruy Lopez;exchange variation, Alapin gambit"},{8586731771773089874ULL, "C33;KGA;bishop's gambit, Paulsen attack"},{8589891930836139257ULL, "D25;QGA, Janowsky-Larsen variation"},
    {8597928191213444463ULL, "A33;English;symmetrical variation"},{8599061658575529542ULL, "A83;Dutch;Staunton gambit, Staunton's line"},{8608186966401072084ULL, "B02;Alekhine's defence;Maroczy variation"},{8611923875122808275ULL, "A00;Lasker simul special"},
    {8617926507036524191ULL, "E82;King's Indian;Saemisch, double fianchetto variation"},{8619941203352494258ULL, "C00;French;Pelikan variation"},{8630710310852080463ULL, "B13;Caro-Kann;exchange variation"},{8649975948799156118ULL, "A36;English;symmetrical, Botvinnik system"},
    {8660046002141854195ULL, "A51;Budapest;Fajarowicz variation"},{8664855540879091345ULL, "A95;Dutch;stonewall: Chekhover variation"},{8665684580543591106ULL, "B13;Caro-Kann;Panov-Botvinnik, Herzog defence"},{8668792095776880557ULL, "C14;French;classical, Tarrasch variation"},
    {8674351781538062003ULL, "E73;King's Indian;5.Be2"},{8676455461131418307ULL, "C00;French defence, Steiner variation"},{8681502059140600126ULL, "E46;Nimzo-Indian;Simagin variation"},{8682556407983504046ULL, "C45;Scotch;Potter variation"},
    {8704797333742910878ULL, "C44;King's pawn game"},{8710519269233481727ULL, "C31;KGD;Falkbeer, 3...e4"},{8713630978339158697ULL, "C00;French;Reti (Spielmann) variation"},{8717402588625579987ULL, "C14;French;classical, Pollock variation"},
    {8725084518162839374ULL, "C03;French;Tarrasch, Haberditz variation"},{8740166013729832498ULL, "D72;Neo-Gruenfeld, 5.cd, main line"},{8742708428188418529ULL, "A61;Benoni;Uhlmann variation"},{8795354450913215585ULL, "C12;French;MacCutcheon, advance variation"},
    {8800780198616876321ULL, "B92;Sicilian;Najdorf, Opovcensky variation"},{8836920027637111400ULL, "A28;English;Bradley Beach variation"},{8839051919898350604ULL, "B90;Sicilian;Najdorf, Byrne (English) attack"},{8848498162624799579ULL, "D46;QGD semi-Slav;Chigorin defence"},
    {8860295162392952087ULL, "D90;Gruenfeld;Flohr variation"},{8865631088705693475ULL, "A70;Benoni;classical with e4 and Nf3"},{8875742382281503761ULL, "B54;Sicilian"},{8881680881265163375ULL, "E76;King's Indian;Four pawns attack, dynamic line"},
    {8892225506042186784ULL, "B01;Scandinavian defence"},{8900205748586513901ULL, "E31;Nimzo-Indian;Leningrad, main line"},{8911933793155644757ULL, "A45;Queen's pawn;Bronstein gambit"},{8940516977523981116ULL, "A35;English;symmetrical variation"},
    {8952007022587676164ULL, "B61;Sicilian;Richter-Rauzer, Larsen variation, 7.Qd2"},{8962827899832153943ULL, "D50;QGD;Been-Koomen variation"},{8987890325611041745ULL, "B02;Alekhine's defence"},{8992032561167977146ULL, "D55;QGD;Pillsbury attack"},
    {9004103863947024230ULL, "D12;QGD Slav;Landau variation"},{9117668792349434880ULL, "C53;Giuoco Piano;centre-holding variation"},{9123848104385492518ULL, "C20;KP;Lopez opening"},{9124561342725515779ULL, "A25;English;closed system (without ...d6)"},
    {9126715053321457917ULL, "E16;Queen's Indian;Riumin variation"},{9129938131945472771ULL, "A00;Polish;Outflank variation"},{9130309906484160158ULL, "E77;King's Indian;Four pawns attack, Florentine gambit"},{9133416567439554388ULL, "D89;Gruenfeld;exchange, Sokolsky variation"},
    {9137085317498418749ULL, "A60;Benoni defence"},{9138779769076190871ULL, "E20;Nimzo-Indian;Kmoch variation"},{9141089396830326901ULL, "A12;English;London defensive system"},{9143753658628420244ULL, "C29;Vienna gambit, Steinitz variation"},
    {9148961895944248211ULL, "C51;Evans gambit declined, Lange variation"},{9171332311576742578ULL, "C45;Scotch;Ghulam Kassim variation"},{9181538511791674715ULL, "C33;KGA;bishop's gambit"},{9194844043182681930ULL, "C70;Ruy Lopez;Bird's defence deferred"},
    {9220078764816103069ULL, "D26;QGA;classical variation, 6.O-O"},{9238254383505225726ULL, "B03;Alekhine's defence"},{9246797226090997570ULL, "E63;King's Indian;fianchetto, Panno variation"},{9247430539336927945ULL, "C22;Centre game"},
    {9251867629257164164ULL, "C05;French;Tarrasch, Botvinnik variation"},{9254585899963942805ULL, "C69;Ruy Lopez;exchange variation, 5.O-O"},{9255784704506865032ULL, "D71;Neo-Gruenfeld, 5.cd"},{9258073315008182915ULL, "C33;KGA;Pawn's gambit (Stamma gambit)"},
    {9259818565525731695ULL, "C55;two knights;Max Lange attack, Rubinstein variation"},{9282994836826930403ULL, "D52;QGD;Cambridge Springs defence, Argentine variation"},{9302537045783472630ULL, "C37;KGA;Lolli gambit (wild Muzio gambit)"},{9310043142453278581ULL, "B01;Scandinavian;Icelandic gambit"},
    {9317003736629728014ULL, "D99;Gruenfeld defence;Smyslov, main line"},{9350771952327409460ULL, "B80;Sicilian;Scheveningen, fianchetto variation"},{9380038344921021019ULL, "C37;KGA;Muzio gambit, Holloway defence"},{9384546495678726550ULL, "B00;King's pawn opening"},
    {9401296748944965564ULL, "C51;Evans gambit declined, Pavlov variation"},{9413637885052890014ULL, "B02;Alekhine's defence;Mokele Mbembe (Buecker) variation"},{9413680960686161580ULL, "B13;Caro-Kann;Panov-Botvinnik, Reifir (Spielmann) variation"},{9424599194644066312ULL, "C37;KGA;Rosentreter gambit"},
    {9427584351514361746ULL, "B72;Sicilian;dragon, classical attack"},{9431628779807295340ULL, "E11;Bogo-Indian defence, Nimzovich variation"},{9443689642921087454ULL, "A40;Queen's pawn"},{9447829756215205476ULL, "A26;English;Botvinnik system"},
    {9450189495482432289ULL, "D21;QGA;3.Nf3"},{9464052383375758531ULL, "A25;English;closed, 5.Rb1"},{9465088315907768135ULL, "C46;Three knights game"},{9465637702427185146ULL, "B74;Sicilian;dragon, classical, Bernard defence"},
    {9468258918230536011ULL, "B97;Sicilian;Najdorf, Poisoned pawn variation"},{9471544435432078546ULL, "C67;Ruy Lopez;open Berlin defence, Showalter variation"},{9524224908858561610ULL, "C14;French;classical, Stahlberg variation"},{9547182507883882719ULL, "C78;Ruy Lopez;...b5 & ...d6"},
    {9554702971602931048ULL, "A43;Old Benoni defence"},{9556430513147556314ULL, "C48;Four knights;Rubinstein counter-gambit, 5.Be2"},{9569824111274234592ULL, "D17;QGD Slav;Krause attack"},{9573249886271021783ULL, "C26;Vienna;Paulsen-Mieses variation"},
    {9626097476822300502ULL, "C45;Scotch;Meitner variation"},{9630112696199530476ULL, "C55;Two knights defence"},{9632950545341173905ULL, "D50;QGD;Semi-Tarrasch, Primitive Pillsbury variation"},{9697147653288649403ULL, "B50;Sicilian"},
    {9710347368368175195ULL, "C15;French;Winawer (Nimzovich) variation"},{9722783023074688851ULL, "D02;Queen's pawn game"},{9727685523403278214ULL, "C47;Four knights;Scotch, 4...exd4"},{9730558558930829353ULL, "C10;French;Rubinstein variation"},
    {9736004374854471041ULL, "D35;QGD;Harrwitz attack"},{9740298646935949576ULL, "B02;Alekhine's defence;Saemisch attack"},{9740501023354455775ULL, "D37;QGD;4.Nf3"},{9747602586449279330ULL, "C60;Ruy Lopez;Nuernberg variation"},
    {9779590214445875170ULL, "C00;French;Wing gambit"},{9791469005209978584ULL, "A43;Hawk (Habichd) defence"},{9793294892616765195ULL, "C39;KGA;Kieseritsky, Brentano (Campbell) defence"},{9802761639032932468ULL, "B45;Sicilian;Taimanov, American attack"},
    {9807010775991373500ULL, "A12;English;Caro-Kann defensive system, Bogolyubov variation"},{9821875977625437987ULL, "D36;QGD;exchange, positional line, 6.Qc2"},{9841844171730982386ULL, "B13;Caro-Kann;Panov-Botvinnik attack"},{9848146488601049887ULL, "C41;Philidor;exchange variation"},
    {9852794038369583593ULL, "B21;Sicilian;Smith-Morra gambit"},{9857899207991007499ULL, "C42;Petrov's defence"},{9872270298815912160ULL, "B40;Sicilian;Marshall variation"},{9890053959501832392ULL, "B58;Sicilian;Boleslavsky variation"},
    {9901403333539929343ULL, "C39;KGA;Allgaier gambit"},{9914458403282913637ULL, "C25;Vienna;Steinitz gambit"},{9944936503865807924ULL, "C52;Evans gambit"},{9945452212337382115ULL, "B07;Robatsch defence;Geller's system"},
    {9954506542371257063ULL, "D86;Gruenfeld;exchange, classical variation"},{9963937660605248767ULL, "D06;Queen's Gambit"},{9967485564787079474ULL, "A05;Reti;King's Indian attack, Spassky's variation"},{9985741066289207165ULL, "D90;Gruenfeld;Schlechter variation"},
    {10000887720397881430ULL, "A12;English;Caro-Kann defensive system"},{10006536212211226745ULL, "C33;KGA;Carrera (Basman) gambit"},{10032596905535576878ULL, "C42;Petrov;Damiano variation"},{10037313390410864515ULL, "C37;KGA;Silberschmidt gambit"},
    {10057522960031609035ULL, "C25;Vienna;Steinitz gambit, Fraser-Minckwitz variation"},{10059319811599020044ULL, "D45;QGD semi-Slav;5...Nd7"},{10065497480844495062ULL, "C31;KGD;Falkbeer, Milner-Barry variation"},{10082451531383332020ULL, "B01;Scandinavian defence, Gruenfeld variation"},
    {10094560481859703026ULL, "B62;Sicilian;Richter-Rauzer, Richter attack"},{10120818753282595405ULL, "C44;Scotch gambit"},{10126329347641959897ULL, "D33;QGD;Tarrasch, Schlechter-Rubinstein system"},{10141743729398568537ULL, "A66;Benoni;Mikenas variation"},
    {10154625774945919283ULL, "B36;Sicilian;accelerated fianchetto, Maroczy bind"},{10191521888709582222ULL, "A39;English;symmetrical, main line with d4"},{10195171887328851309ULL, "C78;Ruy Lopez;5.O-O"},{10196596860375990978ULL, "A17;English;Queens Indian, Romanishin variation"},
    {10207310966796677852ULL, "A00;Anderssen's opening"},{10227515841364189571ULL, "D01;Richter-Veresov attack, Richter variation"},{10268277383547746094ULL, "E15;Queen's Indian;Buerger variation"},{10276490734490287865ULL, "B12;Caro-Masi defence"},
    {10295226814722216123ULL, "D53;QGD;4.Bg5 Be7"},{10307934480497318063ULL, "E59;Nimzo-Indian;4.e3, main line"},{10309150487395993318ULL, "C50;Rousseau gambit"},{10347470135956252646ULL, "E10;Blumenfeld counter-gambit accepted"},
    {10349734187458260719ULL, "C37;KGA;Blachly gambit"},{10367264654225476305ULL, "E77;King's Indian;Four pawns attack, 6.Be2"},{10409587528771951618ULL, "A45;Gedult attack"},{10411958252427798672ULL, "C33;KGA;Orsini gambit"},
    {10413970644442290061ULL, "C42;Petrov;classical attack, close variation"},{10416853657022933212ULL, "A28;English;four knights system"},{10421124793953940062ULL, "D33;QGD;Tarrasch, Schlechter-Rubinstein system, Rey Ardid variation"},{10436188760530291480ULL, "C64;Ruy Lopez;classical defence, 4.c3"},
    {10440258736823854431ULL, "C07;French;Tarrasch, Eliskases variation"},{10440541523478855988ULL, "C33;KGA;bishop's gambit, Greco variation"},{10443369633151123043ULL, "C79;Ruy Lopez;Steinitz defence deferred, Rubinstein variation"},{10459447319415350958ULL, "E26;Nimzo-Indian;Saemisch, O'Kelly variation"},
    {10474633245981484678ULL, "C71;Ruy Lopez;modern Steinitz defence"},{10480934859288532583ULL, "A01;Nimzovich-Larsen attack;Indian variation"},{10489440406649270588ULL, "E60;King's Indian;3.g3"},{10511146789299227800ULL, "A46;Queen's pawn;Torre attack, Wagner gambit"},
    {10517661917435401741ULL, "A15;English, 1...Nf6 (Anglo-Indian defense)"},{10527540912022553896ULL, "B00;KP;Nimzovich defence, Bogolyubov variation"},{10538982899110253730ULL, "A12;English;New York (London) defensive system"},{10549399229582865829ULL, "C15;French;Winawer, Alekhine (Maroczy) gambit"},
    {10563808219651015669ULL, "B27;Sicilian;Acton extension"},{10568551354753509258ULL, "D96;Gruenfeld;Russian variation"},{10572558241368800699ULL, "E12;Queen's Indian defence"},{10581169051009212835ULL, "B20;Sicilian;wing gambit, Marienbad variation"},
    {10628194877928326530ULL, "A30;English;symmetrical, hedgehog, flexible formation"},{10641166529569284813ULL, "B00;KP;Nimzovich defence, Wheeler gambit"},{10649727320182616230ULL, "C01;French;exchange variation"},{10657973619178668561ULL, "A61;Benoni;fianchetto variation"},
    {10660195728854060915ULL, "C41;Philidor;Hanham, Krause variation"},{10666994214560458892ULL, "A41;Modern defence"},{10670936126110070912ULL, "A09;Reti opening"},{10677929950469043692ULL, "C42;Petrov;Nimzovich attack"},
    {10705963060107852010ULL, "A06;Reti;Nimzovich-Larsen attack"},{10714099245185518155ULL, "D52;QGD;Cambridge Springs defence"},{10715610013939775290ULL, "C11;French;Steinitz, Boleslavsky variation"},{10718018622446991410ULL, "B89;Sicilian;Velimirovic attack"},
    {10723184175425273718ULL, "D60;QGD;Orthodox defence, Botvinnik variation"},{10723468565931681235ULL, "C35;KGA;Cunningham, Bertin gambit"},{10732149639755857990ULL, "C38;King's knight's gambit"},{10750749601115687943ULL, "B06;Robatsch defence;two knights, Suttles variation"},
    {10750795572256307104ULL, "C00;French;Steinitz attack"},{10753953036473210942ULL, "E09;Catalan;closed, Sokolsky variation"},{10757985137015468971ULL, "B79;Sicilian;dragon, Yugoslav attack, 12.h4"},{10766341288315023740ULL, "A13;English;Neo-Catalan"},
    {10776206281345784122ULL, "C44;Ponziani;Steinitz variation"},{10778872751806831593ULL, "C44;Tayler opening"},{10796586250350100215ULL, "D21;QGA;Alekhine defense, Borisenko-Furman variation"},{10798791872932476367ULL, "D31;QGD;semi-Slav"},
    {10807105519919809025ULL, "C80;Ruy Lopez;open, 6.d4 b5"},{10814512193657048756ULL, "C33;KGA;bishop's gambit, Boden defence"},{10846997930875023666ULL, "E85;King's Indian;Saemisch, orthodox variation"},{10847114058417957315ULL, "C65;Ruy Lopez;Berlin defence, Mortimer trap"},
    {10855744188163820281ULL, "D51;QGD;5...c6"},{10856056287042004054ULL, "C70;Ruy Lopez;Cozio defence deferred"},{10856512813371396665ULL, "C78;Ruy Lopez;Rabinovich variation"},{10859697265682008604ULL, "D62;QGD;Orthodox defence, 7.Qc2 c5, 8.cd (Rubinstein)"},
    {10880765158922542751ULL, "C45;Scotch;Rosenthal variation"},{10911599738958481612ULL, "C52;Evans gambit;compromised defence"},{10916707893656755707ULL, "E21;Nimzo-Indian;three knights, Korchnoi variation"},{10926986651431801156ULL, "C51;Evans gambit declined, 5.a4"},
    {10936701698831271194ULL, "C25;Vienna gambit"},{10957956849732313389ULL, "D05;Colle system"},{11050487627966281570ULL, "B21;Sicilian;Grand Prix attack"},{11060058610116144967ULL, "C39;KGA;Kieseritsky, Salvio (Rosenthal) defence"},
    {11074621136697514492ULL, "C84;Ruy Lopez;closed defence"},{11087077936265082894ULL, "E92;King's Indian;Petrosian system"},{11095744431524963257ULL, "C51;Evans counter-gambit"},{11102373440004537386ULL, "D50;QGD;4.Bg5"},
    {11108533049700957218ULL, "C64;Ruy Lopez;classical defence, Benelux variation  "},{11114434025341711937ULL, "A44;Old Benoni defence"},{11130709217671602118ULL, "C44;Ponziani counter-gambit"},{11140423004911984214ULL, "D52;QGD;Cambridge Springs defence, 7.cd"},
    {11170314112726847120ULL, "A20;English, Nimzovich variation"},{11187045220831244481ULL, "B83;Sicilian;modern Scheveningen, main line"},{11187574861719361287ULL, "A84;Dutch defence"},{11193585894164008875ULL, "C53;Giuoco Piano"},
    {11193878275596374542ULL, "A36;English;symmetrical, Botvinnik system reversed"},{11195698146774113253ULL, "C41;Philidor;Hanham, Delmar variation"},{11204133280730130991ULL, "D08;QGD;Albin counter-gambit, Janowski variation"},{11204863308318751962ULL, "B22;Sicilian;2.c3, Heidenfeld variation"},
    {11210729043333978658ULL, "D52;QGD;Cambridge Springs defence, Rubinstein variation"},{11249230053398005913ULL, "B82;Sicilian;Scheveningen, Tal variation"},{11252375581820946937ULL, "B13;Caro-Kann;Panov-Botvinnik, Czerniak variation"},{11260741761627940630ULL, "D20;QGA;3.e4"},
    {11261461102376260493ULL, "A33;English;symmetrical, Geller variation"},{11264079407061836630ULL, "C34;KGA;Schallop defence"},{11264334285667164248ULL, "D25;QGA, Smyslov variation"},{11269670497880521002ULL, "B10;Caro-Kann;anti-Caro-Kann defence"},
    {11271975782963195580ULL, "C61;Ruy Lopez;Bird's defence"},{11275848248959733141ULL, "E87;King's Indian;Saemisch, orthodox, Bronstein variation"},{11279980452238510901ULL, "C84;Ruy Lopez;closed, Basque gambit (North Spanish variation)"},{11280029533036916276ULL, "C52;Evans gambit;Richardson attack"},
    {11287977242886629174ULL, "D86;Gruenfeld;exchange, Larsen variation"},{11294981936396335633ULL, "D31;QGD;semi-Slav, Noteboom variation"},{11304418794852451627ULL, "C51;Evans gambit;Mayet defence"},{11311643765603137124ULL, "D22;QGA;Haberditz variation"},
    {11316275861823337272ULL, "C25;Vienna;Fyfe gambit"},{11325643120480356780ULL, "C88;Ruy Lopez;Trajkovic counter-attack"},{11335493363416619824ULL, "E36;Nimzo-Indian;classical, Botvinnik variation"},{11339917551484771745ULL, "A04;Reti opening"},
    {11345787015959633054ULL, "A93;Dutch;stonewall, Botwinnik variation"},{11351945406822001960ULL, "C13;French;classical, Tartakower variation"},{11394481812264647585ULL, "B25;Sicilian;closed, 6.f4"},{11410947445616469291ULL, "C67;Ruy Lopez;Berlin defence, Minckwitz variation"},
    {11429474710770573794ULL, "E75;King's Indian;Averbakh, main line"},{11432035893002844285ULL, "C39;KGA;Allgaier, Horny defence"},{11435137974693267535ULL, "B65;Sicilian;Richter-Rauzer, Rauzer attack, 7...Be7 defence, 9...Nxd4"},{11438475542861888642ULL, "C56;two knights defence;Yurdansky attack"},
    {11454147557610188277ULL, "C19;French;Winawer, advance, 6...Ne7"},{11456303556016632975ULL, "B80;Sicilian;Scheveningen, Vitolins variation"},{11460610026420291629ULL, "B44;Sicilian defence"},{11475173352388847216ULL, "A57;Benko gambit;Nescafe Frappe attack"},
    {11492041941439466413ULL, "C44;Scotch gambit;Vitzhum attack"},{11501372889211137741ULL, "C66;Ruy Lopez;Berlin defence, hedgehog variation"},{11518609555277006898ULL, "B23;Sicilian;closed"},{11521588610279463488ULL, "B75;Sicilian;dragon, Yugoslav attack"},
    {11546669789268735507ULL, "C33;KGA;bishop's gambit, Fraser variation"},{11546853506016128054ULL, "E10;Blumenfeld counter-gambit, Spielmann variation"},{11569590768174408155ULL, "D84;Gruenfeld;Gruenfeld gambit accepted"},{11574135247419513079ULL, "C22;Centre game;l'Hermet variation"},
    {11581835722185434696ULL, "C18;French;Winawer, advance variation"},{11598545319315785617ULL, "A34;English;symmetrical variation"},{11610082673200119466ULL, "D97;Gruenfeld;Russian variation with e4"},{11635161049092548521ULL, "B21;Sicilian;Smith-Morra gambit"},
    {11636144662482050434ULL, "E32;Nimzo-Indian;classical variation"},{11670162937130983530ULL, "A01;Nimzovich-Larsen attack;symmetrical variation"},{11720378311088045142ULL, "C60;Ruy Lopez;fianchetto (Smyslov/Barnes) defence"},{11734955749324608324ULL, "B37;Sicilian;accelerated fianchetto, Simagin variation"},
    {11761520363290152770ULL, "C79;Ruy Lopez;Steinitz defence deferred, Boleslavsky variation"},{11764481108009782615ULL, "E46;Nimzo-Indian;Reshevsky variation"},{11769381156871334711ULL, "D42;QGD;Semi-Tarrasch, 7.Bd3"},{11802956744823910464ULL, "D98;Gruenfeld;Russian, Smyslov variation"},
    {11820585119057594743ULL, "C62;Ruy Lopez;old Steinitz defence, Nimzovich attack"},{11825104619476498147ULL, "A00;Grob;Fritz gambit"},{11844126108305170593ULL, "B31;Sicilian;Nimzovich-Rossolimo attack, Gurgenidze variation"},{11846596372847159731ULL, "C58;two knights defence;Maroczy variation"},
    {11880451404406887404ULL, "D15;QGD Slav;Suechting variation"},{11880578151788722261ULL, "C21;Danish gambit;Collijn defence"},{11882163293698118860ULL, "A04;Reti opening"},{11899196753284809223ULL, "D94;Gruenfeld;Makogonov variation"},
    {11900812969276515315ULL, "C25;Vienna gambit"},{11924653664013535291ULL, "C12;French;MacCutcheon, Janowski variation"},{11937437613677974713ULL, "D00;Queen's pawn;stonewall attack"},{11937646274597073550ULL, "A16;English;Anglo-Gruenfeld defense"},
    {11937763961330085987ULL, "D08;QGD;Albin counter-gambit"},{11943384304268263767ULL, "C21;Danish gambit;Soerensen defence"},{11996905801652612670ULL, "A43;Woozle defence"},{12008229486823844169ULL, "C18;French;Winawer, classical variation"},
    {12019341784461806074ULL, "C80;Ruy Lopez;open, Tartakower variation"},{12024699855778873972ULL, "C20;KP;Indian opening"},{12032122569665008966ULL, "C33;KGA;Breyer gambit"},{12035501163237529194ULL, "D40;QGD;Semi-Tarrasch defence, Pillsbury variation"},
    {12040982420169220681ULL, "A47;Queen's Indian;Marienbad system"},{12047011437325700351ULL, "B02;Alekhine's defence;Scandinavian variation"},{12086870807141892264ULL, "C02;French;advance, Milner-Barry gambit"},{12095545916465242717ULL, "C41;Philidor;Hanham, Berger variation"},
    {12123056109416905455ULL, "C26;Vienna;Falkbeer variation"},{12126346875789483836ULL, "C20;KP;Patzer opening"},{12128944549807902572ULL, "D90;Gruenfeld;Three knights variation"},{12155980846752612527ULL, "A44;Semi-Benoni (`blockade variation')"},
    {12170917594475360235ULL, "C70;Ruy Lopez"},{12183779070392780163ULL, "A00;Amar gambit"},{12184158585879788128ULL, "A05;Reti;King's Indian attack, Reti-Smyslov variation"},{12188224557340591944ULL, "E90;King's Indian;Larsen variation"},
    {12209046555223925806ULL, "C39;KGA;Kieseritsky, Neumann defence"},{12226566814869870181ULL, "E91;King's Indian;6.Be2"},{12245558485149503974ULL, "D51;QGD;4.Bg5 Nbd7"},{12246041459958692612ULL, "C10;French;Marshall variation"},
    {12257781316999965350ULL, "A10;English;Jaenisch gambit"},{12267328170143667650ULL, "E93;King's Indian;Petrosian system, Keres variation"},{12276252203521325354ULL, "E89;King's Indian;Saemisch, orthodox main line"},{12287604782832553968ULL, "C30;KGD;classical, Marshall attack"},
    {12289645807193135735ULL, "C42;Petrov;classical attack"},{12293028565421730508ULL, "A01;Nimzovich-Larsen attack;Polish variation"},{12300353481695348585ULL, "C45;Scotch game"},{12306658812977763238ULL, "B06;Norwegian defence"},
    {12328635236372843080ULL, "B09;Pirc;Austrian attack"},{12329108929078869374ULL, "C21;Halasz gambit"},{12334186494798937227ULL, "C52;Evans gambit;Lasker defence"},{12334743496750033148ULL, "D35;QGD;exchange, positional line"},
    {12347780517185694514ULL, "C31;KGD;Falkbeer, Tartakower variation"},{12361927582217565072ULL, "B15;Caro-Kann defence"},{12363724230535936381ULL, "C39;KGA;Kieseritsky, Salvio defence, Cozio variation"},{12401632153706811936ULL, "A17;English;Queens Indian formation"},
    {12436862047523892809ULL, "D16;QGD Slav;Soultanbeieff variation"},{12450811853906887694ULL, "C55;two knights;Max Lange attack"},{12452455539364017275ULL, "C48;Four knights;Spielmann variation"},{12466668608055029944ULL, "A96;Dutch;classical variation"},
    {12467016253499543105ULL, "D27;QGA;classical, 6...a6"},{12471617168689240028ULL, "A99;Dutch;Ilyin-Genevsky variation with b3"},{12474242559928114764ULL, "A94;Dutch;stonewall with Ba3"},{12475576582682896140ULL, "B80;Sicilian;Scheveningen variation"},
    {12492327627645676135ULL, "C27;Boden-Kieseritsky gambit"},{12495630110652754333ULL, "C52;Evans gambit;Levenfish variation"},{12497905221412858754ULL, "B44;Sicilian, Szen variation, Dely-Kasparov gambit"},{12499638935386099713ULL, "E21;Nimzo-Indian;three knights, Euwe variation"},
    {12551635427254113079ULL, "C22;Centre game;Charousek variation"},{12587662054977172527ULL, "A92;Dutch defence, Alekhine variation"},{12591179326086432191ULL, "C30;KGD;2...Nf6"},{12594399537838306670ULL, "A00;Clemenz (Mead's, Basman's or de Klerk's) opening"},
    {12598836761883668116ULL, "C29;Vienna gambit;Kaufmann variation"},{12602625950758157112ULL, "C23;Bishop's opening;MacDonnell double gambit"},{12618344685100861441ULL, "C12;French;MacCutcheon, Lasker variation, 8...g6"},{12632403227914703711ULL, "C15;French;Winawer, fingerslip variation"},
    {12647370526901136001ULL, "C27;Vienna game"},{12649605828407190515ULL, "D19;QGD Slav;Dutch, Saemisch variation"},{12669683955602084722ULL, "B32;Sicilian;Labourdonnais-Loewenthal (Kalashnikov) variation"},{12673146187968003320ULL, "C44;Scotch;Cochrane variation"},
    {12678731240525364976ULL, "E17;Queen's Indian;5.Bg2 Be7"},{12683611248497553553ULL, "E20;Nimzo-Indian;Mikenas attack"},{12687609549800354016ULL, "A07;Reti;King's Indian attack, Keres variation"},{12688353036496644096ULL, "A57;Benko gambit;Zaitsev system"},
    {12688512350834521620ULL, "B20;Sicilian;Gloria variation"},{12689944874628293289ULL, "A40;Queen's pawn;Franco-Indian (Keres) defence"},{12695818810506447363ULL, "C23;Bishop's opening;Philidor variation"},{12702732178696280511ULL, "C15;French;Winawer, Alekhine gambit"},
    {12726056998635698893ULL, "C41;Philidor;Paulsen attack"},{12745783186277945554ULL, "D32;QGD;Tarrasch, von Hennig-Schara gambit"},{12752541267343654098ULL, "C52;Evans gambit;Tartakower attack"},{12752974222999530500ULL, "B49;Sicilian;Taimanov variation"},
    {12759456302157189868ULL, "E24;Nimzo-Indian;Saemisch variation"},{12764908881138920405ULL, "D15;QGD Slav accepted"},{12785758996267438555ULL, "B05;Alekhine's defence;modern, Panov variation"},{12825934360362564846ULL, "B65;Sicilian;Richter-Rauzer, Rauzer attack, 7...Be7 defence, 9...Nxd4"},
    {12832231678582895510ULL, "B74;Sicilian;dragon, classical, Reti-Tartakower variation"},{12843862339604345138ULL, "C37;KGA;Muzio gambit, From defence"},{12843943375577172615ULL, "C39;KGA;Kieseritsky, Paulsen defence"},{12844381245793831081ULL, "E62;King's Indian;fianchetto, Kavalek (Bronstein) variation"},
    {12854493956840039476ULL, "A03;Mujannah opening"},{12855217450283298587ULL, "C20;KP;Napoleon's opening"},{12859821892952862703ULL, "C12;French;MacCutcheon, Bernstein variation"},{12867417049842150619ULL, "E25;Nimzo-Indian;Saemisch variation"},
    {12868335398863444457ULL, "B33;Sicilian defence"},{12876569570225827890ULL, "A56;Benoni defence"},{12880344782812331939ULL, "A06;Santasiere's folly"},{12883339614184792209ULL, "E78;King's Indian;Four pawns attack, with Be2 and Nf3"},
    {12890534442513187976ULL, "B04;Alekhine's defence;modern, fianchetto variation"},{12896792957045939448ULL, "A08;Reti;King's Indian attack, French variation"},{12906134191187318657ULL, "B42;Sicilian;Kan, 5.Bd3"},{12946094891610192623ULL, "A46;Queen's pawn;Torre attack"},
    {12951373291170894696ULL, "C53;Giuoco Piano;Anderssen variation"},{12971159600028455270ULL, "A20;English, Nimzovich, Flohr variation"},{12972329009009748497ULL, "D94;Gruenfeld;Smyslov defence"},{12976107337049617056ULL, "C00;French defence"},
    {12982997005380161821ULL, "A80;Dutch, Krejcik gambit"},{12994381813327927256ULL, "B08;Pirc;classical, h3 system"},{13000031694732271177ULL, "A42;Pterodactyl defence"},{13011961992712907920ULL, "E73;King's Indian;Averbakh system"},
    {13026117326258355200ULL, "C79;Ruy Lopez;Steinitz defence deferred (Russian defence)"},{13029420206032201895ULL, "C41;Philidor;exchange variation"},{13030348898112066787ULL, "E66;King's Indian;fianchetto, Yugoslav Panno"},{13033153909283277889ULL, "E67;King's Indian;fianchetto with ...Nd7"},
    {13057863971960878842ULL, "C43;Petrov;Urusov gambit"},{13064346338305306936ULL, "B51;Sicilian;Canal-Sokolsky (Nimzovich-Rossolimo, Moscow) attack"},{13071162599844226223ULL, "B87;Sicilian;Sozin with ...a6 and ...b5"},{13093998103905793424ULL, "D50;QGD;Canal (Venice) variation"},
    {13105370460902735855ULL, "B10;Caro-Kann;two knights variation"},{13118478456130223644ULL, "C32;KGD;Falkbeer, Alapin variation"},{13137399742741380921ULL, "A45;Paleface attack"},{13140985555652114401ULL, "E07;Catalan;closed, 6...Nbd7"},
    {13142444336106062267ULL, "C50;Giuoco Piano"},{13145071938140221992ULL, "B46;Sicilian;Taimanov variation"},{13151384280534173382ULL, "D12;QGD Slav;4.e3 Bf5"},{13155466615480084563ULL, "A06;Reti;old Indian attack"},
    {13155468416299335208ULL, "B02;Alekhine's defence;two pawns' (Lasker's) attack"},{13156395808987896582ULL, "C05;French;Tarrasch, closed variation"},{13170171340924440238ULL, "B03;Alekhine's defence;four pawns attack, 7.Be3"},{13178195687193905557ULL, "B60;Sicilian;Richter-Rauzer"},
    {13188355995154410871ULL, "A80;Dutch, Manhattan (Alapin, Ulvestad) variation"},{13190034841581924613ULL, "B11;Caro-Kann;two knights, 3...Bg4"},{13194869117112790023ULL, "E83;King's Indian;Saemisch, 6...Nc6"},{13205222471382339188ULL, "B02;Alekhine's defence;Kmoch variation"},
    {13228745671319008944ULL, "D35;QGD;exchange, chameleon variation"},{13251629644267684009ULL, "E46;Nimzo-Indian;4.e3 O-O"},{13292822591690555697ULL, "B05;Alekhine's defence;modern, Flohr variation"},{13293105817214541248ULL, "D45;QGD semi-Slav;5.e3"},
    {13312280957642488096ULL, "C50;Giuoco Pianissimo;Italian four knights variation"},{13317742521215745992ULL, "C53;Giuoco Piano;Mestel variation"},{13346022947248891095ULL, "B27;Sicilian;Quinteros variation"},{13358301718275044216ULL, "C39;KGA;Kieseritsky, Brentano defence"},
    {13370704657651760787ULL, "B03;Alekhine's defence"},{13371046594381923352ULL, "B95;Sicilian;Najdorf, 6...e6"},{13428589215933096846ULL, "D18;QGD Slav;Dutch, Lasker variation"},{13430045858224199334ULL, "B02;Alekhine's defence;Krejcik variation"},
    {13435846850863880217ULL, "C21;Danish gambit;Schlechter defence"},{13447093783882200946ULL, "C30;KGD;Keene's defence"},{13452720276273602382ULL, "C65;Ruy Lopez;Berlin defence, Mortimer variation"},{13453430573722802355ULL, "A41;Queen's Pawn"},
    {13456762188775529491ULL, "C39;KGA;Kieseritsky, Brentano defence, Kaplanek variation"},{13465896505310666702ULL, "D30;QGD"},{13476810165849712759ULL, "C46;Four knights;Italian variation"},{13481594338249219700ULL, "C44;Ponziani;Caro variation"},
    {13487694934723051797ULL, "A02;Bird's opening"},{13499264355845582023ULL, "C41;Philidor;Hanham, Kmoch variation"},{13501653547879823497ULL, "C57;two knights defence;Wilkes Barre (Traxler) variation"},{13509333608844950833ULL, "E51;Nimzo-Indian;4.e3, Ragozin variation"},
    {13518882385230362601ULL, "C02;French;advance, Wade variation"},{13527225930844158727ULL, "E27;Nimzo-Indian;Saemisch variation"},{13528990020591653228ULL, "C41;Philidor;Philidor counter-gambit"},{13541095530831368898ULL, "C41;Philidor;Nimzovich, Sozin variation"},
    {13544322802740964724ULL, "A21;English opening"},{13550748551895065244ULL, "A36;English;ultra-symmetrical variation"},{13561905476598469169ULL, "C41;Philidor;Improved Hanham variation"},{13576329777141948307ULL, "C37;KGA;Quaade gambit"},
    {13578347436102698953ULL, "A13;English opening"},{13580220346300330212ULL, "A29;English;four knights, kingside fianchetto"},{13596916437536463482ULL, "A81;Dutch;Leningrad, Karlsbad variation"},{13603281747814318693ULL, "D50;QGD;Semi-Tarrasch, Krause variation"},
    {13604708435980360040ULL, "A07;Reti;King's Indian attack, Pachman system"},{13606672410584213927ULL, "B02;Alekhine's defence;Spielmann variation"},{13612586775521638868ULL, "B13;Caro-Kann;Panov-Botvinnik, Gunderam attack"},{13639541573347515483ULL, "B72;Sicilian;dragon, classical, Nottingham variation"},
    {13639927443468212147ULL, "C15;French;Winawer, Alekhine gambit, Kan variation"},{13641354086896200002ULL, "E40;Nimzo-Indian;4.e3"},{13643269535674488506ULL, "D83;Gruenfeld;Gruenfeld gambit, Botvinnik variation"},{13652452506622667012ULL, "E60;King's Indian defence"},
    {13656704708134897434ULL, "C51;Evans gambit declined, Hirschbach variation"},{13660341552645529579ULL, "C23;Bishop's opening;Pratt variation"},{13667455902242012741ULL, "A00;Dunst (Sleipner, Heinrichsen) opening"},{13671509778171057213ULL, "C33;KGA;bishop's gambit, classical defence"},
    {13687793571932465560ULL, "C77;Ruy Lopez;Wormald attack, Gruenfeld variation"},{13688841305454170332ULL, "C44;Scotch opening"},{13693502907570052383ULL, "A13;English;Wimpey system"},{13695091133884366607ULL, "D77;Neo-Gruenfeld, 6.O-O"},
    {13710656965464730075ULL, "D63;QGD;Orthodox defence, 7.Rc1"},{13714208902744711522ULL, "B01;Scandinavian defence"},{13716052122413596095ULL, "D08;QGD;Albin counter-gambit, Lasker trap"},{13717963320483227011ULL, "E15;Queen's Indian;4.g3"},
    {13720509452377636901ULL, "C57;two knights defence;Fritz, Gruber variation"},{13746284750883351361ULL, "C51;Evans gambit declined, Showalter variation"},{13754755682274454009ULL, "B10;Caro-Kann;closed (Breyer) variation"},{13761473791656755272ULL, "C44;Scotch gambit"},
    {13761568455190230228ULL, "E88;King's Indian;Saemisch, orthodox, 7.d5 c6"},{13766943070169258537ULL, "A61;Benoni defence"},{13769047279266061849ULL, "C45;Scotch;Pulling counter-attack"},{13774723490369252310ULL, "B27;Sicilian defence"},
    {13783419172601305379ULL, "E92;King's Indian;Andersson variation"},{13800487806062585295ULL, "A09;Reti accepted;Keres variation"},{13817295057153189277ULL, "C15;French;Winawer, Kondratiyev variation"},{13819972258897181412ULL, "D00;Blackmar-Diemer;Euwe defence"},
    {13852734423407412272ULL, "D00;Levitsky attack (Queen's bishop attack)"},{13866326140149518598ULL, "C51;Evans gambit;Stone-Ware variation"},{13877183751985142215ULL, "B62;Sicilian;Richter-Rauzer, 6...e6"},{13883600369829090961ULL, "A84;Dutch defence"},
    {13887324324685303357ULL, "C53;Giuoco Piano"},{13915133016524721377ULL, "C42;Petrov;French attack"},{13930914726633540567ULL, "B12;Caro-Kann;advance, Short variation"},{13931953810975048950ULL, "B14;Caro-Kann;Panov-Botvinnik attack, 5...g6"},
    {13939451737794340566ULL, "C45;Scotch;Fraser attack"},{13958563874730998592ULL, "C61;Ruy Lopez;Bird's defence, Paulsen variation"},{13960529247401150130ULL, "E23;Nimzo-Indian;Spielmann, Staahlberg variation"},{13968346407936094175ULL, "D11;QGD Slav;Breyer variation"},
    {13969539956735016197ULL, "D60;QGD;Orthodox defence"},{13972682583179928882ULL, "C49;Four knights;symmetrical variation"},{13980171748444592463ULL, "C37;KGA;Salvio gambit, Anderssen counter-attack"},{13999374061742976227ULL, "B12;de Bruycker defence"},
    {14007205216878600946ULL, "D00;Blackmar gambit"},{14008135631616233561ULL, "C13;French;Albin-Alekhine-Chatard attack"},{14015889903186727719ULL, "E44;Nimzo-Indian;Fischer variation, 5.Ne2"},{14033775583792322797ULL, "C78;Ruy Lopez;Wing attack"},
    {14076288276950779513ULL, "D11;QGD Slav;3.Nf3"},{14085731545093530374ULL, "B33;Sicilian;Pelikan (Lasker/Sveshnikov) variation"},{14090210237421479419ULL, "D80;Gruenfeld defence"},{14093308303046909464ULL, "C65;Ruy Lopez;Berlin defence, Beverwijk variation"},
    {14109928206877399669ULL, "B74;Sicilian;dragon, classical, Spielmann variation"},{14112100055236233178ULL, "E10;Doery defence"},{14112207811613571040ULL, "C20;KP;Mengarini's opening"},{14150500697846413400ULL, "A42;Modern defence;Averbakh system, Randspringer variation"},
    {14165873102892291209ULL, "C73;Ruy Lopez;modern Steinitz defence, Alapin variation"},{14194432159013624737ULL, "A38;English;symmetrical, main line with b3"},{14194687831718233622ULL, "C67;Ruy Lopez;Berlin defence, Rosenthal variation"},{14195913748627687439ULL, "A40;Queen's pawn;Englund gambit"},
    {14198926798656068933ULL, "D44;QGD semi-Slav;Botvinnik system (anti-Meran)"},{14204551479057661689ULL, "C37;KGA;Muzio gambit, Kling and Horwitz counter-attack"},{14215208130209065955ULL, "C80;Ruy Lopez;open, 7.Bb3"},{14239585568481500082ULL, "C31;KGD;Nimzovich counter-gambit"},
    {14241348796734069728ULL, "C27;Vienna;`Frankenstein-Dracula' variation"},{14244309805386033725ULL, "E10;Blumenfeld counter-gambit"},{14250018083108907517ULL, "B58;Sicilian;Boleslavsky, Louma variation"},{14253157122122372384ULL, "B03;Alekhine's defence;four pawns attack, 6...Nc6"},
    {14264869764910417724ULL, "D55;QGD;Neo-orthodox variation, 7.Bxf6"},{14275808682173177656ULL, "C04;French;Tarrasch, Guimard main line"},{14288940252950480245ULL, "C38;KGA;Philidor gambit, Schultz variation"},{14300661530519909234ULL, "C19;French;Winawer, advance, Smyslov variation"},
    {14317310571807890487ULL, "A05;Reti opening"},{14324476892240894863ULL, "D27;QGA;classical, Rubinstein variation"},{14324734903396102720ULL, "A89;Dutch;Leningrad, main variation with Nc6"},{14327790633553279750ULL, "A49;King's Indian;fianchetto without c4"},
    {14333378069040301445ULL, "C23;Bishop's opening;Classical variation"},{14334959422148971317ULL, "E25;Nimzo-Indian;Saemisch, Romanovsky variation"},{14335130601602957302ULL, "E49;Nimzo-Indian;4.e3, Botvinnik system"},{14338172935910917814ULL, "C13;French;classical"},
    {14347587926942329134ULL, "C00;French;Alapin variation"},{14349971508957008306ULL, "B15;Caro-Kann;Alekhine gambit"},{14387876395645651009ULL, "C55;Giuoco piano;Rosentreter variation"},{14392440628058055360ULL, "C34;King's knight's gambit"},
    {14407710219603044848ULL, "A54;Old Indian;Ukrainian variation, 4.Nf3"},{14420626461446066783ULL, "A81;Dutch defence, Blackburne variation"},{14422465484595901330ULL, "C51;Evans gambit declined, Hicken variation"},{14468866809511659258ULL, "C68;Ruy Lopez;exchange variation"},
    {14469512087399020978ULL, "C13;French;Albin-Alekhine-Chatard attack, Spielmann variation"},{14475592869083927254ULL, "C70;Ruy Lopez;Caro variation"},{14493137427393433784ULL, "B23;Sicilian;Grand Prix attack"},{14499299322654541207ULL, "A51;Budapest;Fajarowicz, Steiner variation"},
    {14510127655146404740ULL, "B40;Sicilian defence"},{14518117052067852241ULL, "E29;Nimzo-Indian;Saemisch, Capablanca variation"},{14525503288304414368ULL, "B74;Sicilian;dragon, classical, 9.Nb3"},{14536367242597858611ULL, "D75;Neo-Gruenfeld, 6.cd Nxd5, 7.O-O c5, 8.dc"},
    {14539522135797355095ULL, "B15;Caro-Kann defence"},{14562399549841627035ULL, "A10;English opening"},{14580090494323153666ULL, "C17;French;Winawer, advance, 5.a3"},{14580456980461379310ULL, "D46;QGD semi-Slav;Bogolyubov variation"},
    {14582948683893976108ULL, "B09;Pirc;Austrian attack, 6.e5"},{14584943239902939444ULL, "B09;Pirc;Austrian attack"},{14588854353621223729ULL, "D97;Gruenfeld;Russian, Levenfish variation"},{14600096376707389425ULL, "A01;Nimzovich-Larsen attack"},
    {14604568267106270502ULL, "B42;Sicilian;Kan, Gipslis variation"},{14612254154063031697ULL, "E03;Catalan;open, 5.Qa4 Nbd7, 6.Qxc4"},{14615071739645611508ULL, "D73;Neo-Gruenfeld, 5.Nf3"},{14626790228392028394ULL, "C01;French;exchange, Svenonius variation"},
    {14629517878789701851ULL, "E21;Nimzo-Indian;three knights variation"},{14649994266338932728ULL, "C19;French;Winawer, advance, poisoned pawn variation"},{14650357458839736727ULL, "C60;Ruy Lopez;Brentano defence"},{14653816172613324967ULL, "C80;Ruy Lopez;open, Knorre variation"},
    {14663207741534693706ULL, "A27;English;three knights system"},{14666545815359835821ULL, "B15;Caro-Kann;Gurgenidze counter-attack"},{14667673565511265353ULL, "C26;Vienna game"},{14678692669500271753ULL, "A79;Benoni;classical, 11.f3"},
    {14720904078837762428ULL, "C30;KGD;classical, 4.c3"},{14750148238119164208ULL, "B18;Caro-Kann;classical, Flohr variation"},{14752720082630008106ULL, "B00;Lemming defence"},{14760489378658292538ULL, "D08;QGD;Albin counter-gambit, Balogh variation"},
    {14771690643096122586ULL, "A22;English;Carls' Bremen system"},{14776150720362844953ULL, "C24;Bishop's opening;Greco gambit"},{14787306864504671605ULL, "A04;Reti;Pirc-Lisitsin gambit"},{14792236518964500798ULL, "C70;Ruy Lopez;Taimanov (chase/wing/accelerated counterthrust) variation"},
    {14794328125498947833ULL, "C19;French;Winawer, advance, poisoned pawn, Konstantinopolsky variation"},{14797213354813370000ULL, "B01;Scandinavian;Pytel-Wade variation"},{14815960122034164064ULL, "D51;QGD;Manhattan variation"},{14824627536003452888ULL, "E70;King's Indian;accelerated Averbakh system"},
    {14871475915637586901ULL, "C41;Philidor;Nimzovich, Rellstab variation"},{14882020373143564522ULL, "A13;English opening;Agincourt variation"},{14882212263661047715ULL, "B12;Caro-Kann;Edinburgh variation"},{14884839758277456298ULL, "B86;Sicilian;Sozin attack"},
    {14893118225238363933ULL, "B31;Sicilian;Nimzovich-Rossolimo attack (with ...g6, without ...d6)"},{14894018947252371190ULL, "C66;Ruy Lopez;closed Berlin defence, Showalter variation"},{14895725391713937103ULL, "B20;Sicilian;wing gambit"},{14977533291948877560ULL, "E94;King's Indian;orthodox, Donner variation"},
    {14984042310050600888ULL, "C37;KGA;Cochrane gambit"},{14985721896518211740ULL, "B71;Sicilian;dragon, Levenfish variation"},{14985853353466418250ULL, "B94;Sicilian;Najdorf, 6.Bg5"},{14989166926028788183ULL, "A25;English;closed system"},
    {15006522086982748480ULL, "C29;Vienna gambit"},{15012292873623128151ULL, "C14;French;classical, Steinitz variation"},{15025437670393869613ULL, "E90;King's Indian;5.Nf3"},{15025465010277417279ULL, "D43;QGD semi-Slav;Hastings variation"},
    {15037228387531932043ULL, "C51;Evans gambit;Fraser-Mortimer attack"},{15046450172234851981ULL, "E14;Queen's Indian;Averbakh variation"},{15053419976288369119ULL, "D32;QGD;Tarrasch defence, 4.cd ed"},{15054594372511940727ULL, "C60;Ruy Lopez;Vinogradov variation"},
    {15076229889576640774ULL, "E23;Nimzo-Indian;Spielmann, Karlsbad variation"},{15076793835845199816ULL, "A80;Dutch, 2.Bg5 variation"},{15097928347798906336ULL, "E17;Queen's Indian;old main line, 6.O-O"},{15107659714621175145ULL, "D06;QGD;Marshall defence"},
    {15112839135522746521ULL, "A25;English;closed, Taimanov variation"},{15113413395593647635ULL, "E62;King's Indian;fianchetto, lesser Simagin (Spassky) variation"},{15123171606205893030ULL, "C46;Three knights;Schlechter variation"},{15126930300013749110ULL, "A59;Benko gambit;7.e4"},
    {15127961960427594647ULL, "B32;Sicilian defence"},{15169873847617355263ULL, "C45;Scotch;Tartakower variation"},{15170073161979813225ULL, "C39;KGA;Allgaier, Schlechter defence"},{15177748764215673026ULL, "A07;Reti;King's Indian attack (with ...c5)"},
    {15181677536074667986ULL, "C25;Vienna;Pierce gambit"},{15192974215584355311ULL, "E61;King's Indian;Smyslov system"},{15194872930414661462ULL, "D80;Gruenfeld;Lundin variation"},{15199329144574272151ULL, "C39;KGA;Allgaier, Walker attack"},
    {15213300192948443293ULL, "C40;King's knight opening"},{15222016355013874568ULL, "C67;Ruy Lopez;Berlin defence, open variation"},{15223734610017467017ULL, "C41;Philidor;exchange variation"},{15230911183082527995ULL, "D07;QGD;Chigorin defence, Janowski variation"},
    {15233094113651867914ULL, "A82;Dutch;Staunton gambit"},{15233099830111908174ULL, "C30;KGD;classical, Reti variation"},{15235760625082125905ULL, "C35;KGA;Cunningham defence"},{15257491278916051044ULL, "C52;Evans gambit;Leonhardt variation"},
    {15283214011470682992ULL, "C40;Greco defence"},{15294487223394868301ULL, "B27;Sicilian;Katalimov variation"},{15313333065893495872ULL, "D93;Gruenfeld with Bf4    e3"},{15314130182960902232ULL, "C33;KGA;bishop's gambit, Maurian defence"},
    {15319336333895927913ULL, "B10;Caro-Kann;Goldman (Spielmann) variation"},{15331590221438909288ULL, "C53;Giuoco Piano;Eisinger variation"},{15331858841336546701ULL, "C32;KGD;Falkbeer, Keres variation"},{15344753812617339671ULL, "C57;two knights defence;Ulvestad variation"},
    {15344967214281642529ULL, "B57;Sicilian;Sozin, Benko variation"},{15361568111002197568ULL, "D63;QGD;Orthodox defence, Capablanca variation"},{15370820587587100328ULL, "D31;QGD;Janowski variation"},{15379073794709689540ULL, "C02;French;advance variation"},
    {15380735101731651439ULL, "B12;Caro-Kann defence"},{15389073548427185745ULL, "C27;Vienna;Adams' gambit"},{15393416270148465454ULL, "B05;Alekhine's defence;modern, Alekhine variation"},{15406033910881256857ULL, "C40;QP counter-gambit;Maroczy gambit"},
    {15420957939226242710ULL, "C23;Bishop's opening;Four pawns' gambit"},{15431190861522067161ULL, "E43;Nimzo-Indian;Fischer variation"},{15434586014579001101ULL, "D22;QGA;Alekhine defence, Alatortsev variation"},{15436306254104902190ULL, "E30;Nimzo-Indian;Leningrad variation"},
    {15439809523623914539ULL, "C31;KGD;Falkbeer, 4.d3"},{15477795527173836041ULL, "C65;Ruy Lopez;Berlin defence, Anderssen variation"},{15500297121916679727ULL, "C44;Scotch;Goering gambit"},{15516322005118201814ULL, "A54;Old Indian;Dus-Khotimirsky variation"},
    {15522155390400046159ULL, "D33;QGD;Tarrasch, Prague variation"},{15532097781361400410ULL, "E52;Nimzo-Indian;4.e3, main line with ...b6"},{15532511949981647920ULL, "C02;French;advance, Euwe variation"},{15555725423216246323ULL, "B74;Sicilian;dragon, classical, Stockholm attack"},
    {15564088676378730047ULL, "A34;English;symmetrical variation"},{15569128088412623302ULL, "B09;Pirc;Austrian attack, 6.Be3"},{15569465161015409036ULL, "A18;English;Mikenas-Carls variation"},{15581509271640436977ULL, "D16;QGD Slav;Steiner variation"},
    {15607983498552265278ULL, "B03;Alekhine's defence;four pawns attack"},{15615259024392175313ULL, "C46;Four knights game"},{15621832290762619575ULL, "D23;Queen's gambit accepted"},{15626576654585669544ULL, "E62;King's Indian;fianchetto, Uhlmann (Szabo) variation"},
    {15627157773965977987ULL, "A46;Queen's pawn;Yusupov-Rubinstein system"},{15627646045920596040ULL, "A45;Queen's pawn game"},{15636072701952965984ULL, "C12;French;MacCutcheon, Bogolyubov variation"},{15641592630443719461ULL, "C25;Vienna;Hamppe-Allgaier gambit"},
    {15646520387755849209ULL, "D17;QGD Slav;Carlsbad variation"},{15664854805896979651ULL, "E32;Nimzo-Indian;classical, Adorjan gambit"},{15666648304563404373ULL, "C49;Four knights"},{15672155189265297668ULL, "C62;Ruy Lopez;old Steinitz defence, semi-Duras variation"},
    {15675380065046348718ULL, "B03;Alekhine's defence;O'Sullivan gambit"},{15678814394644751140ULL, "C14;French;classical, Alapin variation"},{15681748604783466722ULL, "B21;Sicilian;Andreaschek gambit"},{15687462012168339176ULL, "C48;Four knights;Rubinstein counter-gambit, exchange variation"},
    {15695800775901642752ULL, "B02;Alekhine's defence"},{15702133791734826099ULL, "C11;French;Steinitz, Gledhill attack"},{15703483604919365311ULL, "C33;KGA;Villemson (Steinitz) gambit"},{15742169634567669110ULL, "C33;KGA;bishop's gambit, McDonnell attack"},
    {15745091419995870096ULL, "C13;French;classical, Frankfurt variation"},{15751350905480316386ULL, "A00;Anti-Borg (Desprez) opening"},{15771529400917296766ULL, "A00;Hammerschlag (Fried fox/Pork chop opening)"},{15775949045825043995ULL, "D16;QGD Slav accepted;Alapin variation"},
    {15797792593348577626ULL, "C37;KGA;MacDonnell gambit"},{15804772648990046046ULL, "B56;Sicilian"},{15806601748439065652ULL, "C39;King's knight's gambit"},{15823188032075809836ULL, "C42;Petrov;classical attack, Mason variation"},
    {15831640518201433781ULL, "C37;KGA;Muzio gambit, Brentano defence"},{15840970635094339084ULL, "A87;Dutch;Leningrad, main variation"},{15847795084842953725ULL, "C44;Scotch gambit;Anderssen (Paulsen, Suhle) counter-attack"},{15865148858918698625ULL, "B06;Robatsch (modern) defence"},
    {15866838660325535373ULL, "A06;Tennison (Lemberg, Zukertort) gambit"},{15901429865163912822ULL, "C54;Giuoco Piano;Krause variation"},{15924661861315554740ULL, "C44;Ponziani counter-gambit, Schmidt attack"},{15924859471947038405ULL, "D02;Queen's pawn game"},
    {15931826275219189754ULL, "C56;Two knights defence"},{15938376687770432923ULL, "D00;Queen's pawn, Mason variation"},{15953914683148966388ULL, "A07;Reti;King's Indian attack"},{15980693101790558014ULL, "C40;Latvian;Polerio variation"},
    {15990333383884963287ULL, "E16;Queen's Indian;Yates variation"},{16000361326939620109ULL, "C00;French;Reversed Philidor formation"},{16038518597473563263ULL, "D46;QGD semi-Slav;6.Bd3"},{16042566468421529007ULL, "B83;Sicilian;Scheveningen, 6.Be2"},
    {16049450462512464761ULL, "A51;Budapest defence declined"},{16050964367712545467ULL, "C12;French;MacCutcheon, Lasker variation"},{16053660092422743798ULL, "B07;Pirc defence"},{16070459810519704933ULL, "A52;Budapest defence"},
    {16073994639516997140ULL, "C33;KGA;bishop's gambit, Bogolyubov variation"},{16103764203707286467ULL, "C63;Ruy Lopez;Schliemann defence"},{16119471516802655875ULL, "E07;Catalan;closed, Botvinnik variation"},{16147987561512660194ULL, "A22;English opening"},
    {16163576424076022129ULL, "E37;Nimzo-Indian;classical, San Remo variation"},{16173056704241825790ULL, "C36;KGA;Abbazia defence, Botvinnik variation"},{16188347182616638024ULL, "C25;Vienna;Hamppe-Allgaier gambit, Alapin variation"},{16201995543958234107ULL, "C51;Evans gambit;Cordel variation"},
    {16220212674983522515ULL, "C00;French;Schlechter variation"},{16236031573145634416ULL, "B90;Sicilian;Najdorf, Adams attack"},{16273264397366078729ULL, "D32;QGD;Tarrasch defence"},{16276225291237571616ULL, "C58;two knights defence"},
    {16278738230048593278ULL, "D30;QGD;Stonewall variation"},{16290688973484196265ULL, "D40;QGD;Semi-Tarrasch, Levenfish variation"},{16297884575082582932ULL, "C53;Giuoco Piano;Tarrasch variation"},{16305785612785471931ULL, "E37;Nimzo-Indian;classical, Noa variation, main line, 7.Qc2"},
    {16312378629039253714ULL, "A13;English opening;Agincourt variation"},{16343671809801061916ULL, "B01;Scandinavian;Anderssen counter-attack orthodox attack"},{16351056570317816538ULL, "E47;Nimzo-Indian;4.e3 O-O, 5.Bd3"},{16360121313223077091ULL, "A40;Polish defence"},
    {16362178584101314018ULL, "E71;King's Indian;Makagonov system (5.h3)"},{16363731503156365500ULL, "B06;Robatsch defence;three pawns attack"},{16366568488043939356ULL, "D21;QGA;Ericson variation"},{16376651398350071794ULL, "B02;Alekhine's defence;Brooklyn defence"},
    {16388791520166861233ULL, "B00;Guatemala defence"},{16389769482510037951ULL, "C45;Scotch;Paulsen attack"},{16395190901586387288ULL, "B03;Alekhine's defence;Balogh variation"},{16395837369898107994ULL, "C00;St. George defence"},
    {16399239767114493089ULL, "C33;KGA;bishop's gambit, Morphy variation"},{16400745562692590642ULL, "C42;Petrov;Kaufmann attack"},{16410990949403564118ULL, "A34;English;symmetrical, Rubinstein system"},{16433175165828840019ULL, "A21;English, Troeger defence"},
    {16478669565216631501ULL, "B53;Sicilian;Chekhover, Zaitsev variation"},{16482436390890003238ULL, "C48;Four knights;Rubinstein counter-gambit, Bogolyubov variation"},{16485306579680913984ULL, "B29;Sicilian;Nimzovich-Rubinstein variation"},{16496324126320193749ULL, "E00;Neo-Indian (Seirawan) attack"},
    {16497997127310859526ULL, "C45;Scotch;Romanishin variation"},{16513834430923781629ULL, "A83;Dutch;Staunton gambit, Chigorin variation"},{16534569997232771457ULL, "A00;Grob's attack"},{16543624366772107244ULL, "B06;Robatsch defence"},
    {16546013516754937537ULL, "C37;KGA;Ghulam Kassim gambit"},{16567905108507745864ULL, "D13;QGD Slav;exchange variation"},{16569599364963277940ULL, "C42;Petrov;classical attack, Tarrasch variation"},{16569675981290533643ULL, "C64;Ruy Lopez;classical defence, Zaitsev variation"},
    {16577243330810021022ULL, "C64;Ruy Lopez;classical (Cordel) defence"},{16579558963243030002ULL, "B41;Sicilian;Kan variation"},{16589109944084289686ULL, "C37;KGA;Muzio gambit, Paulsen variation"},{16612785005282348704ULL, "A90;Dutch defence;Dutch-Indian (Nimzo-Dutch) variation"},
    {16621492633903823409ULL, "A02;Bird;From gambit, Lipke variation"},{16631157007222637971ULL, "A43;Old Benoni defence"},{16644850495914937112ULL, "A38;English;symmetrical, main line with d3"},{16650180236467923634ULL, "B94;Sicilian;Najdorf, Ivkov variation"},
    {16658206088295500505ULL, "E91;King's Indian;Kazakh variation"},{16667765233196466468ULL, "C33;KGA;Keres (Mason-Steinitz) gambit"},{16677035067018905255ULL, "E11;Bogo-Indian defence, Monticelli trap"},{16683487961157282048ULL, "C74;Ruy Lopez;modern Steinitz defence"},
    {16697943921311211071ULL, "C16;French;Winawer, advance variation"},{16709441572543569323ULL, "C45;Scotch;Berger variation"},{16729346930918564011ULL, "E62;King's Indian;fianchetto, Larsen system"},{16732059487369459651ULL, "C77;Ruy Lopez;Morphy defence, Duras variation"},
    {16737700165225500881ULL, "E33;Nimzo-Indian;classical, Milner-Barry (Zurich) variation"},{16745143123628333451ULL, "D00;Blackmar-Diemer;Lemberg counter-gambit"},{16746618322167752261ULL, "A40;Queen's pawn;English defence"},{16751226203629813380ULL, "C02;French;advance, Nimzovich system"},
    {16757303323964229223ULL, "B63;Sicilian;Richter-Rauzer, Rauzer attack"},{16762210678428747592ULL, "A00;Mieses opening"},{16779301225419786836ULL, "C41;Philidor;Nimzovich variation"},{16813575956237317645ULL, "B00;Owen defence"},
    {16825675397422311745ULL, "C55;two knights;Max Lange attack, Schlechter variation"},{16852781516583226669ULL, "E15;Queen's Indian;Rubinstein variation"},{16855267038324502450ULL, "E51;Nimzo-Indian;4.e3 e8g8, 5.Nf3 d7d5"},{16864068514529676141ULL, "C32;KGD;Falkbeer, main line, 7...Bf5"},
    {16875043787478098750ULL, "A48;King's Indian;East Indian defence"},{16878683419499065975ULL, "C40;Latvian;Behting variation"},{16880949199975003434ULL, "A92;Dutch;stonewall with Nc3"},{16886828474907542558ULL, "A90;Dutch-Indian, Alekhine variation"},
    {16898976983165164374ULL, "C38;KGA;Hanstein gambit"},{16902459632345936811ULL, "C45;Scotch;Mieses variation"},{16905080946651992560ULL, "C41;Philidor's defence"},{16914685226131339232ULL, "B04;Alekhine's defence;modern, Keres variation"},
    {16919298507111244485ULL, "A92;Dutch defence"},{16923476277282211943ULL, "A82;Dutch;Balogh defence"},{16934667339903891589ULL, "C33;KGA;bishop's gambit, Lopez-Gianutio counter-gambit"},{16935190095000034100ULL, "C53;Giuoco Piano;LaBourdonnais variation"},
    {16939960875862619192ULL, "A00;Van't Kruijs opening"},{16946424506071570785ULL, "C35;KGA;Cunningham, Euwe defence"},{16951503723751990337ULL, "C41;Philidor;Steinitz variation"},{16963470959540383058ULL, "C48;Four knights;Spanish variation"},
    {16968999559129419693ULL, "C42;Petrov;Italian variation"},{16976818512577032860ULL, "D26;QGA;classical, Furman variation"},{16992483941090005236ULL, "B10;Caro-Kann defence"},{17009984879765634148ULL, "C45;Scotch game"},
    {17013105573636412800ULL, "B08;Pirc;classical (two knights) system"},{17026649746379558569ULL, "A86;Dutch with c4 & g3"},{17063826403342389432ULL, "A00;Polish (Sokolsky) opening"},{17069887342225508823ULL, "B00;KP;Nimzovich defence"},
    {17082446679990057879ULL, "A37;English;symmetrical, Botvinnik system reversed"},{17082590228162119392ULL, "C33;KGA;bishop's gambit, Ruy Lopez defence"},{17092763943920584533ULL, "A84;Dutch defence"},{17112232442528471696ULL, "C03;French;Tarrasch"},
    {17133840110987964477ULL, "A69;Benoni;four pawns attack, main line"},{17137440317529098583ULL, "E79;King's Indian;Four pawns attack, main line"},{17139093406942567036ULL, "D14;QGD Slav;exchange variation, 6.Bf4 Bf5"},{17145371064246576698ULL, "B83;Sicilian;modern Scheveningen, main line with Nb3"},
    {17152128568111060596ULL, "B01;Scandinavian;Anderssen counter-attack"},{17161955898996176188ULL, "C02;French;advance variation"},{17162298682496855604ULL, "C57;two knights defence;Lolli attack"},{17164594797838225786ULL, "C55;Two knights defence (Modern bishop's opening)"},
    {17178508056212026544ULL, "D55;QGD;Petrosian variation"},{17188834381637584051ULL, "B00;Fried fox defence"},{17224869444641282802ULL, "A16;English;Anglo-Gruenfeld defense"},{17233063852095765910ULL, "C72;Ruy Lopez;modern Steinitz defence, 5.O-O"},
    {17234276086194062104ULL, "A00;Global opening"},{17238114595344564525ULL, "D30;QGD;Capablanca variation"},{17240235422821638431ULL, "C44;Scotch;Goering gambit, Bardeleben variation"},{17248478247032298921ULL, "D98;Gruenfeld;Russian, Keres variation"},
    {17250063146268628862ULL, "B32;Sicilian;Flohr variation"},{17261717764797898212ULL, "E36;Nimzo-Indian;classical, Noa variation, 5.a3"},{17266773746278801942ULL, "D97;Gruenfeld;Russian, Prins variation"},{17279736255030498072ULL, "A16;English;Anglo-Gruenfeld defense, Korchnoi variation"},
    {17301992993850013884ULL, "B02;Alekhine's defence;Welling variation"},{17312720528227493119ULL, "C43;Petrov;modern attack, main line"},{17315261855013671770ULL, "C43;Petrov;modern attack, Symmetrical variation"},{17317383377016724348ULL, "A83;Dutch;Staunton gambit, Lasker variation"},
    {17336881118833080534ULL, "B02;Alekhine's defence"},{17343636609963591001ULL, "D30;QGD Slav;Semmering variation"},{17366553912307996501ULL, "C39;KGA;Allgaier, Cook variation"},{17367072025214918422ULL, "D69;QGD;Orthodox defence, classical, 13.de"},
    {17376024951504351093ULL, "B07;Pirc;Byrne variation"},{17393118791544350934ULL, "E06;Catalan;closed, 5.Nf3"},{17445160558775370909ULL, "C10;French;Frere (Becker) variation"},{17455597899478470857ULL, "D55;QGD;6.Nf3"},
    {17467901442211497924ULL, "C30;KGD;Norwalde variation"},{17482868290185349094ULL, "A13;English;Neo-Catalan accepted"},{17489077436777137576ULL, "B00;KP;Nimzovich defence"},{17490057229932069554ULL, "D17;QGD Slav;Wiesbaden variation"},
    {17495918134126296051ULL, "C23;Bishop's opening;del Rio variation"},{17503111898632471667ULL, "C10;French;Rubinstein, Capablanca line"},{17512807785136074365ULL, "C77;Ruy Lopez;Morphy defence"},{17525593936776717666ULL, "D30;QGD;Spielmann variation"},
    {17531238699536288324ULL, "B62;Sicilian;Richter-Rauzer, Margate (Alekhine) variation"},{17569540196579818361ULL, "C25;Vienna game"},{17603279437685838276ULL, "C00;French defence"},{17631706836159507214ULL, "C46;Three knights;Steinitz, Rosenthal variation"},
    {17632815208091988491ULL, "B93;Sicilian;Najdorf, 6.f4"},{17643772029103177522ULL, "D18;QGD Slav;Dutch variation"},{17649105744169850521ULL, "C44;Dresden opening"},{17649315393160615169ULL, "A43;Old Benoni;Schmid's system"},
    {17656920697148513383ULL, "C32;KGD;Falkbeer, Charousek variation"},{17666561881319670738ULL, "C24;Bishop's opening;Ponziani gambit"},{17677453271937418281ULL, "C30;King's gambit"},{17688252038015238540ULL, "A40;Queen's pawn"},
    {17695067478655249473ULL, "A03;Bird's opening;Lasker variation"},{17702460145048434292ULL, "D10;QGD Slav defence, Alekhine variation"},{17725859323694186300ULL, "C80;Ruy Lopez;open, 6.d4"},{17748077147255361600ULL, "E99;King's Indian;orthodox, Aronin-Taimanov, Benko attack"},
    {17771601837819489754ULL, "C49;Four knights;symmetrical, Metger unpin"},{17772666520632705486ULL, "C52;Evans gambit;Alapin-Steinitz variation"},{17791120801704203087ULL, "E55;Nimzo-Indian;4.e3, Gligoric system, Bronstein variation"},{17791944332441014363ULL, "C41;Philidor;Lopez counter-gambit, Jaenisch variation"},
    {17794262948643906499ULL, "C42;Petrov;classical attack, Jaenisch variation"},{17795835425796794777ULL, "E76;King's Indian;Four pawns attack"},{17815189175251274773ULL, "A45;Blackmar-Diemer gambit"},{17858447672487539011ULL, "B55;Sicilian;Prins variation, Venice attack"},
    {17887254546191184695ULL, "C74;Ruy Lopez;Siesta, Kopayev variation"},{17893477135797087327ULL, "C49;Four knights;double Ruy Lopez"},{17916079855471771955ULL, "C44;Scotch gambit;Hanneken variation"},{17920586671336800156ULL, "C45;Scotch;Blackburne attack"},
    {17936931103873470308ULL, "A12;English;Bled variation"},{17957108133876916726ULL, "B52;Sicilian;Canal-Sokolsky attack, Sokolsky variation"},{17974948268386274589ULL, "D81;Gruenfeld;Russian variation"},{17982251938526772413ULL, "C23;Bishop's opening;Philidor counter-attack"},
    {17995881873847413937ULL, "C42;Petrov;Cozio (Lasker) attack"},{18000901611687889440ULL, "E10;Queen's pawn game"},{18010885306056010109ULL, "C34;KGA;Bonsch-Osmolovsky variation"},{18014753333679299453ULL, "C33;KGA;bishop's gambit, Bledow variation"},
    {18025016513345994651ULL, "B17;Caro-Kann;Steinitz variation"},{18025734818440088432ULL, "C16;French;Winawer, Petrosian variation"},{18032628727821906489ULL, "A03;Bird's opening;Williams gambit"},{18036392020610273589ULL, "E26;Nimzo-Indian;Saemisch variation"},
    {18096700440621831651ULL, "D60;QGD;Orthodox defence, Rauzer variation"},{18099370244471965036ULL, "A28;English;four knights, Stean variation"},{18111504978769103085ULL, "B34;Sicilian;accelerated fianchetto, modern variation"},{18119074494043829312ULL, "C59;two knights defence;Steinitz variation"},
    {18145111275632565304ULL, "C42;Petrov;Cochrane gambit"},{18150090417059983493ULL, "A42;Modern defence;Averbakh system, Kotov variation"},{18164286095627007312ULL, "B71;Sicilian;dragon, Levenfish; Flohr variation"},{18166612001575558929ULL, "C12;French;MacCutcheon, Chigorin variation"},
    {18172295748331349165ULL, "D30;Queen's gambit declined"},{18174880444938282061ULL, "C55;two knights;Max Lange attack, Berger variation"},{18176973918386514678ULL, "B63;Sicilian;Richter-Rauzer, Rauzer attack, 7...Be7"},{18178982025881061427ULL, "A00;Battambang opening"},
    {18188999142774366532ULL, "C13;French;classical, Vistaneckis (Nimzovich) variation"},{18198719400938357795ULL, "E77;King's Indian;Six pawns attack"},{18201791376041335703ULL, "A35;English;symmetrical, four knights system"},{18271895080136158965ULL, "C37;KGA;Salvio gambit"},
    {18296940419720754144ULL, "B25;Sicilian;closed, 6.Ne2 e5 (Botvinnik)"},{18297507625271513832ULL, "B41;Sicilian;Kan, Maroczy bind - Bronstein variation"},{18299113765639953697ULL, "D00;Queen's pawn;Chigorin variation"},{18304509183597521837ULL, "C34;KGA;Fischer defence"},
    {18308333268416881448ULL, "B07;Pirc;150 attack"},{18308748558190168123ULL, "B09;Pirc;Austrian attack, 6.Bd3"},{18319740228938719182ULL, "C75;Ruy Lopez;modern Steinitz defence, Rubinstein variation"},{18331473956482556880ULL, "C42;Petrov;classical attack, Chigorin variation"},
    {18379489022013167378ULL, "A12;English;Torre defensive system"},{18383774175722148811ULL, "D00;Queen's pawn;Anti-Veresov"},{18387856988164628044ULL, "A50;Kevitz-Trajkovich defence"},{18392926954650495337ULL, "D86;Gruenfeld;exchange, Simagin's lesser variation"},
    {18400753095161218296ULL, "A88;Dutch;Leningrad, main variation with c6"},{18415784667315910130ULL, "A80;Dutch, Korchnoi attack"},{18417542882991751896ULL, "C23;Bishop's opening;Calabrese counter-gambit"},{18424396128252022114ULL, "C37;KGA;Lolli gambit, Young variation"}
};

std::string ChessBoard::getLastEcoString() const
{
    std::string ecoString;
    for(auto && hist : histList) {
        auto it = ecoMap.find(hist.hashKey);
        if (it != ecoMap.end()) {
            auto vec = Funcs::splitString(it->second, ';');
            if (!vec.empty()) {
                ecoString = vec.front();
            }
        }
    }
    return ecoString;
}


//////////////////////
uint64_t ChessBoard::_posToBitboard[64];

static const int toSFPos[] {
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
};

static const int fromSFPos[] {
    pos_a1, pos_b1, pos_c1, pos_d1, pos_e1, pos_f1, pos_g1, pos_h1,
    pos_a2, pos_b2, pos_c2, pos_d2, pos_e2, pos_f2, pos_g2, pos_h2,
    pos_a3, pos_b3, pos_c3, pos_d3, pos_e3, pos_f3, pos_g3, pos_h3,
    pos_a4, pos_b4, pos_c4, pos_d4, pos_e4, pos_f4, pos_g4, pos_h4,
    pos_a5, pos_b5, pos_c5, pos_d5, pos_e5, pos_f5, pos_g5, pos_h5,
    pos_a6, pos_b6, pos_c6, pos_d6, pos_e6, pos_f6, pos_g6, pos_h6,
    pos_a7, pos_b7, pos_c7, pos_d7, pos_e7, pos_f7, pos_g7, pos_h7,
    pos_a8, pos_b8, pos_c8, pos_d8, pos_e8, pos_f8, pos_g8, pos_h8
};

// for encoding/decoding a bishop move into one byte
static std::vector<int> bishopStartPointsVec;

void ChessBoard::staticInit()
{
    for(int i = 0; i < 64; ++i) {
        _posToBitboard[i] = 1ULL << toSFPos[i];
        
        bishopStartPointsVec.push_back(0);
        
//        auto sfpos = toSFPos[i]; assert(sfpos >= 0 && sfpos < 64);
//        auto pos = fromSFPos[sfpos]; assert(pos >= 0 && pos < 64);
//        assert(pos == i);
    }
    
    // for encoding/decoding a bishop move into one byte
    for(auto pos = 0; pos < 8; ++pos) {
        // down right from the top row (row 8)
        for(auto i = 0; i < 8 - pos; i++) {
            auto k = pos + i * 9; assert(k >= 0 && k < 64);
            bishopStartPointsVec[k] |= pos;
        }

        // up right from the bottm row (1)
        for(auto i = 0; ; i++) {
            auto k = 56 + pos - i * 7; // last row
            bishopStartPointsVec[k] |= (56 + pos) << 8;
            if (k % 8 >= 7) break;
        }
    }

    // from column a
    for(auto pos = 8; pos < 64; pos += 8) {
        // down right from column a
        for(auto i = 0; ; i++) {
            auto k = pos + i * 9;
            if (k >= 64) break;
            bishopStartPointsVec[k] |= pos;
        }
        
        // up right from column a
        for(auto i = 0; ; i++) {
            auto k = pos - i * 7;
            if (k < 0) break;
            bishopStartPointsVec[k] |= pos << 8;
        }
    }
}

// "e2", "a7"
uint64_t ChessBoard::posToBitboard(const char* s)
{
    auto c = s[0], d = s[1];
    if (c >= 'a' && c <= 'h' && d >= '1' && d <= '8') {
        auto pos = static_cast<int>('8' - d) * 8 + static_cast<int>(c - 'a');
        assert(pos >= 0 && pos < 64);
        return _posToBitboard[pos];
    }
    
    return 0;
}

std::vector<uint64_t> ChessBoard::posToBitboards() const
{
    std::vector<uint64_t> vec = {   hashKey, 0ULL, 0ULL, 0ULL, 0ULL,
                                    0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
                                    0ULL, 0ULL

    };
    assert(vec.size() == static_cast<int>(BBIdx::max));
    
    for (int i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            continue;
        }
        auto k = _posToBitboard[i];
        auto sd = static_cast<int>(piece.side); assert(sd == 0 || sd == 1);
        
        vec[static_cast<int>(BBIdx::kings) + piece.type - 1] |= k;
        vec[static_cast<int>(BBIdx::black) + sd] |= k;
        
        if (piece.type == static_cast<int>(PieceTypeStd::king)) {
            vec[static_cast<int>(BBIdx::blackkingsquare) + sd] = k;
        }
    }

    int64_t rights0 = castleRights[0], rights1 = castleRights[1];
    vec[static_cast<int>(BBIdx::prop)] = (enpassant & 0xff) | rights0 << 8 | rights1 << 10;
    assert(vec[static_cast<int>(BBIdx::hash)] && vec[static_cast<int>(BBIdx::black)] && vec[static_cast<int>(BBIdx::white)]); // two sides and king must be not zero
    assert(vec[static_cast<int>(BBIdx::blackkingsquare)] != vec[static_cast<int>(BBIdx::whitekingsquare)]);
    return vec;
}


std::string ChessBoard::bitboard2string(uint64_t bb)
{
    std::string s;
    uint64_t k = 1;
    for(int i = 0; i < 64; i++, k <<= 1) {
        s += (bb & k) != 0 ? "1 " : ". ";
        if (i && (i & 7) == 7) {
            s += "\n";
        }
    }
    
    s += "bb: " + std::to_string(bb) + "\n";
    return s;
}


// Our: empty, king, queen, rook, bishop, knight, pawn
// SF : NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
uint16_t ChessBoard::encode2Bytes(Move move)
{
    return toSFPos[move.from] | toSFPos[move.dest] << 6
        | (move.promotion > 0 ? ((PAWNSTD + 1 - move.promotion) << 12) : 0);
}

Move ChessBoard::decode2Bytes(uint16_t d)
{
    Move m;
    m.from = fromSFPos[d & 0x3f];
    m.dest = fromSFPos[(d >> 6) & 0x3f];
    m.promotion = d >> 12;
    if (m.promotion > 0) {
        m.promotion = PAWNSTD + 1 - m.promotion;
    }
    
    assert(m.isValid());
    assert(m.promotion == EMPTY || (m.promotion > KING && m.promotion < PAWNSTD));
    return m;
}

static const std::unordered_map<int, int> knightMoveIndexMap {
    {-17, 0}, {-15, 1}, {-10, 2}, {-6,  3},
    {6,   4}, {10,  5}, {15,  6}, {17,  7},
};

static const std::vector<int> knightMoveIndexVec{ -17, -15, -10, -6, 6, 10, 15, 17};

static const std::unordered_map<int, int> kingMoveIndexMap {
    {-9, 0}, {-8, 1}, {-7, 2}, {-2, 3}, {-1, 4},  // +- 2 is castling
    {1,  5}, {2,  6}, {7,  7}, {8,  8}, {9,  9},
};
static const std::vector<int> kingMoveIndexVec{ -9, -8, -7, -2, -1, 1, 2, 7, 8, 9};


static const std::unordered_map<int, int> pawnMoveIndexMap {
    {7, 0}, {8, 1}, {9, 2}, {16,  3},
};

static const std::vector<int> pawnMoveIndexVec{ 7, 8, 9, 16};

std::pair<uint16_t, int> ChessBoard::encode1Byte(MoveFull move)
{
    uint16_t t = move.piece.idx;
    assert(t >= 0 && t < 16);
    
    std::pair<uint16_t, int> pair;
    
    auto type = static_cast<PieceTypeStd>(move.piece.type);
    // Queen move takes 2 byte
    if (type == PieceTypeStd::queen) {
        t |= static_cast<int16_t>(move.dest << 4);
        pair.second = 2;
        
        assert((t >> 4) < 64);
    } else {
        switch (type) {
            case PieceTypeStd::king:
            {
                auto d = move.dest - move.from;
                auto it = kingMoveIndexMap.find(d);
                if (it != kingMoveIndexMap.end()) {
                    t |= it->second << 4;
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::knight:
            {
                auto d = move.dest - move.from;
                auto it = knightMoveIndexMap.find(d);
                if (it != knightMoveIndexMap.end()) {
                    t |= it->second << 4;
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::pawn:
            {
                auto d = std::abs(move.dest - move.from);
                auto it = pawnMoveIndexMap.find(d);
                if (it != pawnMoveIndexMap.end()) {
                    t |= it->second << 4;
                    if (move.promotion >= static_cast<int>(PieceTypeStd::queen)) {
                        auto promotion = move.promotion - static_cast<int>(PieceTypeStd::queen);
                        t |= promotion << 6;
                    }
                } else {
                    assert(false);
                }
                break;
            }
            case PieceTypeStd::rook:
            {
                if (move.from / 8 == move.dest / 8) { // same row
                    t |= (move.dest % 8) << 5;
                } else {
                    t |= 1 << 4 | (move.dest / 8) << 5;
                }
                break;
            }
            case PieceTypeStd::bishop:
            {
                auto sf = bishopStartPointsVec.at(move.from);
                auto sd = bishopStartPointsVec.at(move.dest);
                
                if ((sf & 0xff) == (sd & 0xff)) {
                    assert((sf >> 8) != (sd >> 8));
                    auto s = sd & 0xff; assert(s < 64);
                    assert(move.dest >= s && (move.dest - s) % 9 == 0);
                    auto k = (move.dest - s) / 9; assert(k >= 0 && k < 8);
                    t |= k << 5;
                } else {
                    assert((sf >> 8) == (sd >> 8));
                    auto s = sd >> 8; assert(s < 64);
                    assert(move.dest <= s && (move.dest - s) % 7 == 0);
                    auto k = (s - move.dest) / 7; assert(k >= 0 && k < 8);
                    t |= 1 << 4 | k << 5;

                }
                break;
            }

            default:
                break;
        }
        pair.second = 1;
    }
    
    pair.first = t;
    assert(pair.second == 1 || pair.second == 2);
    return pair;
}

std::pair<Move, int> ChessBoard::decode1Byte(const int8_t* d)
{
    // first 4 bits is the index of the piece
    uint8_t dd = *d;
    auto idx = static_cast<int>(dd & 0xf);
    
    std::pair<Move, int> pair;
    int from;
    
    for(from = 0; from < 64; ++from) {
        if (pieces[from].idx == idx && pieces[from].side == side) {
            break;
        }
    }
    
    if (from >= 64) {
        std::cout << "Error on decode1Byte. Stop!" << std::endl;
        return pair; // something wrong
    }
    
    auto n = 1;
    Move m;
    m.promotion = 0;

    auto piece = pieces[from];
    m.from = from;
    
    auto type = static_cast<PieceTypeStd>(piece.type);
    switch (type) {
        case PieceTypeStd::king:
        {
            auto k = (dd >> 4) & 0xf;
            m.dest = from + kingMoveIndexVec.at(k);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::knight:
        {
            auto k = (dd >> 4) & 0xf;
            m.dest = from + knightMoveIndexVec.at(k);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::pawn:
        {
            auto k = (dd >> 4) & 0x3;
            auto x = pawnMoveIndexVec.at(k);
            m.dest = from + (piece.side == Side::white ? -x : x);
            assert(BoardCore::isValid(m));
            
            if (m.dest < 8 || m.dest >= 56) {
                m.promotion = ((dd >> 6) & 0x3) + static_cast<int>(PieceTypeStd::queen);
            }
            break;
        }
        case PieceTypeStd::queen:
        {
            auto q = reinterpret_cast<const uint16_t*>(d);
            auto qq = *q;
            assert((qq & 0xff) == dd);
            m.dest = qq >> 4;
            n = 2;
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::rook:
        {
            auto k = dd >> 5; assert(k < 8);
            if (dd & (1 << 4)) { // same column
                m.dest = k * 8 + (m.from % 8);
                assert(m.from % 8 == m.dest % 8);
            } else {
                int r = m.from / 8;
                m.dest = r * 8 + k;
                assert(m.from / 8 == m.dest / 8);
            }
            assert(m.from / 8 == m.dest / 8 || m.from % 8 == m.dest % 8);
            assert(BoardCore::isValid(m));
            break;
        }
        case PieceTypeStd::bishop:
        {
            auto sf = bishopStartPointsVec.at(m.from);
            auto k = dd >> 5;
            if (dd & (1 << 4)) {
                auto s = sf >> 8; assert(s < 64);
                m.dest = s - k * 7;
                assert(BoardCore::isValid(m));
            } else {
                auto s = sf & 0xff; assert(s < 64);
                m.dest = s + k * 9;
                assert(BoardCore::isValid(m));
            }
            break;
        }

        default:
            break;
    }
    
    pair.first = m;
    pair.second = n;
    assert(BoardCore::isValid(m));
    return pair;
}

