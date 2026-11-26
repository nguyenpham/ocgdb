/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "base/rng.h"
#include <random>
#include <iomanip> // for setprecision
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "chess.h"




#define make_move(promote, from, to) ((((promote) & 0x7) << 12) | (((from) & 0x3F) << 6) | ((to) & 0x3F))
#define move_from(move) (((move) >> 6) & 0x3F)
#define move_to(move) ((move) & 0x3F)
#define move_promotes(move) (((move) >> 12) & 0x7)

using namespace banksia;

extern const int pieceListStartIdxByType[7];
const int pieceListStartIdxByType[7] = { -1, 0, 1, 2, 4, 6, 8 };

extern const PieceType pieceListIdxToType[16];

const PieceType pieceListIdxToType[16]
        = { PieceType::king,   PieceType::queen,  PieceType::rook,   PieceType::rook,
            PieceType::bishop, PieceType::bishop, PieceType::knight, PieceType::knight,
            PieceType::pawn,   PieceType::pawn,   PieceType::pawn,   PieceType::pawn,
            PieceType::pawn,   PieceType::pawn,   PieceType::pawn,   PieceType::pawn };


/// We use Polyglot ramdom numbers to support directly Polyglot books without using extra space nor converting
extern u64 polyglotRandom64[800];
extern u64* RandomCastle;

u64* RandomPiece = polyglotRandom64;
u64* RandomCastle = polyglotRandom64 + 768;
u64* RandomEnPassant = polyglotRandom64 + 772;
u64* RandomTurn = polyglotRandom64 + 780;

int createBoardCnt = 0, deleteBoardCnt = 0;

ChessBoard::ChessBoard(ChessVariant _variant)
{
    variant = _variant;
    assert(isChessFamily(variant));

    pieces.clear();
    for (auto i = 0; i < 64; i++) {
        pieces.push_back(Piece::emptyPiece);
    }

    createBoardCnt++;
}

ChessBoard::ChessBoard(const ChessBoard& other)
{
    _clone(&other);
    createBoardCnt++;
}

ChessBoard::~ChessBoard()
{
    deleteBoardCnt++;
}


int ChessBoard::coordinateStringToPos(const std::string& str, const MoveFormat&) const
{
    return chessCoordinateStringToPos(str);
}

std::string ChessBoard::posToCoordinateString(int pos, const MoveFormat&) const
{
    return chessPosToCoordinateString(pos);
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
        if (((castleRights[B] & CastleRight_long)
             && !_isPiece(castleRights_column_rook_left, static_cast<int>(PieceTypeStd::rook), Side::black))
            || ((castleRights[B] & CastleRight_short)
                && !_isPiece(castleRights_column_rook_right, static_cast<int>(PieceTypeStd::rook),
                             Side::black))) {
            return false;
        }
    }

    if (castleRights[W]) {
        if (!_isPiece(56 + castleRights_column_king, KING, Side::white)) {
            return false;
        }
        if (((castleRights[W] & CastleRight_long)
             && !_isPiece(56 + castleRights_column_rook_left, static_cast<int>(PieceTypeStd::rook),
                          Side::white))
            || ((castleRights[W] & CastleRight_short)
                && !_isPiece(56 + castleRights_column_rook_right, static_cast<int>(PieceTypeStd::rook),
                             Side::white))) {
            return false;
        }
    }
    return true;
}

bool ChessBoard::isValid() const
{
    auto cnt = 0;

    int pieceCnt[2][7] = { { 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0 } };

    for (auto i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) {
            continue;
        }

        cnt++;
        pieceCnt[static_cast<int>(piece.side)][static_cast<int>(piece.type)] += 1;
        if (piece.type == PAWNSTD && (i < 8 || i >= 56)) {
            return false;
        }
    }

    if (cnt < 2 || (castleRights[0] + castleRights[1] && !isValidCastleRights())) {
        return false;
    }

    if (enpassant > 0) {
        auto row = getRow(enpassant);
        if (row != 2 && row != 5) {
            return false;
        }

        auto pawnPos = row == 2 ? (enpassant + 8) : (enpassant - 8);
        if (!_isPiece(pawnPos, PAWNSTD, row == 2 ? Side::black : Side::white)) {
            return false;
        }
    }

    bool b = pieceCnt[0][1] == 1 && pieceCnt[1][1] == 1 && // king
             pieceCnt[0][2] <= 9 && pieceCnt[1][2] <= 9 && // queen
             pieceCnt[0][3] <= 10 && pieceCnt[1][3] <= 10 && // rook
             pieceCnt[0][4] <= 10 && pieceCnt[1][4] <= 10 && // bishop
             pieceCnt[0][5] <= 10 && pieceCnt[1][5] <= 10 && // knight
             pieceCnt[0][6] <= 8 && pieceCnt[1][6] <= 8 && // pawn
             pieceCnt[0][2] + pieceCnt[0][3] + pieceCnt[0][4] + pieceCnt[0][5] + pieceCnt[0][6] <= 15
             && pieceCnt[1][2] + pieceCnt[1][3] + pieceCnt[1][4] + pieceCnt[1][5] + pieceCnt[1][6] <= 15;

    if (!b) {
        return false;
    }

    for (auto sd = 0; sd < 2; sd++) {
        for (auto i = 0; i < 16; i++) {
            auto pos = pieceList[sd][i];
            if (pos < 0 || pos >= 64) {
                continue;
            }
            cnt--;
            if (sd != static_cast<int>(pieces[pos].side)) {
                return false;
            }
        }
    }
    return cnt == 0;
}

std::string ChessBoard::toString() const
{
    std::ostringstream stringStream;

    stringStream << getFen(false) << std::endl;

    for (auto i = 0; i < 64; i++) {
        auto piece = _getPiece(i);

        stringStream << toString(Piece(piece.type, piece.side)) << " ";

        if (i > 0 && getColumn(i) == 7) {
            int row = 8 - getRow(i);
            stringStream << " " << row << "\n";
        }
    }

    stringStream << "a b c d e f g h  " << side2String(side, false) << std::endl;
    stringStream << "key: " << key() << std::endl;

    return stringStream.str();
}

void ChessBoard::checkEnpassant()
{
    if ((enpassant >= 16 && enpassant < 24) || (enpassant >= 40 && enpassant < 48)) {
        return;
    }
    enpassant = -1;
}

void ChessBoard::setFenCastleRights_clear()
{
    castleRights[0] = castleRights[1] = 0;
}

void ChessBoard::setFenCastleRights(const std::string& str)
{
    for (auto&& ch : str) {
        switch (ch) {
            case 'K':
                if (pieces[60].type == KING && pieces[60].side == Side::white && pieces[63].type == ROOKSTD
                    && pieces[63].side == Side::white) {
                    castleRights[W] |= CastleRight_short;
                }
                break;
            case 'k':
                if (pieces[4].type == KING && pieces[4].side == Side::black && pieces[7].type == ROOKSTD
                    && pieces[7].side == Side::black) {
                    castleRights[B] |= CastleRight_short;
                }
                break;
            case 'Q':
                if (pieces[60].type == KING && pieces[60].side == Side::white && pieces[56].type == ROOKSTD
                    && pieces[56].side == Side::white) {
                    castleRights[W] |= CastleRight_long;
                }
                break;
            case 'q':
                if (pieces[4].type == KING && pieces[4].side == Side::black && pieces[0].type == ROOKSTD
                    && pieces[0].side == Side::black) {
                    castleRights[B] |= CastleRight_long;
                }
                break;

            default:
                break;
        }
    }
}

bool ChessBoard::_setFen(const std::string& fen)
{
    reset();

    std::string str = fen;
    startFen = fen;

    /// A chess960 board given no FEN takes the standard array (position 518), as a standard board
    /// does
    if (variant == ChessVariant::chess960 && fen.empty()) {
        str = getOriginFen(ChessVariant::standard);
        startFen = str;
    }

    if (variant != ChessVariant::chess960) {
        auto originalFen = getOriginFen(variant);
        if (fen.empty()) {
            str = originalFen;
        } else if (fen.compare(0, originalFen.size(), originalFen) == 0) { // not memcmp: a shorter FEN was read past its end
            startFen = "";
        }
    }

    side = Side::none;
    enpassant = -1;
    status = 0;
    setFenCastleRights_clear();

    auto vec = splitString(str, ' ');
    auto thefen = vec.front();

    auto ok = true;

    for (size_t i = 0, pos = 0; i < thefen.length(); i++) {
        char ch = thefen.at(i);

        if (ch == '/') {
            continue;
        }

        if (ch >= '0' && ch <= '8') {
            int num = ch - '0';
            pos += num;
            continue;
        }

        auto side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }

        auto pieceType = charactorToPieceType(ch);

        if (pieceType == EMPTY || pos >= 64) {
            ok = false;
        } else {
            _setPiece(int(pos), Piece(pieceType, side));
        }
        pos++;
    }

    /// side
    _setFen_basic_2ndpart(vec);

    /// castle rights
    if (vec.size() >= 3 && vec.at(2) != "-") {
        auto str = vec.at(2);
        setFenCastleRights(str);
    }

    /// enpassant
    if (vec.size() >= 4 && vec.at(3).size() >= 2) {
        // enpassant
        auto str = vec.at(3);
        auto pos = coordinateStringToPos(str.c_str(), banksia::moveFormat);
        if (isPositionValid(pos)) {
            enpassant = pos;
        }
    }

    // half + full move
    _setFenComplete();

    // standardize the starting FEN, good for chess960, fix problem such as missing move count numbers
    if (!startFen.empty()) {
        startFen = getFen();
    }

    return ok;
}

void ChessBoard::_setFenComplete()
{
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
    memset(pieceCnt, 0, sizeof(pieceCnt));

    auto vec = splitString(fen, ' ');
    auto thefen = vec.front();

    auto errCnt = 0, pos = 0;
    for (size_t i = 0; i < thefen.length(); i++) {
        char ch = thefen.at(i);

        if (ch == '/') {
            continue;
        }

        if (ch >= '0' && ch <= '8') {
            int num = ch - '0';
            pos += num;
            continue;
        }

        auto side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }

        auto pieceType = chessCharactorToPieceType(ch);
        if (pieceType == EMPTY) {
            errCnt++;
        } else {
            pieceCnt[static_cast<int>(side)][static_cast<int>(pieceType)]++;
        }
        pos++;
    }

    if (pos > 50 && pos <= 64 && errCnt < 3 && pieceCnt[0][KING] == 1 && pieceCnt[1][KING] == 1) {
        bool b = pieceCnt[0][2] <= 9 && pieceCnt[1][2] <= 9 && // queen
                 pieceCnt[0][3] <= 10 && pieceCnt[1][3] <= 10 && // rook
                 pieceCnt[0][4] <= 10 && pieceCnt[1][4] <= 10 && // bishop
                 pieceCnt[0][5] <= 10 && pieceCnt[1][5] <= 10 && // knight
                 pieceCnt[0][6] <= 8 && pieceCnt[1][6] <= 8 && // pawn
                 pieceCnt[0][2] + pieceCnt[0][3] + pieceCnt[0][4] + pieceCnt[0][5] + pieceCnt[0][6] <= 15
                 && pieceCnt[1][2] + pieceCnt[1][3] + pieceCnt[1][4] + pieceCnt[1][5] + pieceCnt[1][6] <= 15;
        return b;
    }

    return false;
}

std::string ChessBoard::getFenCastleRights() const
{
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

/// check if opponent could capture the last advanced-2-rows Pawn
bool ChessBoard::canRivalCaptureEnpassant() const
{
    if (enpassant > 0 && enpassant < 64) {
        auto col = getColumn(enpassant), row = getRow(enpassant);
        assert(row == 2 || row == 5);
        if (row == 2) {
            return ((col && _isPiece(enpassant + 7, PAWNSTD, Side::white))
                    || (col < 7 && _isPiece(enpassant + 9, PAWNSTD, Side::white)));
        } else {
            return ((col && _isPiece(enpassant - 9, PAWNSTD, Side::black))
                    || (col < 7 && _isPiece(enpassant - 7, PAWNSTD, Side::black)));
        }
    }

    return false;
}

std::string ChessBoard::getFen(bool enpassantLegal, int halfCount, int fullMoveCount, bool horizontalFlip,
                               const MoveFormat& mf) const
{
    std::string str;

    int e = 0;
    for (auto i = 0; i < 64; i++) {
        auto k = i;
        if (horizontalFlip) {
            auto col = i % 8;
            k = i + 7 - 2 * col;
        }
        auto piece = _getPiece(k);
        if (piece.isEmpty()) {
            e += 1;
        } else {
            if (e) {
                str += std::to_string(e);
                e = 0;
            }
            str += toString(piece);
        }

        if (i % 8 == 7) {
            if (e) {
                str += std::to_string(e);
            }
            if (i < 63) {
                str += "/";
            }
            e = 0;
        }
    }

    std::string epStr = "-";
    if (enpassant > 0 && (!enpassantLegal || canRivalCaptureEnpassant())) {
        epStr = posToCoordinateString(enpassant, mf);
    }

    str += (side == Side::white ? " w " : " b ") + getFenCastleRights() + " " + epStr;

    if (halfCount >= 0 && fullMoveCount >= 0) {
        str += " " + std::to_string(halfCount) + " " + std::to_string(fullMoveCount);
    }

    return str;
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

    assert(movingPiece.type == PAWN);
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

void ChessBoard::clearCastleRights(int rookPos, Side rookSide)
{
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


bool ChessBoard::_isIncheck(Side beingAttackedSide) const
{
    int kingPos = findKing(beingAttackedSide);
    Side attackerSide = xSide(beingAttackedSide);
    return kingPos >= 0 ? beAttacked(kingPos, attackerSide) : false;
}

////////////////////////////////////////////////////////////////////////
void ChessBoard::_gen(std::vector<MoveFull>& moves, Side side, int forPieceType) const
{
    moves.reserve(Chess_MaxMoveNumber);

    auto captureOnly = false;

    for (auto pos = 0; pos < 64; ++pos) {
        auto piece = pieces[pos];

        if (piece.side != side || (forPieceType > EMPTY && forPieceType != piece.type)) {
            continue;
        }

        auto theType = static_cast<PieceTypeStd>(piece.type);

        switch (theType) {
            case PieceTypeStd::king: {
                genBishop(moves, side, pos, true);
                genRook(moves, side, pos, true);

                if (!captureOnly) {
                    gen_castling(moves, pos);
                }
                break;
            }

            case PieceTypeStd::queen: {
                genBishop(moves, side, pos, false);
                genRook(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::bishop: {
                genBishop(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::rook: // both queen and rook here
            {
                genRook(moves, side, pos, false);
                break;
            }

            case PieceTypeStd::knight: {
                genKnight(moves, side, pos);
                break;
            }

            case PieceTypeStd::pawn: {
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

    if (y >= 0 && col < 6) gen_addMove(moves, pos, y, captureOnly);
    y = pos - 10;
    if (y >= 0 && col > 1) gen_addMove(moves, pos, y, captureOnly);
    y = pos - 15;
    if (y >= 0 && col < 7) gen_addMove(moves, pos, y, captureOnly);
    y = pos - 17;
    if (y >= 0 && col > 0) gen_addMove(moves, pos, y, captureOnly);
    y = pos + 6;
    if (y < 64 && col > 1) gen_addMove(moves, pos, y, captureOnly);
    y = pos + 10;
    if (y < 64 && col < 6) gen_addMove(moves, pos, y, captureOnly);
    y = pos + 15;
    if (y < 64 && col > 0) gen_addMove(moves, pos, y, captureOnly);
    y = pos + 17;
    if (y < 64 && col < 7) gen_addMove(moves, pos, y, captureOnly);
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
    for (int y = pos - 9; y >= 0 && getColumn(y) != 7; y -= 9) { /* go left up */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos - 7; y >= 0 && getColumn(y) != 0; y -= 7) { /* go right up */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos + 9; y < 64 && getColumn(y) != 0; y += 9) { /* go right down */
        gen_addMove(moves, pos, y, captureOnly);
        if (oneStep || !_isEmpty(y)) {
            break;
        }
    }
    for (int y = pos + 7; y < 64 && getColumn(y) != 7; y += 7) { /* go right down */
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

        if (col
            && (_getPiece(pos + 7).side == Side::white
                || (pos + 7 == enpassant && _getPiece(pos + 7).side == Side::none))) {
            gen_addPawnMove(moves, pos, pos + 7, captureOnly);
        }
        if (col < 7
            && (_getPiece(pos + 9).side == Side::white
                || (pos + 9 == enpassant && _getPiece(pos + 9).side == Side::none))) {
            gen_addPawnMove(moves, pos, pos + 9, captureOnly);
        }
    } else {
        if (!captureOnly && _isEmpty(pos - 8)) {
            gen_addPawnMove(moves, pos, pos - 8, captureOnly);
        }
        if (!captureOnly && pos >= 48 && _isEmpty(pos - 8) && _isEmpty(pos - 16)) {
            gen_addMove(moves, pos, pos - 16, captureOnly);
        }

        if (col < 7
            && (_getPiece(pos - 7).side == Side::black
                || (pos - 7 == enpassant && _getPiece(pos - 7).side == Side::none)))
            gen_addPawnMove(moves, pos, pos - 7, captureOnly);
        if (col
            && (_getPiece(pos - 9).side == Side::black
                || (pos - 9 == enpassant && _getPiece(pos - 9).side == Side::none)))
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
        if ((castleRights[B] & CastleRight_long) && pieces[1].isEmpty() && pieces[2].isEmpty()
            && pieces[3].isEmpty() && !beAttacked(2, Side::white) && !beAttacked(3, Side::white)
            && !beAttacked(4, Side::white)) {
            assert(_isPiece(0, static_cast<int>(PieceTypeStd::rook), Side::black));
            gen_addMove(moves, 4, 2, captureOnly);
        }
        if ((castleRights[B] & CastleRight_short) && pieces[5].isEmpty() && pieces[6].isEmpty()
            && !beAttacked(4, Side::white) && !beAttacked(5, Side::white) && !beAttacked(6, Side::white)) {
            assert(_isPiece(7, static_cast<int>(PieceTypeStd::rook), Side::black));
            gen_addMove(moves, 4, 6, captureOnly);
        }
    } else {
        if ((castleRights[W] & CastleRight_long) && pieces[57].isEmpty() && pieces[58].isEmpty()
            && pieces[59].isEmpty() && !beAttacked(58, Side::black) && !beAttacked(59, Side::black)
            && !beAttacked(60, Side::black)) {
            assert(_isPiece(56, static_cast<int>(PieceTypeStd::rook), Side::white));
            gen_addMove(moves, 60, 58, captureOnly);
        }
        if ((castleRights[W] & CastleRight_short) && pieces[61].isEmpty() && pieces[62].isEmpty()
            && !beAttacked(60, Side::black) && !beAttacked(61, Side::black) && !beAttacked(62, Side::black)) {
            assert(_isPiece(63, static_cast<int>(PieceTypeStd::rook), Side::white));
            gen_addMove(moves, 60, 62, captureOnly);
        }
    }
}

bool ChessBoard::beAttacked(int pos, Side attackerSide) const
{
    int row = getRow(pos), col = getColumn(pos);
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos + 8)) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos - 8)) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos - 1)) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos + 1)) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos + 9
                        && (piece.type == KING
                            || (piece.type == static_cast<int>(PieceTypeStd::pawn)
                                && piece.side == Side::white)))) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos + 7
                        && (piece.type == KING
                            || (piece.type == static_cast<int>(PieceTypeStd::pawn)
                                && piece.side == Side::white)))) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos - 9
                        && (piece.type == KING
                            || (piece.type == static_cast<int>(PieceTypeStd::pawn)
                                && piece.side == Side::black)))) {
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
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos - 7
                        && (piece.type == KING
                            || (piece.type == static_cast<int>(PieceTypeStd::pawn)
                                && piece.side == Side::black)))) {
                    return true;
                }
            }
            break;
        }
    }

    return false;
}


std::vector<int> ChessBoard::see_genAttack(Side attackerSide, int pos, const int* usedSet) const
{
    std::map<int, std::vector<int>> theMap {
        { 0, std::vector<int>() }, { 1, std::vector<int>() }, { 2, std::vector<int>() },
        { 3, std::vector<int>() }, { 4, std::vector<int>() }, { 5, std::vector<int>() },
        { 6, std::vector<int>() }, { 7, std::vector<int>() }, { 8, std::vector<int>() },
    };

    auto row = getRow(pos), col = getColumn(pos);

    // Knight
    const auto KNIGHT = static_cast<int>(PieceTypeStd::knight);
    if (!usedSet[pos - 17] && col > 0 && row > 1
        && _isPiece(pos - 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos - 17);
    if (!usedSet[pos - 15] && col < 7 && row > 1
        && _isPiece(pos - 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos - 15);
    if (!usedSet[pos - 10] && col > 1 && row > 0
        && _isPiece(pos - 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos - 10);
    if (!usedSet[pos - 6] && col < 6 && row > 0
        && _isPiece(pos - 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos - 6);
    if (!usedSet[pos + 6] && col > 1 && row < 7
        && _isPiece(pos + 6, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos + 6);
    if (!usedSet[pos + 10] && col < 6 && row < 7
        && _isPiece(pos + 10, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos + 10);
    if (!usedSet[pos + 15] && col > 0 && row < 6
        && _isPiece(pos + 15, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos + 15);
    if (!usedSet[pos + 17] && col < 7 && row < 6
        && _isPiece(pos + 17, static_cast<int>(PieceTypeStd::knight), attackerSide))
        theMap[KNIGHT].push_back(pos + 17);


    /* go down */
    for (int y = pos + 8; y < 64; y += 8) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos + 8)) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go up */
    for (int y = pos - 8; y >= 0; y -= 8) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos - 8)) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go left */
    for (int y = pos - 1; y >= pos - col; y--) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos - 1)) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go right */
    for (int y = pos + 1; y < pos - col + 8; y++) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::rook)
                    || (piece.type == KING && y == pos + 1)) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* Check diagonal lines for attacking of Queen, Bishop, King, Pawn */
    /* go right down */
    for (int y = pos + 9; y < 64 && getColumn(y) != 0; y += 9) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos + 9
                        && (piece.type == KING
                            || (piece.type == static_cast<int>(PieceTypeStd::pawn)
                                && piece.side == Side::white)))) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go left down */
    for (int y = pos + 7; y < 64 && getColumn(y) != 7; y += 7) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos + 7
                        && (piece.type == KING || (piece.type == PAWNSTD && piece.side == Side::white)))) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go left up */
    for (int y = pos - 9; y >= 0 && getColumn(y) != 7; y -= 9) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos - 9
                        && (piece.type == KING || (piece.type == PAWNSTD && piece.side == Side::black)))) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    /* go right up */
    for (int y = pos - 7; y >= 0 && getColumn(y) != 0; y -= 7) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                if (piece.type == static_cast<int>(PieceTypeStd::queen)
                    || piece.type == static_cast<int>(PieceTypeStd::bishop)
                    || (y == pos - 7
                        && (piece.type == KING || (piece.type == PAWNSTD && piece.side == Side::black)))) {
                    theMap[piece.type].push_back(y);
                }
            }
            break;
        }
    }

    std::vector<int> vec;
    for (int i = PAWNSTD; i > 0; i--) {
        if (!theMap[i].empty()) {
            vec.insert(vec.end(), theMap[i].begin(), theMap[i].end());
        }
    }
    return vec;
}

// empty, king, queen, rook, bishop, knight, pawn
static const int seePieceVals[] = { 0, 10000, 900, 500, 300, 300, 100, 0, 0 };

static int see_pieceVal(int type, int dest)
{
    if (type == PAWNSTD && (dest < 8 || dest >= 56)) {
        type = static_cast<int>(PieceTypeStd::queen);
    }

    return seePieceVals[type];
}

int ChessBoard::see_capture(Side side, int pos, int xPieceVal, int* usedSet) const
{
    auto vec = see_genAttack(side, pos, usedSet);
    auto xside = xSide(side);
    auto gain = 0;

    for (auto&& x : vec) {
        assert(usedSet[x] == 0);
        usedSet[x] = 1;

        auto fromType = pieces[x].type;

        auto bonus = 0;
        auto pieceVal = seePieceVals[fromType];
        if (fromType == PAWNSTD && (pos < 8 || pos >= 56)) {
            pieceVal = seePieceVals[static_cast<int>(PieceTypeStd::queen)];
            bonus = pieceVal - seePieceVals[PAWNSTD];
        }

        gain = std::max(gain, xPieceVal + bonus - see_capture(xside, pos, pieceVal, usedSet));
        usedSet[x] = 0;
    }

    return gain;
}

int ChessBoard::see(const Move& m) const
{
    assert(m.isValid() && !pieces[m.from].isEmpty());

    int usedSet[64];
    memset(&usedSet, 0, sizeof(usedSet));
    usedSet[m.from] = 1;

    auto xside = xSide(pieces[m.from].side);

    auto fromType = pieces[m.from].type;

    auto destType = pieces[m.dest].type;

    /// enpassant
    if (destType == EMPTY && fromType == PAWNSTD && std::abs(m.from - m.dest) != 8) {
        destType = PAWNSTD;
    }
    auto capVal = see_pieceVal(destType, m.dest);

    auto pieceVal = seePieceVals[fromType];
    if (m.promotion != EMPTY) {
        assert(fromType == PAWNSTD && (m.dest < 8 || m.dest >= 56));
        pieceVal = seePieceVals[m.promotion];
        capVal += pieceVal - seePieceVals[PAWNSTD];
    }

    auto gain = capVal - see_capture(xside, m.dest, pieceVal, usedSet);
    return gain;
}


// low 2 bytes is for black, hi for white
int ChessBoard::materials() const
{
    int w = 0, b = 0;

    for (auto&& p : pieces) {
        if (p.isEmpty() || p.type == KING) {
            continue;
        }

        if (p.side == Side::white) {
            w += seePieceVals[p.type];
        } else {
            b += seePieceVals[p.type];
        }
    }
    assert(b < 0xffff && b < 0xffff);
    return b | w << 16;
}


void ChessBoard::_make(const MoveFull& move, Hist& hist)
{
    assert(pieceList_isValid());

    hist.enpassant = enpassant;
    hist.status = status;
    hist.castleRights[0] = castleRights[0];
    hist.castleRights[1] = castleRights[1];
    hist.castled = 0;
    hist.move = move;
    hist.cap = pieces[move.dest];
    hist.hashKey = hashKey;
    hist.quietCnt = quietCnt;
    hist.flag = 0;

    hashKey ^= hashKeyEnpassant(enpassant);

    hashKey ^= xorHashKey(move.from);
    if (!hist.cap.isEmpty()) {
        hashKey ^= xorHashKey(move.dest);
    }

    auto p = move.piece;
    assert(p == pieces[move.from]);
    hist.move.piece = p;
    pieces[move.dest] = p;
    pieces[move.from].setEmpty();

    hashKey ^= xorHashKey(move.dest);

    quietCnt++;
    enpassant = -1;

    if ((castleRights[0] + castleRights[1]) && hist.cap.type == static_cast<int>(PieceTypeStd::rook)) {
        clearCastleRights(move.dest, hist.cap.side);
    }

    auto pieceType = static_cast<PieceTypeStd>(p.type);

    switch (pieceType) {
        case PieceTypeStd::king: {
            castleRights[static_cast<int>(p.side)] &= ~(CastleRight_long | CastleRight_short);
            if (abs(move.from - move.dest) == 2) { /// castle
                auto rookPos = move.from + (move.from < move.dest ? 3 : -4);
                assert(pieces[rookPos].type == static_cast<int>(PieceTypeStd::rook));
                auto newRookPos = (move.from + move.dest) / 2;
                assert(pieces[newRookPos].isEmpty());

                hashKey ^= xorHashKey(rookPos);
                pieces[newRookPos] = pieces[rookPos];
                pieces[rookPos].setEmpty();
                hashKey ^= xorHashKey(newRookPos);
                quietCnt = 0;
                hist.castled = move.dest == 2 || move.dest == 56 + 2 ? CastleRight_long : CastleRight_short;
                hist.flag |= Hist::MoveFlag_Castled;
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
            auto d = abs(move.from - move.dest);
            quietCnt = 0;

            if (d == 16) {
                enpassant = (move.from + move.dest) / 2;
            } else if (move.dest == hist.enpassant) {
                int ep = move.dest + (p.side == Side::white ? +8 : -8);
                hist.cap = pieces[ep];

                hashKey ^= xorHashKey(ep);
                pieces[ep].setEmpty();
                hist.flag |= Hist::MoveFlag_CapEnpassant;
            } else {
                if (move.promotion != EMPTY) {
                    hashKey ^= xorHashKey(move.dest);
                    pieces[move.dest].type = move.promotion;
                    hashKey ^= xorHashKey(move.dest);
                    hist.flag |= Hist::MoveFlag_Promotion;
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

    pieceList_make(hist);
    assert(pieceList_isValid());
}

void ChessBoard::_takeBack(const Hist& hist)
{
    assert(pieceList_isValid());

    auto movep = pieces[hist.move.dest];
    assert(movep.isValid());
    pieces[hist.move.from] = movep;

    auto capPos = hist.move.dest;

    if (hist.move.piece.type == PAWNSTD && hist.enpassant == hist.move.dest) {
        capPos = hist.move.dest + (hist.move.piece.side == Side::white ? +8 : -8);
        pieces[hist.move.dest].setEmpty();
    }
    pieces[capPos] = hist.cap;

    if (hist.move.piece.type == KING) {
        if (abs(hist.move.from - hist.move.dest) == 2) {
            assert(hist.castled == CastleRight_long || hist.castled == CastleRight_short);
            auto rookPos = hist.move.from + (hist.move.from < hist.move.dest ? 3 : -4);
            assert(_isEmpty(rookPos));
            auto newRookPos = (hist.move.from + hist.move.dest) / 2;
            assert(pieces[newRookPos].type == ROOKSTD && pieces[newRookPos].side == hist.move.piece.side);
            pieces[rookPos] = pieces[newRookPos];
            pieces[newRookPos].setEmpty();
        }
    }

    if (hist.move.promotion != EMPTY) {
        /// OCGDB: the pawn keeps its index. A new Piece has none (-1): every take-back of a
        /// promotion -- also of each promotion tried when the legal moves are generated, as for a
        /// check -- left the pawn without it, and the 1-byte move encoding (Moves1), which names
        /// a piece by its index, then wrote the pawn's next move wrong.
        Piece pawn(PAWNSTD, movep.side);
        pawn.idx = movep.idx;
        pieces[hist.move.from] = pawn;
        assert(pieces[hist.move.from].isValid() && pieces[hist.move.from].type == PAWNSTD);
    }

    status = hist.status;
    castleRights[0] = hist.castleRights[0];
    castleRights[1] = hist.castleRights[1];
    enpassant = hist.enpassant;
    quietCnt = hist.quietCnt;

    hashKey = hist.hashKey;

    pieceList_takeback(hist);
    assert(pieceList_isValid());
}

Result ChessBoard::ruleRepetition(int /*repeatLen*/)
{
    return Result(ResultType::draw, ReasonType::repetition);
}

Result ChessBoard::rule()
{
    auto result = BoardCore::rule();
    if (!result.isNone()) {
        return result;
    }

    std::lock_guard<std::mutex> dolock(dataMutex);

    // draw by insufficientmaterial
    int pieceCnt[2][8], bishopColor[2][2];
    memset(pieceCnt, 0, sizeof(pieceCnt));
    memset(bishopColor, 0, sizeof(bishopColor));

    auto draw = true;
    for (auto i = 0; i < 64; i++) {
        auto p = pieces[i];
        if (p.type <= KING) continue;
        if (p.type != static_cast<int>(PieceTypeStd::bishop)
            && p.type != static_cast<int>(PieceTypeStd::knight)) {
            draw = false;
            break;
        }

        auto sd = static_cast<int>(p.side);
        auto t = p.type;
        pieceCnt[sd][t]++;

        if (p.type == static_cast<int>(PieceTypeStd::bishop)) {
            auto c = (getColumn(i) + getRow(i)) & 1;
            bishopColor[sd][c]++;
            if ((bishopColor[sd][0] && bishopColor[sd][1])
                || pieceCnt[sd][static_cast<int>(
                        PieceTypeStd::knight)]) { // bishops in different colors or a knight + a bishop
                draw = false;
                break;
            }
        } else if (pieceCnt[sd][t]) {
            if (pieceCnt[sd][t] > 1
                || pieceCnt[sd][static_cast<int>(
                        PieceTypeStd::bishop)]) { // more than 2 knights or a knight + a bishop
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

    // repetition
    return result;
}

// Check and make the move if it is legal
bool ChessBoard::_checkMake(int from, int dest, int promotion)
{
    Move move(from, dest, promotion);
    if (!move.isValid() || !BoardCore::isValid(move)) {
        return false;
    }

    auto piece = _getPiece(from), cap = _getPiece(dest);
    if (piece.isEmpty() || piece.side != side
        || (piece.side == cap.side
            && (variant != ChessVariant::chess960 || piece.type != KING
                || cap.type != static_cast<int>(PieceTypeStd::rook)))
        || !Move::isValidPromotion(promotion)) {
        return false;
    }

    std::vector<MoveFull> moveList;
    _gen(moveList, side);
    for (auto&& move : moveList) {
        assert(_isHashKeyValid());
        if (move.from != from || move.dest != dest || move.promotion != promotion) {
            continue;
        }

        auto theSide = side;
        auto fullmove = createFullMove(from, dest, promotion);
        _make(fullmove);
        assert(side != theSide);

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

        for (auto i = from + d, step = 0; i != dest; i += d, step++) {
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

    if (d == 2) { /// O-O, O-O-O
        auto rf = getRow(from), rd = getRow(dest);
        if (rf == rd && ((from == 60 && side == Side::white) || (from == 4 && side == Side::black))) {
            if (checkMore) {
                if (from < dest) { /// O-O
                    auto rook = _getPiece(from + 3);
                    return _isEmpty(from + 1) && _isEmpty(from + 2)
                           && rook.type == static_cast<int>(PieceTypeStd::rook)
                           && rook.side == _getPiece(from).side;
                } else { /// O-O-O
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
        auto rf = getRow(from), rd = getRow(dest);
        if (d == 1) {
            return rf == rd;
        }

        return std::abs(rf - rd) == 1;
    }

    return false;
}

/// Check and make the move if it is legal
bool ChessBoard::_quickCheckMake(int from, int dest, int promotion, bool createSanString)
{
    Move move(from, dest, promotion);
    if (!BoardCore::isValid(move)) {
        return false;
    }

    auto piece = _getPiece(from), cap = _getPiece(dest);
    if (piece.isEmpty() || piece.side != side
        || (piece.side == cap.side
            && (variant != ChessVariant::chess960 || piece.type != KING
                || cap.type != static_cast<int>(PieceTypeStd::rook)))
        || !Move::isValidPromotion(promotion)) {
        return false;
    }

    bool checkMore = true;
    assert(isValid());

    switch (static_cast<PieceTypeStd>(piece.type)) {
        case PieceTypeStd::king: {
            if (!_quickCheck_king(from, dest, checkMore)) {
                return false;
            }
            break;
        }

        case PieceTypeStd::queen: {
            if (_quickCheck_rook(from, dest, checkMore) || _quickCheck_bishop(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::bishop: {
            if (_quickCheck_bishop(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::rook: {
            if (_quickCheck_rook(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeStd::knight: {
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

        case PieceTypeStd::pawn: {
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
                if (getRow(from) - getRow(dest) != 1) {
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
                if (getRow(from) - getRow(dest) != -1) {
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
    _make(fullmove);
    assert(side != theSide);

    if (checkMore && _isIncheck(theSide)) {
        _takeBack();
        return false;
    }

    if (createSanString) {
        createSanStringForLastMove(banksia::moveFormat);
    }


    assert(isValid());
    return true;
}

std::string ChessBoard::chessPiece2String(const Piece& piece, bool alwayLowerCase)
{
    char ch = chessPieceType2Char(piece.type);
    if (!alwayLowerCase && piece.side == Side::white) {
        ch += 'A' - 'a';
    }
    return std::string(1, ch);
}

std::string ChessBoard::piece2String(const Piece& piece, bool alwayLowerCase) const
{
    return chessPiece2String(piece, alwayLowerCase);
}

char ChessBoard::pieceType2Char(int pieceType) const
{
    return chessPieceType2Char(pieceType);
}

std::string ChessBoard::toString(const Piece& piece) const
{
    return piece2String(piece, false);
}

std::string ChessBoard::moveString_coordinate(const Move& move)
{
    /// Plain concatenation, not an ostringstream
    std::string s = chessPosToCoordinateString(move.from) + chessPosToCoordinateString(move.dest);
    if (move.promotion > KING && move.promotion < PAWNSTD) {
        s += chessPiece2String(Piece(move.promotion, Side::white), true);
    }
    return s;
}

std::string ChessBoard::toString(const Move& move, const MoveFormat& /*mf*/) const
{
    return moveString_coordinate(move);
}

std::string ChessBoard::toString(const MoveFull& move, const MoveFormat& mf) const
{
    return toString(Move(move), mf);
}

std::string ChessBoard::toString_san(const Hist& hist, const MoveFormat&) const
{
    return hist.sanString;
}

std::string ChessBoard::hist2String(const HistBasic& hist, Notation notation)
{
    switch (notation) {
        case Notation::san:
            return hist.sanString;
        case Notation::lan:
            if (hist.move.isValid() && hist.move.piece.isValid()) {
                return toString_lan(hist.move, ChessVariant::standard);
            }
            break;

        default:
            if (hist.move.isValid()) {
                return moveString_coordinate(Move(hist.move));
            }
    }

    return "";
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

    /// special cases - castling moves
    if (movePiece.type == KING && hist->castled) {
        assert(hist->castled == CastleRight_long || hist->castled == CastleRight_short);
        str = hist->castled == CastleRight_long ? "O-O-O" : "O-O";
    } else {
        auto ambi = false, sameCol = false, sameRow = false;

        if (movePiece.type != static_cast<int>(PieceTypeStd::king)) {
            for (auto&& m : moveList) {
                if (m.dest == hist->move.dest && m.from != hist->move.from
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
                str += posToCoordinateString(hist->move.from, banksia::moveFormat);
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

        str += posToCoordinateString(hist->move.dest, banksia::moveFormat);

        /// promotion
        if (hist->move.promotion != EMPTY) {
            str += "=";
            str += char(pieceType2Char(hist->move.promotion) - 'a' + 'A');
        }
    }

    /// incheck
    if (_isIncheck(side)) {
        assert(_isHashKeyValid());

        std::vector<MoveFull> moveList;
        _genLegalOnly(moveList, side);
        str += moveList.empty() ? "#" : "+";
    }

    hist->sanString = str;
    return true;
}

/// don't penetrate the origin pos (from)
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

std::vector<int> ChessBoard::_attackByPawn(int from, int dest, const Piece& movePiece, const Piece& cap,
                                           int enpassant)
{
    std::vector<int> vec;
    auto col = getColumn(dest);

    if (movePiece.side == Side::white) {
        auto p = _getPiece(dest + 8);
        if (p == movePiece)
            vec.push_back(dest + 8);
        else if (p.isEmpty() && dest >= 32 && dest < 40 && dest + 8 != from) {
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
        if (p == movePiece)
            vec.push_back(dest - 8);
        else if (p.isEmpty() && dest >= 24 && dest < 32 && dest - 8 != from) {
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
    int row = getRow(pos), col = getColumn(pos);
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

bool ChessBoard::createSanStringForLastMove(const MoveFormat& mf)
{
    if (histList.empty()) {
        return false;
    }

    auto hist = &histList.back();

    auto movePiece = hist->move.piece;
    if (movePiece.isEmpty()) {
        return false; // something wrong
    }

    hist->sanAmbiguous = false;
    hist->sanSameCol = false;
    hist->sanSamRow = false;

    std::string str;

    // special cases - castling moves
    if (movePiece.type == KING && hist->castled) {
        assert(hist->castled == CastleRight_long || hist->castled == CastleRight_short);
        str = hist->castled == CastleRight_long ? "O-O-O" : "O-O";
    } else {
        std::vector<int> avec;
        auto t = static_cast<PieceTypeStd>(movePiece.type);
        switch (t) {
            case PieceTypeStd::rook: {
                avec = _attackByRook(hist->move.from, hist->move.dest, movePiece);
                break;
            }
            case PieceTypeStd::bishop: {
                avec = _attackByBishop(hist->move.from, hist->move.dest, movePiece);
                break;
            }
            case PieceTypeStd::queen: {
                avec = _attackByQueen(hist->move.from, hist->move.dest, movePiece);
                break;
            }
            case PieceTypeStd::knight: {
                avec = _attackByKnight(hist->move.dest, movePiece);
                break;
            }
            case PieceTypeStd::pawn: {
                avec = _attackByPawn(hist->move.from, hist->move.dest, movePiece, hist->cap, hist->enpassant);
                break;
            }
            default:
                break;
        }

        if (!avec.empty()) {
            hist->sanAmbiguous = true;

            for (auto&& pos : avec) {
                if (pos / 8 == hist->move.from / 8) {
                    hist->sanSamRow = true;
                }
                if (pos % 8 == hist->move.from % 8) {
                    hist->sanSameCol = true;
                }
            }
        }

        if (movePiece.type != PAWNSTD) {
            str = char(pieceType2Char(movePiece.type) - 'a' + 'A');
        }
        if (hist->sanAmbiguous) {
            if (hist->sanSameCol && hist->sanSamRow) {
                str += posToCoordinateString(hist->move.from, mf);
            } else if (hist->sanSameCol) {
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

        str += posToCoordinateString(hist->move.dest, mf);

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
    return chessCharactorToPieceType(ch);
}

Move ChessBoard::moveFromString_castling(const std::string& str, Side side) const
{
    assert(str == "O-O" || str == "O-O+" || str == "0-0" || str == "O-O-O" || str == "O-O-O+"
           || str == "0-0-0");

    auto from = side == Side::black ? 4 : 60;
    auto dest = from + (str.length() < 5 ? 2 : -2);
    return Move(from, dest, EMPTY);
}

Move ChessBoard::moveFromString_san(const std::string& str, const MoveFormat& mf)
{
    if (str.length() < 2) {
        return Move::illegalMove;
    }

    if (str == "O-O" || str == "O-O+" || str == "0-0" || str == "O-O-O" || str == "O-O-O+"
        || str == "0-0-0") {
        return moveFromString_castling(str, side);
    }

    std::string s;
    for (auto ch : str) {
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
            return Move::illegalMove;
            ;
        }

        s = s.substr(0, p);
        if (s.size() < 2 || promotion == EMPTY) {
            return Move::illegalMove;
            ;
        }
    }

    if (s.length() < 2) {
        return Move::illegalMove;
    }

    auto destString = s.substr(s.length() - 2, 2);
    dest = coordinateStringToPos(destString.c_str(), mf);

    if (!isPositionValid(dest)) {
        return Move::illegalMove;
        ;
    }

    if (s.length() > 2) {
        auto k = 0;
        char ch = s.at(0);
        if (ch >= 'A' && ch <= 'Z') {
            k++;
            pieceType = charactorToPieceType(ch);

            if (pieceType == EMPTY) {
                return Move::illegalMove;
                ;
            }
        }

        auto left = s.length() - k - 2;
        if (left > 0) {
            s = s.substr(k, left);
            if (left == 2) {
                from = coordinateStringToPos(s.c_str(), mf);
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
        _gen(moveList, side, pieceType);

        std::vector<Move> goodMoves;
        for (auto&& m : moveList) {
            if (m.dest != dest || m.promotion != promotion || _getPiece(m.from).type != pieceType) {
                continue;
            }

            if ((fromRow < 0 && fromCol < 0) || (fromRow >= 0 && getRow(m.from) == fromRow)
                || (fromCol >= 0 && getColumn(m.from) == fromCol)) {
                goodMoves.push_back(m);
                from = m.from;
            }
        }

        if (goodMoves.size() > 1) {
            for (auto&& m : goodMoves) {
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

        if (from < 0) {
            return Move::illegalMove;
            ;
        }
    }

    /// A damaged SAN can name its destination as its origin ("Nf6xf6"): illegal, not an assert
    if (from == dest) {
        return Move::illegalMove;
    }

    assert(Move::isValidPromotion(promotion));
    assert(from >= 0 && dest >= 0 && from != dest);
    return Move(from, dest, promotion);
}

std::vector<Move> ChessBoard::coordinateString2Moves(const std::string& str)
{
    std::vector<Move> moves;
    auto ss = replaceString(str, ".", " ");
    auto list = splitString(ss, ' ');
    for (auto&& s : list) {
        auto ch = s.at(0);
        if (isdigit(ch)) continue;

        Move m = ChessBoard::chessMoveFromCoordiateString(s);
        moves.push_back(m);
    }

    return moves;
}


void ChessBoard::_clone(const BoardData* oboard)
{
    BoardData::_clone(oboard);
    auto ob = static_cast<const ChessBoard*>(oboard);
    enpassant = ob->enpassant;
    castleRights[0] = ob->castleRights[0];
    castleRights[1] = ob->castleRights[1];

    castleRights_column_king = ob->castleRights_column_king;
    castleRights_column_rook_left = ob->castleRights_column_rook_left;
    castleRights_column_rook_right = ob->castleRights_column_rook_right;
}

#define HAVE_COMMENT_BIT (1 << 15)


int ChessBoard::toPieceCount(int* pieceCnt) const
{
    if (pieceCnt) {
        memset(pieceCnt, 0, 14 * sizeof(int));
    }
    auto totalCnt = 0;
    ;
    for (auto i = 0; i < 64; i++) {
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
    for (auto i = 0; i < 64; i++) {
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
    static int pieceCompareValues[] = { 0, 10000, 100, 50, 20, 20, 1, 1 };

    return pieceCompareValues[type0] < pieceCompareValues[type1];
}


int ChessBoard::attackerCnt() const
{
    auto cnt = 0;
    for (auto&& piece : pieces) {
        if (piece.type >= QUEEN) cnt++;
    }
    return cnt;
}

bool ChessBoard::hasAttackers() const
{
    for (auto i = 1; i < 16; ++i) { /// from the queen: the king is ignored
        if (pieceList[0][i] >= 0 || pieceList[1][i] >= 0) {
            return true;
        }
    }
    return false;
}

/// Chess960 castles by the king "capturing" its own rook.
bool ChessBoard::canMoveOntoOwnPiece(const Piece& piece, const Piece& target) const
{
    return variant == ChessVariant::chess960 && piece.type == KING
           && target.type == static_cast<int>(PieceTypeStd::rook);
}

/// OCGDB: no endgame tablebases. Banksia probes Syzygy here (Fathom, under the tournament
/// manager's GameMng::egtbSource); a database tool never adjudicates games, so the probes are
/// left out and the board answers "no tablebase".
std::vector<MoveFull> ChessBoard::probeEgtbAll() const
{
    return BoardCore::probeEgtbAll();
}

Result ChessBoard::probeEgtb(int, bool& tberror) const
{
    tberror = false;
    return Result();
}


u64 ChessBoard::xorHashKey(int pos) const
{
    assert(isPositionValid(pos));

    assert(!pieces[pos].isEmpty());

    auto sd = static_cast<int>(pieces[pos].side);
    auto kind_of_piece = (6 - static_cast<int>(pieces[pos].type)) * 2 + sd;
    assert(kind_of_piece >= 0 && kind_of_piece <= 11);

    auto file = getColumn(pos), row = 7 - getRow(pos);
    auto offset_piece = 64 * kind_of_piece + 8 * row + file;

    return RandomPiece[offset_piece];
}

u64 ChessBoard::hashKeyEnpassant(int enpassant) const
{
    u64 key = 0;

    if (enpassant > 0 && enpassant < 64) {
        assert(enpassant >= 16 && enpassant < 48);

        auto col = getColumn(enpassant), row = getRow(enpassant);
        assert(row == 2 || row == 5);
        [[maybe_unused]] auto ok = false;
        if (row == 2) {
            ok = _isPiece(enpassant + 8, static_cast<int>(PieceTypeStd::pawn), Side::black)
                 && ((col && _isPiece(enpassant + 7, static_cast<int>(PieceTypeStd::pawn), Side::white))
                     || (col < 7
                         && _isPiece(enpassant + 9, static_cast<int>(PieceTypeStd::pawn), Side::white)));
        } else {
            ok = _isPiece(enpassant - 8, static_cast<int>(PieceTypeStd::pawn), Side::white)
                 && ((col && _isPiece(enpassant - 9, static_cast<int>(PieceTypeStd::pawn), Side::black))
                     || (col < 7
                         && _isPiece(enpassant - 7, static_cast<int>(PieceTypeStd::pawn), Side::black)));
        }

        if (ok) {
            key ^= RandomEnPassant[col];
        }
    }

    return key;
}

u64 ChessBoard::xorSideHashKey() const
{
    return *RandomTurn;
}

u64 ChessBoard::initHashKey() const
{
    u64 key = 0;
    for (int i = 0, n = static_cast<int>(pieces.size()); i < n; i++) {
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









bool ChessBoard::pieceList_setPiece(int* pieceList, int pos, int type, Side side)
{
    if (type != EMPTY) {
        auto d = side == Side::white ? 16 : 0;

        auto k = type <= QUEEN ? 1 : type == PAWNSTD ? 8 : 2;
        for (auto t = pieceListStartIdxByType[static_cast<int>(type)]; k > 0; t++, k--) {
            assert(t >= 0 && t < 16);
            if (pieceList[d + t] < 0 || pieceList[d + t] == pos) {
                pieceList[d + t] = pos;
                return true;
            }
        }

        if (type != PAWNSTD) {
            for (auto t = pieceListStartIdxByType[PAWNSTD]; t < 16; t++) {
                assert(t >= 0 && t < 16);
                if (pieceList[d + t] < 0 || pieceList[d + t] == pos) {
                    pieceList[d + t] = pos;
                    return true;
                }
            }
        }
    }
    return false;
}

bool ChessBoard::pieceList_isValid() const
{
    auto cnt = 0;
    for (auto sd = 0; sd < 2; sd++) {
        for (auto i = 0; i < 16; i++) {
            auto k = pieceList[sd][i];
            if (k < 0) {
                continue;
            }
            if (k >= 64) {
                return false;
            }
            auto piece = pieces[k];
            if (!piece.isValid() || static_cast<int>(piece.side) != sd) {
                printOut("pieceList_isValid");
                return false;
            }

            auto tp = pieceListIdxToType[i];

            if (tp < PieceType::pawn && piece.type != static_cast<int>(tp)) {
                return false;
            }
            cnt++;
        }
    }

    for (auto&& p : pieces) {
        if (!p.isEmpty()) cnt--;
    }

    return cnt == 0;
}

bool ChessBoard::pieceList_setEmpty(int* pieceList, int pos, int type, Side side)
{
    auto k = type <= QUEEN ? 1 : type == PAWNSTD ? 8 : 2;
    auto d = side == Side::white ? 16 : 0;
    for (auto t = pieceListStartIdxByType[type]; k > 0; t++, k--) {
        assert(t >= 0 && t < 16);
        if (pieceList[d + t] == pos) {
            pieceList[d + t] = -1;
            return true;
        }
    }

    if (type != PAWNSTD) {
        for (auto t = pieceListStartIdxByType[PAWNSTD]; t < 16; t++) {
            assert(t >= 0 && t < 16);
            if (pieceList[d + t] == pos) {
                pieceList[d + t] = -1;
                return true;
            }
        }
    }
    return false;
}

/// The loops below walk the piece-list slots of one piece type. They stop at the end of the list
/// and report the failure when the piece is not in it, which happens when the board came from a
/// FEN that does not fit it, say a xiangqi position with three Advisors.
bool ChessBoard::pieceList_make(const Hist& hist)
{
    if (!hist.cap.isEmpty()) {
        auto dest = hist.move.dest;
        if (hist.cap.type == PAWNSTD && dest == hist.enpassant) {
            dest = dest + (hist.cap.side == Side::black ? +8 : -8);
        }
        [[maybe_unused]] auto ok = false;
        for (auto t = pieceListStartIdxByType[hist.cap.type], sd = static_cast<int>(hist.cap.side); t < 16; t++) {
            assert(t >= 0 && t < 16);
            if (pieceList[sd][t] == dest) {
                pieceList[sd][t] = -1;
                ok = true;
                break;
            }
        }

        assert(ok);
    }

    auto type = hist.move.promotion != EMPTY ? hist.move.promotion : hist.move.piece.type;
    for (auto t = pieceListStartIdxByType[type], sd = static_cast<int>(hist.move.piece.side); t < 16; t++) {
        assert(t >= 0 && t < 16);

        if (pieceList[sd][t] == hist.move.from) {
            pieceList[sd][t] = hist.move.dest;

            /// castle rook
            if (type == KING && abs(hist.move.from - hist.move.dest) == 2) {
                auto rookPos = hist.move.from + (hist.move.from < hist.move.dest ? 3 : -4);
                auto newRookPos = (hist.move.from + hist.move.dest) / 2;

                for (auto v = pieceListStartIdxByType[ROOKSTD];; v++) {
                    assert(v >= 0 && v < 16);
                    if (pieceList[sd][v] == rookPos) {
                        pieceList[sd][v] = newRookPos;
                        return true;
                    }
                }
                assert(false);
                return false;
            }
            return true;
        }
    }

    assert(false);
    return false;
}

bool ChessBoard::pieceList_takeback(const Hist& hist)
{
    auto type = hist.move.promotion != EMPTY ? hist.move.promotion : hist.move.piece.type;
    for (auto t = pieceListStartIdxByType[type], sd = static_cast<int>(hist.move.piece.side); t < 16; t++) {
        assert(t >= 0 && t < 16);

        if (pieceList[sd][t] == hist.move.dest) {
            if (hist.move.promotion == EMPTY) {
                pieceList[sd][t] = hist.move.from;
                /// Castle
                if (hist.move.piece.type == KING && abs(hist.move.from - hist.move.dest) == 2) {
                    assert(hist.move.from == 4 || hist.move.from == 60);
                    int rfrom, rdest;
                    if (hist.move.from > hist.move.dest) { /// O-O-O
                        rfrom = hist.move.from - 4;
                        rdest = hist.move.from - 1;
                    } else { /// O-O
                        rfrom = hist.move.from + 3;
                        rdest = hist.move.from + 1;
                    }

                    /// board takeback already, Rook in the original position
                    assert(pieces[rdest].isEmpty() && pieces[rfrom].type == ROOKSTD
                           && static_cast<int>(pieces[rfrom].side) == sd);
                    for (auto h = pieceListStartIdxByType[ROOKSTD];; h++) {
                        assert(h >= 0 && h < 16);

                        if (pieceList[sd][h] == rdest) {
                            pieceList[sd][h] = rfrom;
                            break;
                        }
                    }
                }
            } else {
                pieceList[sd][t] = -1;
                for (auto t2 = pieceListStartIdxByType[PAWNSTD];; t2++) {
                    assert(t2 < 16);
                    if (pieceList[sd][t2] < 0) {
                        pieceList[sd][t2] = hist.move.from;
                        break;
                    }
                }
            }
            break;
        }
    }

    if (hist.cap.isEmpty()) {
        return true;
    }

    auto dest = hist.move.dest;

    /// en passant
    if (_isEmpty(dest)) {
        assert(hist.move.piece.type == PAWNSTD && hist.cap.type == PAWNSTD);

        dest += (hist.cap.side == Side::black ? +8 : -8);
    }

    assert(!pieceList_hasPos((const int*)&pieceList, dest, hist.cap.side));

    auto sd = static_cast<int>(hist.cap.side);
    auto k = hist.cap.type == QUEEN ? 1 : hist.cap.type == PAWNSTD ? 8 : 2;
    for (auto t = pieceListStartIdxByType[hist.cap.type]; k > 0; t++, k--) {
        assert(t >= 0 && t < 16);

        if (pieceList[sd][t] < 0) {
            pieceList[sd][t] = dest;
            return true;
        }
    }

    for (auto t = pieceListStartIdxByType[PAWNSTD]; t < 16; t++) {
        assert(t >= 0 && t < 16);

        if (pieceList[sd][t] < 0) {
            pieceList[sd][t] = dest;
            return true;
        }
    }

    assert(false);
    return false;
}

bool ChessBoard::pieceList_isDraw(const int* pieceList) const
{
    for (auto i = 1; i < 16; i++) {
        if (pieceList[i] >= 0 || pieceList[i + 16] >= 0) {
            return false;
        }
    }
    return true;
}


/// for chess only
static const int flip_r90[64]
        = { 7,  15, 23, 31, 39, 47, 55, 63, 6,  14, 22, 30, 38, 46, 54, 62, 5,  13, 21, 29, 37, 45,
            53, 61, 4,  12, 20, 28, 36, 44, 52, 60, 3,  11, 19, 27, 35, 43, 51, 59, 2,  10, 18, 26,
            34, 42, 50, 58, 1,  9,  17, 25, 33, 41, 49, 57, 0,  8,  16, 24, 32, 40, 48, 56 };

static const int flip_r270[64]
        = { 56, 48, 40, 32, 24, 16, 8,  0,  57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18,
            10, 2,  59, 51, 43, 35, 27, 19, 11, 3,  60, 52, 44, 36, 28, 20, 12, 4,  61, 53, 45, 37,
            29, 21, 13, 5,  62, 54, 46, 38, 30, 22, 14, 6,  63, 55, 47, 39, 31, 23, 15, 7 };

static const int flip_vh[64]
        = { // a8-h1
            0,  8,  16, 24, 32, 40, 48, 56, 1,  9,  17, 25, 33, 41, 49, 57, 2,  10, 18, 26, 34, 42,
            50, 58, 3,  11, 19, 27, 35, 43, 51, 59, 4,  12, 20, 28, 36, 44, 52, 60, 5,  13, 21, 29,
            37, 45, 53, 61, 6,  14, 22, 30, 38, 46, 54, 62, 7,  15, 23, 31, 39, 47, 55, 63
          };

static const int flip_hv[64]
        = { // a1-h8
            63, 55, 47, 39, 31, 23, 15, 7,  62, 54, 46, 38, 30, 22, 14, 6,  61, 53, 45, 37, 29, 21,
            13, 5,  60, 52, 44, 36, 28, 20, 12, 4,  59, 51, 43, 35, 27, 19, 11, 3,  58, 50, 42, 34,
            26, 18, 10, 2,  57, 49, 41, 33, 25, 17, 9,  1,  56, 48, 40, 32, 24, 16, 8,  0
          };


void ChessBoard::flip(FlipMode flipMode)
{
    switch (flipMode) {
        case FlipMode::none:
            return;

        case FlipMode::rotate90:
        case FlipMode::rotate270: {
            histList.clear();

            ChessBoard board2;
            board2.reset();
            board2.side = side;

            for (auto pos = 0; pos < 64; pos++) {
                auto piece = _getPiece(pos);
                auto pos2 = csFlip(pos, flipMode);
                board2._setPiece(pos2, piece);
            }

            _clone(&board2);
            _setFenComplete();
            return;
        }

        default:
            BoardCore::flip(flipMode);
    }
}


int ChessBoard::csFlip(int pos, FlipMode flipMode)
{
    if (pos >= 0) {
        switch (flipMode) {
            case FlipMode::none:
                return pos;
            case FlipMode::horizontal: {
                auto f = pos % 8;
                return pos - 2 * f + 8 - 1;
            }

            case FlipMode::vertical: {
                auto r = pos / 8, c = pos % 8;
                return (7 - r) * 8 + c;
            }
            case FlipMode::rotate: {
                return 63 - pos;
            }

            /// For chess only since it has a square board
            case FlipMode::flipVH:
                return flip_vh[pos];
            case FlipMode::flipHV:
                return flip_hv[pos];
            case FlipMode::rotate90:
                return flip_r90[pos];
            case FlipMode::rotate270:
                return flip_r270[pos];

            default:
                assert(false);
                return pos;
        }
    }

    return pos;
}
