/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <iomanip> // for setprecision
#include <fstream>
#include <iostream>

#include "xq.h"
#include "jieqiboard.h"


/// defined by the host application (console/main.cpp, the GUI's basewindow.cpp)


namespace banksia {

    const int originalXqBaseFenLen = 59;

    const int maxPieceCntsByTypes[9] = { 0, 1, 2, 2, 2, 2, 2, 5, 15 };

    const char* notationPieceNames[][9] = {
        { "", "K", "A", "E", "R", "C", "H", "P", "X" }, // san
        { "", "K", "A", "E", "R", "C", "H", "P", "X" }, // lan
        { "", "K", "A", "E", "R", "C", "H", "P", "X" }, // algebraic_coordinate
        { "", "K", "A", "E", "R", "C", "H", "P", "X" } // traditional_en
    };

    static const char* advanceNames[][3] = {
        { "", "", "" }, // san
        { "", "", "" }, // lan
        { "", "", "" }, // algebraic_coordinate
        { "+", "-", "." } // traditional_en
    };

    static auto& originalBoard() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
    {
        static const Piece v[90] = { // line 9
                Piece(static_cast<int>(PieceTypeXq::rook), Side::black),
                Piece(static_cast<int>(PieceTypeXq::horse), Side::black),
                Piece(static_cast<int>(PieceTypeXq::elephant), Side::black),
                Piece(static_cast<int>(PieceTypeXq::advisor), Side::black),
                Piece(static_cast<int>(PieceTypeXq::king), Side::black),
                Piece(static_cast<int>(PieceTypeXq::advisor), Side::black),
                Piece(static_cast<int>(PieceTypeXq::elephant), Side::black),
                Piece(static_cast<int>(PieceTypeXq::horse), Side::black),
                Piece(static_cast<int>(PieceTypeXq::rook), Side::black),
                // line 8
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                // line 7
                Piece::emptyPiece, Piece(static_cast<int>(PieceTypeXq::cannon), Side::black),
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::cannon), Side::black), Piece::emptyPiece,
                // line 6
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::black), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::black), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::black), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::black), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::black),
                // line 5
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,

                // line 4
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                // line 3
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::white), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::white), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::white), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::white), Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::pawn), Side::white),
                // line 2
                Piece::emptyPiece, Piece(static_cast<int>(PieceTypeXq::cannon), Side::white),
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece(static_cast<int>(PieceTypeXq::cannon), Side::white), Piece::emptyPiece,
                // line 1
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece, Piece::emptyPiece,
                // line 0
                Piece(static_cast<int>(PieceTypeXq::rook), Side::white),
                Piece(static_cast<int>(PieceTypeXq::horse), Side::white),
                Piece(static_cast<int>(PieceTypeXq::elephant), Side::white),
                Piece(static_cast<int>(PieceTypeXq::advisor), Side::white),
                Piece(static_cast<int>(PieceTypeXq::king), Side::white),
                Piece(static_cast<int>(PieceTypeXq::advisor), Side::white),
                Piece(static_cast<int>(PieceTypeXq::elephant), Side::white),
                Piece(static_cast<int>(PieceTypeXq::horse), Side::white),
                Piece(static_cast<int>(PieceTypeXq::rook), Side::white)
              };
        return v;
    }

}

using namespace banksia;

static const int8_t legalPosBits[] = { 0, 1, 2, 4, 0, 0, 0, 8, 16 };

static const int8_t legalPositions[90] = {
    16, 16, 29, 19, 17, 19, 20, 16, 16, 0, 0,  0, 1,  3, 1, 0,  0, 0, 4, 16, 0, 3, 5,
    3,  0,  16, 4,  24, 0,  24, 0,  24, 0, 24, 0, 24, 8, 0, 12, 0, 8, 0, 12, 0, 8,

    8,  8,  8,  8,  8,  8,  8,  8,  8,  8, 8,  8, 8,  8, 8, 8,  8, 8, 8, 8,  8, 8, 8,
    8,  8,  8,  8,  8,  8,  8,  8,  8,  8, 8,  8, 8,  8, 8, 8,  8, 8, 8, 8,  8, 8,
};


extern const int pieceListStartIdxByType[8];
const int pieceListStartIdxByType[8] = { -1, 0, 1, 3, 5, 7, 9, 11 };

extern const PieceType pieceListIdxToType[16];

const PieceType pieceListIdxToType[16]
        = { PieceType::king,     PieceType::advisor, PieceType::advisor, PieceType::elephant,
            PieceType::elephant, PieceType::rook,    PieceType::rook,    PieceType::cannon,
            PieceType::cannon,   PieceType::horse,   PieceType::horse,   PieceType::pawn,
            PieceType::pawn,     PieceType::pawn,    PieceType::pawn,    PieceType::pawn };

XqBoard::XqBoard()
{
    variant = ChessVariant::xq;
    assert(variant == ChessVariant::xq || variant == ChessVariant::jieqi);

    pieces.clear();
    for (auto i = 0; i < 90; i++) {
        pieces.push_back(Piece::emptyPiece);
    }
}

XqBoard::XqBoard(const XqBoard& other)
{
    _clone(&other);
}

XqBoard::~XqBoard() { }

void XqBoard::_clone(const BoardData* oboard)
{
    assert(isXqFamily(oboard->variant));
    BoardData::_clone(oboard);
    auto ob = static_cast<const XqBoard*>(oboard);

    memcpy(pieceCount, ob->pieceCount, sizeof(pieceCount));
    memcpy(capturedCount, ob->capturedCount, sizeof(capturedCount));
}


/// How many men of this type a side has left to reveal -- the "bag".
///
/// It is the initial complement less the ones already revealed, whether they
/// are still on the board (pieceCount) or have since been captured
/// (capturedCount). Counting only what is on the board would let a type be
/// revealed again every time one of them is taken.
///
/// A covered man captured while still covered is deliberately *not* subtracted.
/// Its identity was never determined, so the identities still unrevealed remain
/// equally likely for every covered man left standing; capturedCount only ever
/// counts men that were face up when they were taken.
int XqBoard::availableRevelationCount(int pieceType, Side side) const
{
    if (pieceType <= KING || pieceType >= COVERXQ) {
        return 0;
    }
    auto sd = static_cast<int>(side);
    auto n = maxPieceCntsByTypes[pieceType] - pieceCount[sd][pieceType] - capturedCount[sd][pieceType];
    return n > 0 ? n : 0;
}

bool XqBoard::isValidPromotion(int promotion, Side side) const
{
    if (promotion != EMPTY) {
        return availableRevelationCount(promotion, side) > 0;
    }
    return true;
}

int XqBoard::xqCharactorToPieceType(char ch)
{
    /// The table is comm.cpp's; the letter-folding lives with the table.
    return banksia::xqCharactorToPieceType(ch);
}

/// OCGDB: no endgame tablebases. Banksia probes the Felicity tablebases here for the
/// tournament manager (GameMng::egtbSource); a database tool never adjudicates games, so the
/// probes are left out and the board answers "no tablebase".
Result XqBoard::probeEgtb(int /*maxPieces*/, bool& tberror) const
{
    tberror = false;
    return Result();
}


bool XqBoard::canLocatePiece(int type, Side side, int pos) const
{
    if (pos >= 0 && pos < 90) {
        auto flag = legalPosBits[static_cast<int>(type)];
        if (flag) {
            auto k = side == Side::white ? (89 - pos) : pos;
            return (flag & legalPositions[k]) != 0;
        }
        return true;
    }
    return false;
}

char XqBoard::pieceType2Char(int pieceType) const
{
    return xqPieceType2Char(pieceType, moveFormat);
}

int XqBoard::coordinateStringToPos(const std::string& str, const MoveFormat& mf) const
{
    return xqCoordinateStringToPos(str, mf);
}

std::string XqBoard::posToCoordinateString(int pos, const MoveFormat& mf) const
{
    return xqPosToCoordinateString(pos, mf);
}


int XqBoard::xqCoordinateStringToPos(const std::string& str, const MoveFormat& mf)
{
    auto colChr = str[0], rowChr = str[1];
    if (colChr >= 'a' && colChr <= 'i' && rowChr >= '0' && rowChr <= '9') {
        int col = colChr - 'a';
        int row = rowChr - '0';

        if (mf.rankFrom1) {
            if (rowChr == '1' && str.length() > 2 && str[2] == '0') {
                row = 10;
            }
            row--;
        }

        return (9 - row) * 9 + col;
    }
    return -1;
}

bool XqBoard::isOriginal() const
{
    if (side != Side::white) {
        return false;
    }
    for (auto i = 0; i < 90; i++) {
        if (pieces[i] != originalBoard()[i]) {
            return false;
        }
    }
    return true;
}

bool XqBoard::isValid() const
{
    int cnt[2][10] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
    int pawnHome[2][5] = { { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 } };
    int kings[2] = { 0, 0 };

    for (auto i = 0; i < 90; i++) {
        auto p = pieces[i];
        if (p.isEmpty()) {
            continue;
        }
        auto sd = static_cast<int>(p.side);
        cnt[sd][p.type]++;

#ifdef _COVERXQ_
        if (variant == ChessVariant::COVERXQ && p.type != KING) {
            if (p.type == static_cast<int>(PieceTypeXq::COVERXQ) && originalBoard()[i].isEmpty()) {
                return false;
            }
        } else
#endif
                if (variant != ChessVariant::jieqi || p.type == KING) {
            auto flag = legalPosBits[p.type];
            if (flag) {
                auto k = p.side == Side::white ? (89 - i) : i;
                if ((flag & legalPositions[k]) == 0) {
                    return false;
                }
            }
        }

        if (p.type == KING) {
            kings[sd] = i;
        } else if (p.type == static_cast<int>(PieceTypeXq::pawn)
                   && ((p.side == Side::white && i >= 45) || (p.side == Side::black && i < 45))) {
            int col = (i % 9) / 2;
            pawnHome[sd][col]++;
        }
    }

    if (variant == ChessVariant::xq) {
        for (auto i = 0; i < 5; i++) {
            if (pawnHome[0][i] > 1 || pawnHome[1][i] > 1) {
                return false;
            }
        }
    }

    /// Total
    int totals[2] = { 0, 0 }, caps[2] = { 0, 0 };
    for (auto sd = 0; sd < 2; ++sd) {
        for (auto i = KING; i <= COVERXQ; i++) {
            int n = i == KING ? 1 : i == COVERXQ ? 15 : i < PAWNXQ ? 2 : 5;

            if (pieceCount[sd][i] < 0 || pieceCount[sd][i] > n || capturedCount[sd][i] < 0
                || capturedCount[sd][i] > n || pieceCount[sd][i] + capturedCount[sd][i] > n) {
                return false;
            }
            if (i != COVERXQ && cnt[sd][i] != pieceCount[sd][i]) {
                return false;
            }

            totals[sd] += pieceCount[sd][i];
            caps[sd] += capturedCount[sd][i];
            if (i > KING && i <= PAWNXQ) {
                auto t = pieceCount[sd][i] + capturedCount[sd][i];
                auto n = i < static_cast<int>(PieceTypeXq::pawn) ? 2 : 5;
                if ((variant == ChessVariant::xq && t != n) || (variant == ChessVariant::jieqi && t > n)) {
                    return false;
                }
            }
        }
        if (totals[sd] + caps[sd] != 16) {
            return false;
        }
    }

    if (kings[0] % 9 == kings[1] % 9) {
        auto hasMidPiece = false;
        for (auto i = kings[0] + 9; i < kings[1]; i += 9) {
            if (!pieces[i].isEmpty()) {
                hasMidPiece = true;
                break;
            }
        }

        return hasMidPiece;
    }
    return true;
}

void XqBoard::setupPieceCount()
{
    memset(pieceCount, 0, sizeof(pieceCount));
    for (auto&& piece : pieces) {
        if (piece.isEmpty()) continue;
        auto sd = static_cast<int>(piece.side);
        pieceCount[sd][piece.type]++;
    }
}

std::string XqBoard::toString() const
{
    std::ostringstream stringStream;

    stringStream << getFen(false) << std::endl;

    for (auto i = 0; i < 90; i++) {
        auto piece = _getPiece(i);
        stringStream << piece2String(piece) << " ";

        if (i > 0 && getColumn(i) == 8) {
            int row = 9 - getRow(i);
            stringStream << " " << row << "\n";
        }
    }

    stringStream << "a b c d e f g h i  " << side2String(side, false) << std::endl;
    stringStream << "key: " << key() << std::endl;

    return stringStream.str();
}

bool XqBoard::_setFen(const std::string& fen)
{
    reset();

    std::string str = fen;
    startFen = fen;
    if (fen.empty()) {
        str = getOriginFen(variant);
    } else {
        if (fen.compare(0, originalXqBaseFenLen, getOriginFen(variant), 0, originalXqBaseFenLen) == 0) { // not memcmp: a shorter FEN was read past its end
            startFen = "";
        }
    }

    side = Side::none;
    status = 0;

    auto vec = splitString(str, ' ');
    if (vec.empty() || vec.at(0).empty()) {
        return false;
    }

    std::string fieldOne = vec.at(0);

    /// captured enclosed in brackets [ ]
    std::vector<std::string> fiedlOneVec = splitString(fieldOne, '[');
    if (fiedlOneVec.empty()) { /// something wrong
        return false;
    }

    std::string thefen = fiedlOneVec.at(0);

    side = Side::white;

    auto ok = true;
    for (auto i = 0, pos = 0; i < int(thefen.length()); i++) {
        char ch = char(thefen.at(i));

        if (ch == ' ') {
            continue;
        }

        if (ch == '/') {
            continue;
        }

        if (ch >= '0' && ch <= '9') {
            int num = ch - '0';
            pos += num;
            continue;
        }

        auto side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }

        auto pieceType = xqCharactorToPieceType(ch);

        if (pieceType == EMPTY || pos >= 90) {
            ok = false;
        } else {
            assert(pieceType >= EMPTY && pieceType <= COVERXQ);
            if (pieceType != EMPTY && pieceType <= COVERXQ) {
                setPiece(pos, Piece(pieceType, side));
#ifdef _COVERXQ_
                if (pieceType == COVERXQ) {
                    variant = ChessVariant::COVERXQ;
                }
#endif
            }
        }
        pos++;
    }

    /// Captures
    memset(capturedCount, 0, sizeof(capturedCount));
    if (fiedlOneVec.size() > 1) {
#ifdef _COVERXQ_
        variant = ChessVariant::COVERXQ;
#endif
        auto s = fiedlOneVec.at(1);
        if (s.length() > 1) { /// it is ended by ]

            for (size_t i = 0, len = s.length(); i < len - 1; i++) {
                char ch = s.at(i);

                int sd = B;
                if (ch >= 'A' && ch < 'Z') {
                    sd = W;
                    ch += 'a' - 'A';
                }

                auto pieceType = xqCharactorToPieceType(ch);
                if (pieceType > KING && pieceType < COVERXQ) {
                    char ch2 = s.at(i + 1);
                    if (ch2 >= '0' && ch2 <= '9') {
                        int n = ch2 - '0';

                        if (pieceType == COVERXQ && i + 2 < len) {
                            char ch3 = s.at(i + 2);
                            if (ch3 >= '0' && ch3 <= '9') {
                                n = n * 10 + static_cast<int>(ch2 - '0');
                                i++;
                            }
                        }
                        capturedCount[sd][static_cast<int>(pieceType)] = n;
                        i++;
                    }
                }
            }
        }
    }

    _setFen_basic_2ndpart(vec);

    /// Side

    _setFenComplete();

    /// A plain xiangqi FEN lists no captures: what is missing from a side's full set (2 of each
    /// officer, 5 pawns) was captured. isValid() wants on board + captured to make the full set,
    /// so without this every position with a piece missing would be invalid and newGame() would
    /// refuse every mid-game FEN.
    if (variant == ChessVariant::xq) {
        for (auto sd = 0; sd < 2; sd++) {
            for (auto t = KING + 1; t <= PAWNXQ; t++) {
                auto n = t < static_cast<int>(PieceTypeXq::pawn) ? 2 : 5;
                capturedCount[sd][t] = std::max(0, n - pieceCount[sd][t]);
            }
        }
    }

    /// standardize the starting FEN, good for COVERXQ, and complete missing move count numbers
    if (!startFen.empty()) {
        startFen = getFen();
    }

    assert(!ok || pieceList_isValid());
    return ok;
}

void XqBoard::_setFenComplete()
{
    setupPieceIndexes();
    setupPieceCount();
    setupCapturedCount();

    hashKey = initHashKey();
}


bool XqBoard::simpleSetupCaptureNumber(int pieceType, Side side, int newCaptureNumber)
{
    auto sd = static_cast<int>(side);
    if (pieceType < banksia::KING || pieceType > banksia::COVERXQ || sd < 0 || sd > 1
        || newCaptureNumber < 0) {
        return false;
    }

    capturedCount[sd][pieceType] = newCaptureNumber;
    return true;
}


bool XqBoard::setupCaptureNumber(int pieceType, Side side, int newCaptureNumber)
{
    auto sd = static_cast<int>(side);
    if (pieceType <= banksia::KING || pieceType > banksia::PAWNXQ || sd < 0 || sd > 1 || newCaptureNumber < 0
        || newCaptureNumber == capturedCount[sd][pieceType]) {
        return false;
    }

    auto maxNum = pieceType == banksia::PAWNXQ ? 5 : 2;
    if (newCaptureNumber > maxNum - pieceCount[sd][pieceType]) {
        return false;
    }

    /// More new captured than the number of COVERXQ left
    auto d = newCaptureNumber - capturedCount[sd][pieceType];
    if (d > capturedCount[sd][banksia::COVERXQ]) {
        return false;
    }

    capturedCount[sd][pieceType] = newCaptureNumber;
    setupCapturedCount();
    return true;
}


void XqBoard::setupCapturedCount()
{
    int totals[2] = { 0, 0 }, caps[2] = { 0, 0 };
    for (auto sd = 0; sd < 2; ++sd) {
        for (int i = KING; i < COVERXQ; i++) {
            totals[sd] += pieceCount[sd][i];

            auto n = i < banksia::PAWNXQ ? 2 : 5;
            auto captured = n - pieceCount[sd][i];
            if (variant == ChessVariant::xq) {
                if (i > KING) {
                    capturedCount[sd][i] = captured;
                }
            } else {
                /// Correct the error
                if (captured < capturedCount[sd][i]) {
                    capturedCount[sd][i] = captured;
                }
                caps[sd] += capturedCount[sd][i];
            }
        }

        totals[sd] += pieceCount[sd][COVERXQ];
        if (variant == ChessVariant::jieqi) {
            capturedCount[sd][COVERXQ] = 16 - totals[sd] - caps[sd];
        }
    }
}


std::string XqBoard::getStartingFen(const MoveFormat& mf) const
{
    auto fenString = BoardCore::getStartingFen(mf);
    if (!fenString.empty()) {
        /// standardise fen by mf
        /// (this board's variant: a plain XqBoard is xiangqi, whose FEN has no men taken, so a jieqi
        /// game's start lost its "[A1c1]" on the way to the engines and the files
        XqBoard board;
        board.variant = variant;
        board._setFen(fenString);
        fenString = board.getFen(0, fen_quietCnt, fen_fullMoveCnt, false, mf);
    }
    /// The round trip above is what applies mf: BoardCore::getStartingFen()
    /// returns startFen verbatim, and XqBoard::getFen() ends by converting the
    /// piece letters when charSet is not standard. Re-parsing to re-emit looks
    /// wasteful and is the only place the conversion happens.
    return fenString;
}

std::string XqBoard::getFen(bool /*enpassantLegal*/, int halfCount, int fullMoveCount, bool horizontalFlip,
                            const MoveFormat& mf) const
{
    std::ostringstream stringStream;

    int e = 0;
    for (auto i = 0; i < 90; i++) {
        auto k = i;
        if (horizontalFlip) {
            auto col = i % 9;
            k = i + 8 - 2 * col;
        }
        auto piece = _getPiece(k);
        if (piece.isEmpty()) {
            e += 1;
        } else {
            if (e) {
                stringStream << e;
                e = 0;
            }
            stringStream << toString(piece);
        }

        if (i % 9 == 8) {
            if (e) {
                stringStream << e;
            }
            if (i < 89) {
                stringStream << "/";
            }
            e = 0;
        }
    }

    if (variant == ChessVariant::jieqi) {
        std::string s[2];

        for (auto sd = 0; sd < 2; sd++) {
            for (int i = KING + 1; i < COVERXQ; i++) {
                if (capturedCount[sd][i]) {
                    s[sd] += xqPieceType2Char(i) + std::to_string(capturedCount[sd][i]);
                }
            }
        }

        if (!s[0].empty() || !s[1].empty()) {
            toUpper(s[W]);
            stringStream << "[" << s[W] << s[B] << "]";
        }
    }

    stringStream << (side == Side::white ? " w " : " b ");

    stringStream << "- -";

    if (halfCount >= 0 && fullMoveCount >= 0) {
        stringStream << " " << halfCount << " " << fullMoveCount;
    }

    auto fenString = stringStream.str();
    return mf.charSet == FENCharactorSet::standard ? fenString : convert2BHString(fenString);
}

void XqBoard::gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool captureOnly) const
{
    auto toSide = _getPiece(dest).side;
    auto movingPiece = _getPiece(from);
    auto fromSide = movingPiece.side;

    if (fromSide != toSide && (!captureOnly || toSide != Side::none)) {
        moveList.push_back(MoveFull(movingPiece, from, dest));
    }
}

bool XqBoard::_isIncheck(Side beingAttackedSide) const
{
    int kingPos = findKing(beingAttackedSide);

    Side attackerSide = beingAttackedSide == Side::white ? Side::black : Side::white;

    /*
     * Check horizontal and vertical lines for attacking of Rook, Cannon and
     * King face
     */

    /* go down */
    auto y = kingPos + 9;
    if (y < 90) {
        auto f = 0;
        auto p = pieces[y];
        if (!p.isEmpty()) {
            f = 1;
            if (p.side == attackerSide
                && (p.type == static_cast<int>(PieceTypeXq::rook)
                    || (p.type == static_cast<int>(PieceTypeXq::pawn) && attackerSide == Side::white))) {
                return true;
            }
        }

        for (auto yy = y + 9; yy < 90; yy += 9) {
            auto p = pieces[yy];
            if (p.isEmpty()) {
                continue;
            }
            f++;
            if (p.side == attackerSide) {
                if ((f == 1 && (p.type == static_cast<int>(PieceTypeXq::rook) || p.type == KING))
                    || (f == 2 && p.type == static_cast<int>(PieceTypeXq::cannon))) {
                    return true;
                }
            }
            if (f == 2) {
                break;
            }
        }
    }

    /* go left */
    y = kingPos - 1;
    auto f = 0;

    auto p = pieces[y];
    if (!p.isEmpty()) {
        f = 1;
        if (p.side == attackerSide
            && (p.type == static_cast<int>(PieceTypeXq::rook)
                || p.type == static_cast<int>(PieceTypeXq::pawn))) {
            return true;
        }
    }

    auto col = kingPos % 9;

    for (auto yy = y - 1; yy >= kingPos - col; yy--) {
        auto p = pieces[yy];
        if (p.isEmpty()) {
            continue;
        }
        f++;
        if (p.side == attackerSide) {
            if ((f == 1 && p.type == static_cast<int>(PieceTypeXq::rook))
                || (f == 2 && p.type == static_cast<int>(PieceTypeXq::cannon))) {
                return true;
            }
        }
        if (f == 2) {
            break;
        }
    }

    /* go right */
    y = kingPos + 1;
    f = 0;
    p = pieces[y];
    if (!p.isEmpty()) {
        f = 1;
        if (p.side == attackerSide
            && (p.type == static_cast<int>(PieceTypeXq::rook)
                || p.type == static_cast<int>(PieceTypeXq::pawn))) {
            return true;
        }
    }

    for (auto yy = y + 1; yy < kingPos - col + 9; yy++) {
        auto p = pieces[yy];
        if (p.isEmpty()) {
            continue;
        }
        f++;
        if (p.side == attackerSide) {
            if ((f == 1 && p.type == static_cast<int>(PieceTypeXq::rook))
                || (f == 2 && p.type == static_cast<int>(PieceTypeXq::cannon))) {
                return true;
            }
        }
        if (f == 2) {
            break;
        }
    }

    /* go up */
    y = kingPos - 9;
    if (y >= 0) {
        f = 0;
        p = pieces[y];
        if (!p.isEmpty()) {
            f = 1;
            if (p.side == attackerSide
                && (p.type == static_cast<int>(PieceTypeXq::rook)
                    || (p.type == static_cast<int>(PieceTypeXq::pawn) && attackerSide == Side::black))) {
                return true;
            }
        }

        for (auto yy = y - 9; yy >= 0; yy -= 9) {
            auto p = pieces[yy];
            if (p.isEmpty()) {
                continue;
            }
            f++;
            if (p.side == attackerSide) {
                if ((f == 1 && (p.type == static_cast<int>(PieceTypeXq::rook) || p.type == KING))
                    || (f == 2 && p.type == static_cast<int>(PieceTypeXq::cannon))) {
                    return true;
                }
            }
            if (f == 2) {
                break;
            }
        }
    }

    /* Check attacking of Knight */
    if (kingPos > 9 && _isPiece(kingPos - 11, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos - 10)) {
        return true;
    }
    if (kingPos > 18 && _isPiece(kingPos - 19, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos - 10)) {
        return true;
    }
    if (kingPos > 18 && _isPiece(kingPos - 17, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos - 8)) {
        return true;
    }
    if (kingPos > 9 && _isPiece(kingPos - 7, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos - 8)) {
        return true;
    }
    if (kingPos < 81 && _isPiece(kingPos + 7, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos + 8)) {
        return true;
    }
    if (kingPos < 72 && _isPiece(kingPos + 17, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos + 8)) {
        return true;
    }
    if (kingPos < 72 && _isPiece(kingPos + 19, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos + 10)) {
        return true;
    }
    if (kingPos < 81 && _isPiece(kingPos + 11, static_cast<int>(PieceTypeXq::horse), attackerSide)
        && _isEmpty(kingPos + 10)) {
        return true;
    }

    /* Check attacking of Advisor */
    if (kingPos > 9) {
        if (_isPiece(kingPos - 8, static_cast<int>(PieceTypeXq::advisor), attackerSide)) {
            return true;
        }
        if (_isPiece(kingPos - 10, static_cast<int>(PieceTypeXq::advisor), attackerSide)) {
            return true;
        }
    }
    if (kingPos < 81) {
        if (_isPiece(kingPos + 8, static_cast<int>(PieceTypeXq::advisor), attackerSide)) {
            return true;
        }
        if (_isPiece(kingPos + 10, static_cast<int>(PieceTypeXq::advisor), attackerSide)) {
            return true;
        }
    }


    /* Check attacking of Elephant */
    if (kingPos >= 20 && _isPiece(kingPos - 20, static_cast<int>(PieceTypeXq::elephant), attackerSide)
        && _isEmpty(kingPos - 10)) {
        return true;
    }
    if (kingPos >= 16 && _isPiece(kingPos - 16, static_cast<int>(PieceTypeXq::elephant), attackerSide)
        && _isEmpty(kingPos - 8)) {
        return true;
    }
    if (kingPos < 90 - 16 && _isPiece(kingPos + 16, static_cast<int>(PieceTypeXq::elephant), attackerSide)
        && _isEmpty(kingPos + 8)) {
        return true;
    }
    if (kingPos < 90 - 20 && _isPiece(kingPos + 20, static_cast<int>(PieceTypeXq::elephant), attackerSide)
        && _isEmpty(kingPos + 10)) {
        return true;
    }

    return false;
}

bool XqBoard::isValueSmaller(int type0, int type1) const
{
    /// empty, king, advisor, elephant, rook, cannon, horse, pawn, COVERXQ
    static int pieceCompareValues[] = { 0, 10000, 2, 2, 10, 5, 5, 2, 10 };

    return pieceCompareValues[type0] < pieceCompareValues[type1];
}

////////////////////////////////////////////////////////////////////////
/// don't penetrate the origin pos (from)
std::vector<int> XqBoard::_attackByRook(int from, int dest, const Piece& movePiece)
{
    assert(!_isEmpty(dest));
    std::vector<int> vec;
    auto col = getColumn(dest);

    /* go down */
    for (auto y = dest + 9; y < 90 && y != from; y += 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go up */
    for (auto y = dest - 9; y >= 0 && y != from; y -= 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go left */
    for (auto y = dest - 1; y >= dest - col && y != from; y--) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece) {
                vec.push_back(y);
            }
            break;
        }
    }

    /* go right */
    for (auto y = dest + 1; y < dest - col + 9 && y != from; y++) {
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

/// move by the other Cannon
/// don't penetrate the origin pos (from)
std::vector<int> XqBoard::_moveByCannon(int from, int dest, const Piece& movePiece, bool cap)
{
    assert(!_isEmpty(dest));

    std::vector<int> vec;
    auto col = getColumn(dest);

    /* go down */
    for (auto y = dest + 9, f = 0; y < 90 && y != from; y += 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece && ((f == 1 && cap) || (f == 0 && !cap))) {
                vec.push_back(y);
                break;
            }
            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go up */
    for (auto y = dest - 9, f = 0; y >= 0 && y != from; y -= 9) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece && ((f == 1 && cap) || (f == 0 && !cap))) {
                vec.push_back(y);
                break;
            }
            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go left */
    for (auto y = dest - 1, f = 0; y >= dest - col && y != from; y--) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece && ((f == 1 && cap) || (f == 0 && !cap))) {
                vec.push_back(y);
                break;
            }
            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go right */
    for (auto y = dest + 1, f = 0; y < dest - col + 9 && y != from; y++) {
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece == movePiece && ((f == 1 && cap) || (f == 0 && !cap))) {
                vec.push_back(y);
                break;
            }
            f++;
            if (f > 1) {
                break;
            }
        }
    }

    return vec;
}

std::vector<int> XqBoard::_attackByPawn(int from, int dest, const Piece& movePiece)
{
    assert(!_isEmpty(dest));

    std::vector<int> vec;
    auto col = getColumn(dest);

    if (movePiece.side == Side::white) {
        if (dest < 54 && from != dest + 9 && _getPiece(dest + 9) == movePiece) {
            vec.push_back(dest + 9);
        }

        if (dest < 45) {
            if (col && dest - 1 != from && _getPiece(dest - 1) == movePiece) vec.push_back(dest - 1);
            if (col < 8 && dest + 1 != from && _getPiece(dest + 1) == movePiece) vec.push_back(dest + 1);
        }
    } else {
        if (dest > 35 && from != dest - 9 && _getPiece(dest - 9) == movePiece) {
            vec.push_back(dest - 9);
        }

        if (dest > 44) {
            if (col && dest - 1 != from && _getPiece(dest - 1) == movePiece) vec.push_back(dest - 1);
            if (col < 8 && dest + 1 != from && _getPiece(dest + 1) == movePiece) vec.push_back(dest + 1);
        }
    }

    return vec;
}

std::vector<int> XqBoard::_attackByAdvisor(int from, int dest, const Piece& movePiece)
{
    assert(!_isEmpty(dest));

    std::vector<int> vec;
    auto row = getRow(dest);

    assert(variant != ChessVariant::xq
           || (getColumn(dest) >= 3 && getColumn(dest) <= 5 && (row >= 7 || row <= 2)));

    /// The column has to be checked as well as the row. In xiangqi an advisor
    /// never leaves the palace, so dest is always a middle file and stepping a
    /// column either way is safe; the assert above says as much. In jieqi a
    /// revealed advisor goes anywhere, so an advisor on the a- or i-file walks
    /// off the side of the board -- dest - 10 from a8 is -1, which _getPiece()
    /// then indexes with. _attackByElephant() and _attackByHorse() guard the
    /// column already; this was the one that did not.
    auto col = getColumn(dest);

    if (row) {
        if (col > 0 && from != dest - 10 && _getPiece(dest - 10) == movePiece)
            vec.push_back(dest - 10);
        if (col < 8 && from != dest - 8 && _getPiece(dest - 8) == movePiece)
            vec.push_back(dest - 8);
    }
    if (row < 9) {
        if (col < 8 && from != dest + 10 && _getPiece(dest + 10) == movePiece)
            vec.push_back(dest + 10);
        if (col > 0 && from != dest + 8 && _getPiece(dest + 8) == movePiece)
            vec.push_back(dest + 8);
    }

    return vec;
}

std::vector<int> XqBoard::_attackByElephant(int from, int dest, const Piece& movePiece)
{
    assert(!_isEmpty(dest));

    std::vector<int> vec;
    auto col = getColumn(dest), row = getRow(dest);

    assert(variant != ChessVariant::xq || (col & 1) == 0);

    if (row >= 2) {
        if (col >= 2 && from != dest - 20 && _getPiece(dest - 20) == movePiece && _isEmpty(dest - 10))
            vec.push_back(dest - 20);
        if (col <= 6 && from != dest - 16 && _getPiece(dest - 16) == movePiece && _isEmpty(dest - 8))
            vec.push_back(dest - 16);
    }
    if (row <= 7) {
        if (col >= 2 && from != dest + 16 && _getPiece(dest + 16) == movePiece && _isEmpty(dest + 8))
            vec.push_back(dest + 16);
        if (col <= 6 && from != dest + 20 && _getPiece(dest + 20) == movePiece && _isEmpty(dest + 10))
            vec.push_back(dest + 20);
    }

    return vec;
}


std::vector<int> XqBoard::_attackByHorse(int from, int dest, const Piece& movePiece)
{
    assert(!_isEmpty(dest));

    std::vector<int> vec;
    auto row = getRow(dest), col = getColumn(dest);

    if (row) {
        if (row > 1) {
            if (col > 0 && from != dest - 19 && _getPiece(dest - 19) == movePiece && _isEmpty(dest - 10))
                vec.push_back(dest - 19);
            if (col < 8 && from != dest - 17 && _getPiece(dest - 17) == movePiece && _isEmpty(dest - 8))
                vec.push_back(dest - 17);
        }

        if (col > 1 && from != dest - 11 && _getPiece(dest - 11) == movePiece && _isEmpty(dest - 10))
            vec.push_back(dest - 11);
        if (col < 7 && from != dest - 7 && _getPiece(dest - 7) == movePiece && _isEmpty(dest - 8))
            vec.push_back(dest - 7);
    }

    if (row < 9) {
        if (row < 8) {
            if (col > 0 && from != dest + 17 && _getPiece(dest + 17) == movePiece && _isEmpty(dest + 8))
                vec.push_back(dest + 17);
            if (col < 8 && from != dest + 19 && _getPiece(dest + 19) == movePiece && _isEmpty(dest + 10))
                vec.push_back(dest + 19);
        }

        if (col > 1 && from != dest + 7 && _getPiece(dest + 7) == movePiece && _isEmpty(dest + 8))
            vec.push_back(dest + 7);
        if (col < 7 && from != dest + 11 && _getPiece(dest + 11) == movePiece && _isEmpty(dest + 10))
            vec.push_back(dest + 11);
    }

    return vec;
}


/// OCGDB addition (jieqi): the other covered men of the mover that could have gone to the same
/// point. A covered man moves as the man of the point it started on, so two can reach one point
/// in different ways -- the man on h0 jumping like a horse and the man on h2 sliding like a
/// cannon both reach g2 -- and the SAN "Xg2=P" did not say which: read back, the game went
/// wrong at that move. Worked out on a copy of the board before the move.
std::vector<int> XqBoard::_moveByCover(const Hist& hist) const
{
    std::vector<int> vec;
    if (variant != ChessVariant::jieqi || histList.empty()) {
        return vec;
    }

    JieqiBoard copy(*static_cast<const JieqiBoard*>(this));
    static_cast<BoardCore&>(copy)._takeBack();

    std::vector<MoveFull> moveList;
    copy._genLegalOnly(moveList, hist.move.piece.side);
    for (auto&& m : moveList) {
        if (m.dest == hist.move.dest && m.from != hist.move.from && m.piece.type == COVERXQ
            && std::find(vec.begin(), vec.end(), m.from) == vec.end()) {
            vec.push_back(m.from);
        }
    }
    return vec;
}

bool XqBoard::createSanStringForLastMove(const MoveFormat& mf)
{
    if (histList.empty()) {
        return false;
    }

    auto hist = &histList.back();

    auto movePiece = hist->move.piece;
    if (movePiece.isEmpty()) {
        return false; /// something wrong
    }

    hist->sanAmbiguous = false;
    hist->sanSameCol = false;
    hist->sanSamRow = false;

    std::vector<int> avec;
    auto t = static_cast<PieceTypeXq>(movePiece.type);
    switch (t) {
        case PieceTypeXq::rook: {
            avec = _attackByRook(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeXq::cannon: {
            avec = _moveByCannon(hist->move.from, hist->move.dest, movePiece, !hist->cap.isEmpty());
            break;
        }
        case PieceTypeXq::horse: {
            avec = _attackByHorse(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeXq::advisor: {
            avec = _attackByAdvisor(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeXq::elephant: {
            avec = _attackByElephant(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeXq::pawn: {
            avec = _attackByPawn(hist->move.from, hist->move.dest, movePiece);
            break;
        }
        case PieceTypeXq::cover: {
            avec = _moveByCover(*hist);
            break;
        }
        default:
            break;
    }

    if (!avec.empty()) {
        hist->sanAmbiguous = true;
        for (auto&& pos : avec) {
            if (pos / 9 == hist->move.from / 9) {
                hist->sanSamRow = true;
            }
            if (pos % 9 == hist->move.from % 9) {
                hist->sanSameCol = true;
            }
        }
    }

    /// incheck
    if (_isIncheck(side)) {
        assert(_isHashKeyValid());

        std::vector<MoveFull> moveList;
        _genLegalOnly(moveList, side);
        hist->sanIncheckSym = moveList.empty() ? "#" : "+";
    }

    hist->sanString = getSanString(*hist, mf);
    return true;
}

std::string XqBoard::getSanString(const Hist& hist, const MoveFormat& mf) const
{
    auto movePiece = hist.move.piece;

    std::string str;
    if (movePiece.type != static_cast<int>(PieceTypeXq::pawn)) {
        str = char(xqPieceType2Char(movePiece.type, mf) - 'a' + 'A');
    }

    if (hist.sanAmbiguous) {
        if (hist.sanSameCol && hist.sanSamRow) {
            str += posToCoordinateString(hist.move.from, mf);
        } else if (hist.sanSameCol) {
            auto row = 9 - hist.move.from / 9;
            if (mf.rankFrom1) row++;
            str += std::to_string(row);
        } else {
            str += char('a' + hist.move.from % 9);
        }
    }

    if (!hist.cap.isEmpty()) {
        /// When a pawn makes a capture, the file from which the pawn departed is used to
        /// identify the pawn. For example, exd5
        if (str.empty() && movePiece.type == static_cast<int>(PieceTypeStd::pawn)) {
            str += char('a' + hist.move.from % 9);
        }
        str += "x";
    }

    str += posToCoordinateString(hist.move.dest, mf);

    /// promotion
    if (hist.move.promotion != EMPTY) {
        str += "=";
        str += char(xqPieceType2Char(hist.move.promotion, mf) - 'a' + 'A');
    }

    str += hist.sanIncheckSym;

    return str;
}


void XqBoard::_gen(std::vector<MoveFull>& moves, Side side, int forPieceType) const
{
    moves.reserve(Chess_MaxMoveNumber);

    auto captureOnly = false;


    for (auto pos = 0; pos < 90; ++pos) {
        auto piece = pieces[pos];

        if (piece.side != side || (forPieceType > EMPTY && forPieceType != piece.type)) {
            continue;
        }

        auto ot = static_cast<PieceTypeXq>(piece.type);
        auto theType = ot;
        if (theType == PieceTypeXq::cover) {
            assert(!originalBoard()[pos].isEmpty() && originalBoard()[pos].side == side);
            theType = static_cast<PieceTypeXq>(originalBoard()[pos].type);
        }
        auto COVERXQFreeBoundary = variant == ChessVariant::jieqi && ot != PieceTypeXq::cover;

        switch (theType) {
            case PieceTypeXq::king: {
                auto col = pos % 9;
                if (col != 3) { /// go left
                    gen_addMove(moves, pos, pos - 1, captureOnly);
                }
                if (col != 5) { /// right
                    gen_addMove(moves, pos, pos + 1, captureOnly);
                }
                if (pos > 72 || (pos > 8 && pos < 27)) { /// up
                    gen_addMove(moves, pos, pos - 9, captureOnly);
                }
                if (pos < 18 || (pos > 63 && pos < 81)) { /// down
                    gen_addMove(moves, pos, pos + 9, captureOnly);
                }
                break;
            }

            case PieceTypeXq::advisor: {
                auto f = pos % 9;

                if (pos > 8) {
                    auto y = pos - 10; /* go left up */
                    if (f > 0 && (y == 3 || y == 13 || y == 66 || y == 76 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                    y = pos - 8; /* go right up */
                    if (f < 8 && (y == 5 || y == 13 || y == 68 || y == 76 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                }

                if (pos < 81) {
                    auto y = pos + 8; /* go left down */
                    if (f > 0 && (y == 13 || y == 21 || y == 84 || y == 76 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                    y = pos + 10; /* go right down */
                    if (f < 8 && (y == 13 || y == 23 || y == 76 || y == 86 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                }
                break;
            }

            case PieceTypeXq::elephant: {
                auto f = pos % 9;

                if (pos >= 18) {
                    auto y = pos - 20; /* go left up */

                    if (f > 1 && _isEmpty(pos - 10)
                        && (y == 2 || y == 6 || y == 18 || y == 22 || y == 47 || y == 51 || y == 63 || y == 67
                            || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                    y = pos - 16; /* go right up */
                    if (f < 7 && _isEmpty(pos - 8)
                        && (y == 2 || y == 6 || y == 22 || y == 26 || y == 47 || y == 51 || y == 67 || y == 71
                            || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                }

                if (pos < 72) {
                    auto y = pos + 16; /* go left down */
                    if (f > 1 && _isEmpty(pos + 8)
                        && (y == 18 || y == 22 || y == 38 || y == 42 || y == 63 || y == 67 || y == 83
                            || y == 87 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                    y = pos + 20; /* go right down */
                    if (f < 7 && _isEmpty(pos + 10)
                        && (y == 22 || y == 26 || y == 38 || y == 42 || y == 67 || y == 71 || y == 83
                            || y == 87 || COVERXQFreeBoundary)) {
                        gen_addMove(moves, pos, y, captureOnly);
                    }
                }
                break;
            }

            case PieceTypeXq::cannon: {
                auto col = pos % 9;
                /*
                 * go left
                 */
                auto f = 0;

                for (auto y = pos - 1; y >= pos - col; y--) {
                    if (_isEmpty(y)) {
                        if (f == 0 && !captureOnly) {
                            gen_addMove(moves, pos, y, captureOnly);
                        }
                        continue;
                    }
                    f++;
                    if (f == 2) {
                        gen_addMove(moves, pos, y, captureOnly);
                        break;
                    }
                }
                /*
                 * go right
                 */
                f = 0;
                for (auto y = pos + 1; y < pos - col + 9; y++) {
                    if (_isEmpty(y)) {
                        if (f == 0 && !captureOnly) {
                            gen_addMove(moves, pos, y, captureOnly);
                        }
                        continue;
                    }
                    f++;
                    if (f == 2) {
                        gen_addMove(moves, pos, y, captureOnly);
                        break;
                    }
                }

                f = 0;
                for (auto y = pos - 9; y >= 0; y -= 9) { /* go up */
                    if (_isEmpty(y)) {
                        if (f == 0 && !captureOnly) {
                            gen_addMove(moves, pos, y, captureOnly);
                        }
                        continue;
                    }
                    f += 1;
                    if (f == 2) {
                        gen_addMove(moves, pos, y, captureOnly);
                        break;
                    }
                }

                f = 0;
                for (auto y = pos + 9; y < 90; y += 9) { /* go down */
                    if (_isEmpty(y)) {
                        if (f == 0 && !captureOnly) {
                            gen_addMove(moves, pos, y, captureOnly);
                        }
                        continue;
                    }
                    f += 1;
                    if (f == 2) {
                        gen_addMove(moves, pos, y, captureOnly);
                        break;
                    }
                }

                break;
            }

            case PieceTypeXq::rook: {
                auto col = pos % 9;
                for (auto y = pos - 1; y >= pos - col; y--) { /* go left */
                    gen_addMove(moves, pos, y, captureOnly);
                    if (!_isEmpty(y)) {
                        break;
                    }
                }

                for (auto y = pos + 1; y < pos - col + 9; y++) { /* go right */
                    gen_addMove(moves, pos, y, captureOnly);
                    if (!_isEmpty(y)) {
                        break;
                    }
                }

                for (auto y = pos - 9; y >= 0; y -= 9) { /* go up */
                    gen_addMove(moves, pos, y, captureOnly);
                    if (!_isEmpty(y)) {
                        break;
                    }
                }

                for (auto y = pos + 9; y < 90; y += 9) { /* go down */
                    gen_addMove(moves, pos, y, captureOnly);
                    if (!_isEmpty(y)) {
                        break;
                    }
                }
                break;
            }

            case PieceTypeXq::horse: {
                auto col = pos % 9;
                auto y = pos - 11;
                auto z = pos - 1;
                if (y >= 0 && col > 1 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos - 19;
                z = pos - 9;
                if (y >= 0 && col > 0 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos - 17;
                z = pos - 9;
                if (y >= 0 && col < 8 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos - 7;
                z = pos + 1;
                if (y >= 0 && col < 7 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }

                y = pos + 7;
                z = pos - 1;
                if (y < 90 && col > 1 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos + 17;
                z = pos + 9;
                if (y < 90 && col > 0 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos + 19;
                z = pos + 9;
                if (y < 90 && col < 8 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                y = pos + 11;
                z = pos + 1;
                if (y < 90 && col < 7 && _isEmpty(z)) {
                    gen_addMove(moves, pos, y, captureOnly);
                }
                break;
            }

            case PieceTypeXq::pawn: {
                if ((side == Side::black && pos > 44) || (side == Side::white && pos < 45)) {
                    auto col = pos % 9;
                    /* go left */
                    if (col > 0) {
                        gen_addMove(moves, pos, pos - 1, captureOnly);
                    }
                    /* go right */
                    if (col < 8) {
                        gen_addMove(moves, pos, pos + 1, captureOnly);
                    }
                }

                if (side == Side::black) {
                    /* go down */
                    if (pos < 81) {
                        gen_addMove(moves, pos, pos + 9, captureOnly);
                    }
                } else {
                    /* go up */
                    if (pos > 8) {
                        gen_addMove(moves, pos, pos - 9, captureOnly);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void XqBoard::_make(const MoveFull& move, Hist& hist)
{
    assert(!pieces[move.from].isEmpty());
    assert(_isHashKeyValid());
    assert(move.piece == pieces[move.from]);
    assert(move.piece.idx == pieces[move.from].idx);

    hist.status = status;
    hist.move = move;
    hist.cap = pieces[move.dest];
    hist.hashKey = hashKey;
    hist.quietCnt = quietCnt;
    /// xiangqi has no castling, but the field is read back wherever a move is keyed
    hist.castled = 0;
    hist.enpassant = -1;

    auto sd = static_cast<int>(move.piece.side);
    assert(sd == 0 || sd == 1);

    if (!hist.cap.isEmpty() || (abs(move.from - move.dest) == 9 && move.piece.type == PAWNXQ)
        || move.piece.type == COVERXQ) {
        quietCnt = 0;
    } else {
        quietCnt++;
    }

    hashKey ^= xorHashKey(move.from);

    if (!hist.cap.isEmpty()) {
        hashKey ^= xorHashKey(move.dest);
        auto t = static_cast<int>(hist.cap.type);
        assert(pieceCount[1 - sd][t] > 0);
        pieceCount[1 - sd][t]--;
        capturedCount[1 - sd][t]++;
    }

    pieces[move.dest] = move.piece;
    pieces[move.from].setEmpty();

    assert(pieces[move.dest] == move.piece && pieces[move.dest].idx == move.piece.idx);

    hashKey ^= xorHashKey(move.dest);

    pieceList_make(hist);
}

void XqBoard::_takeBack(const Hist& hist)
{
    if (!hist.cap.isEmpty()) {
        auto t = static_cast<int>(hist.cap.type);
        auto xsd = static_cast<int>(hist.cap.side);
        pieceCount[xsd][t]++;
        capturedCount[xsd][t]--;
        assert(capturedCount[xsd][t] >= 0);
    }

    pieces[hist.move.from] = hist.move.piece;
    pieces[hist.move.dest] = hist.cap;
    quietCnt = hist.quietCnt;

    hashKey = hist.hashKey;

    pieceList_takeback(hist);
}

/// Check and make the move if it is legal
bool XqBoard::_checkMake(int from, int dest, int promotion)
{
    if (!MoveFull::isValid(from, dest)) {
        return false;
    }

    auto piece = _getPiece(from);
    if (piece.isEmpty() || piece.side != side || piece.side == _getPiece(dest).side
        || !Move::isValidPromotion(promotion)) {
        return false;
    }

    assert(isValid());

    /// Jieqi: revealing is not the mover's decision, so when no outcome is
    /// supplied the arbiter draws one. Every move arriving from a player comes
    /// in with EMPTY -- Game::make() strips whatever the engine claimed -- so
    /// this is the live path during a game.
    ///
    /// A supplied outcome is honoured instead of redrawn, which is what makes
    /// replaying a recorded game reproduce it: the PGN and the resume files
    /// carry the revelation, and _checkMake(hist) passes it back in here.
    if (variant == ChessVariant::jieqi && promotion == EMPTY && piece.type == COVERXQ) {
        promotion = drawRevelation(from, side);
        if (promotion == EMPTY) {
            assert(false);  /// a covered man standing with an empty bag cannot happen
            return false;
        }
    }

    auto moveList = _gen(side);

    for (auto&& move : moveList) {
        /// The revelation has to match for jieqi, or the first candidate the
        /// generator happened to emit would silently win -- which is what used
        /// to happen, making every reveal an advisor until the advisors ran
        /// out. Xiangqi has no promotions, so it is not filtered there.
        if (move.from != from || move.dest != dest
            || (variant == ChessVariant::jieqi && move.promotion != promotion)) {
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

        createSanStringForLastMove(moveFormat);

        assert(isValid());
        return true;
    }

    return false;
}

/// Check and make the move if it is legal
bool XqBoard::_quickCheckMake(int from, int dest, int promotion, bool createSanString)
{
    Move move(from, dest, promotion);
    if (!BoardCore::isValid(move)) {
        return false;
    }

    auto piece = _getPiece(from), cap = _getPiece(dest);
    if (piece.isEmpty() || piece.side != side || piece.side == cap.side) {
        return false;
    }

    auto checkMore = true;

    switch (static_cast<PieceTypeXq>(piece.type)) {
        case PieceTypeXq::king: {
            if (!canLocatePiece(KING, side, from)) {
                return false;
            }
            auto d = std::abs(from - dest);
            if (d != 1 && d != 9) {
                return false;
            }
            auto r = getRow(dest), f = getColumn(dest);
            if (f < 3 || f > 5 || (r > 2 && r < 7)) {
                return false;
            }
            break;
        }

        case PieceTypeXq::rook: {
            if (_quickCheck_rook(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeXq::cannon: {
            if (_quickCheck_cannon(from, dest, checkMore)) {
                break;
            }
            return false;
        }

        case PieceTypeXq::horse: {
            auto r = from / 9, f = from % 9, e = 0;

            switch (from - dest) {
                case 19:
                    if (f == 0 || r < 2) {
                        return false;
                    }
                    e = -9;
                    break;

                case 17:
                    if (f == 8 || r < 2) {
                        return false;
                    }
                    e = -9;
                    break;

                case 11:
                    if (f < 2 || r < 1) {
                        return false;
                    }
                    e = -1;
                    break;
                case 7:
                    if (f > 6 || r < 1) {
                        return false;
                    }
                    e = 1;
                    break;

                case -7:
                    if (f < 2 || r > 8) {
                        return false;
                    }
                    e = -1;
                    break;

                case -11:
                    if (f > 6 || r > 8) {
                        return false;
                    }
                    e = 1;
                    break;

                case -17:
                    if (f == 0 || r > 7) {
                        return false;
                    }
                    e = 9;
                    break;

                case -19:
                    if (f == 8 || r > 7) {
                        return false;
                    }
                    e = 9;
                    break;
                default:
                    return false;
            }

            if (!_isEmpty(from + e)) {
                return false;
            }
            break;
        }

        case PieceTypeXq::advisor: {
            if (!canLocatePiece(static_cast<int>(PieceTypeXq::advisor), side, from)) {
                return false;
            }
            auto d = std::abs(from - dest);
            if (d != 8 && d != 10) {
                return false;
            }

            if (variant == ChessVariant::xq) {
                auto r = getRow(dest), f = getColumn(dest);
                if (f < 3 || f > 5 || (r > 2 && r < 7)) {
                    return false;
                }
            } else {
                if (std::abs(getRow(from) - getRow(dest)) != 1
                    || std::abs(getColumn(from) - getColumn(dest)) != 1) {
                    return false;
                }
            }
            break;
        }
        case PieceTypeXq::elephant: {
            if (!canLocatePiece(static_cast<int>(PieceTypeXq::elephant), side, from)) {
                return false;
            }
            auto d = std::abs(from - dest);
            if ((d != 16 && d != 20) || !_isEmpty((from + dest) / 2)) {
                return false;
            }

            if (variant == ChessVariant::xq) {
                if ((side == Side::white && dest < 45) || (side == Side::black && dest > 45)) {
                    return false;
                }
            } else {
                if (std::abs(getRow(from) - getRow(dest)) != 2
                    || std::abs(getColumn(from) - getColumn(dest)) != 2) {
                    return false;
                }
            }

            break;
        }
        case PieceTypeXq::pawn: {
            if (!canLocatePiece(static_cast<int>(PieceTypeXq::pawn), side, from)) {
                return false;
            }
            auto d = std::abs(from - dest);
            if (d != 1 && d != 9) {
                return false;
            }

            auto rf = getRow(from), rd = getRow(dest);
            if (rf != rd && d == 1) {
                return false;
            }

            if (side == Side::white) {
                if ((d == 9 && from < dest) || (d == 1 && from > 44)) {
                    return false;
                }
            } else {
                if ((d == 9 && from > dest) || (d == 1 && from < 45)) {
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
        createSanStringForLastMove(moveFormat);
    }

#ifdef _COVERXQ_
    if (variant == ChessVariant::COVERXQ) {
        return isValid();
    }
#endif

    assert(isValid());
    return true;
}

bool XqBoard::_quickCheck_cannon(int from, int dest, bool checkMiddle) const
{
    /// same row
    if (getRow(from) == getRow(dest)) {
        if (checkMiddle) {
            auto b = 0, d = from < dest ? +1 : -1;
            for (auto i = from + d, step = 0; i != dest; i += d, step++) {
                assert(step < 8);
                if (!_isEmpty(i)) {
                    b++;
                    if (b > 1) {
                        return false;
                    }
                }
            }

            if (b == 1) {
                auto destside = _getPiece(dest).side;
                if (destside == Side::none || _getPiece(from).side == destside) {
                    return false;
                }
            } else if (!_isEmpty(dest)) {
                /// no screen: a cannon takes nothing (2026-11-19: "c6c7" over the next point was
                /// taken as a capture, so games with such moves loaded; Pikafish 2026 refuses them)
                return false;
            }
        }
        return true;
    }

    if (getColumn(from) != getColumn(dest)) {
        return false;
    }

    /// same file
    if (checkMiddle) {
        auto c = columnCount();
        auto b = 0, d = from < dest ? c : -c;
        for (auto i = from + d, step = 0; i != dest; i += d, step++) {
            assert(step < 8);
            if (!_isEmpty(i)) {
                b++;
                if (b > 1) {
                    return false;
                }
            }
        }
        if (b == 1) {
            auto destside = _getPiece(dest).side;
            if (destside == Side::none || _getPiece(from).side == destside) {
                return false;
            }
        } else if (!_isEmpty(dest)) {
            return false; /// no screen: no capture (see above)
        }
    }

    return true;
}

std::string XqBoard::toString(const Piece& piece) const
{
    return piece2String(piece);
}

std::string XqBoard::piece2String(const Piece& piece)
{
    auto ch = xqPieceType2Char(piece.type);
    if (piece.side == Side::white) {
        ch += 'A' - 'a';
    }
    return std::string(1, ch);
}

std::string XqBoard::toString(const MoveFull& move, const MoveFormat& mf) const
{
    return toString(Move(move), mf);
}


std::string XqBoard::toString(const Move& move, const MoveFormat& mf) const
{
    return moveString_coordinate(move, mf);
}

std::vector<Move> XqBoard::coordinateString2Moves(const std::string& str, const MoveFormat& mf)
{
    std::vector<Move> moves;
    auto ss = replaceString(str, ".", " ");
    auto list = splitString(ss, ' ');
    for (auto&& s : list) {
        auto ch = s.at(0);
        if (isdigit(ch)) continue;

        Move m = xqMoveFromCoordiateString(s, mf);
        moves.push_back(m);
    }

    return moves;
}

std::string XqBoard::toString_coordinate(const MoveFull& move, const MoveFormat& mf) const
{
    return moveString_coordinate(move, mf);
}

std::string XqBoard::toString_traditional(const MoveFull& move, Notation notation, const MoveFormat& mf) const
{
    return moveString_traditional(move, notation, mf);
}

std::string XqBoard::toString_san(const Hist& hist) const
{
    return moveString_san(hist);
}


std::string XqBoard::toString_san(const Hist& hist, const MoveFormat& mf) const
{
    return getSanString(hist, mf);
}

std::string XqBoard::hist2String(const HistBasic& hist, Notation notation, const MoveFormat& mf)
{
    if (notation == Notation::san) {
        return moveString_san(hist);
    }
    if (notation == Notation::algebraic_coordinate) {
        return moveString_coordinate(hist.move, mf);
    }
    return moveString_traditional(hist.move, notation, mf);
}

std::string XqBoard::moveString_san(const HistBasic& hist)
{
    return hist.sanString;
}


std::string XqBoard::moveString_coordinate(const Move& move, const MoveFormat& mf)
{
    std::string s = xqPosToCoordinateString(move.from, mf) + xqPosToCoordinateString(move.dest, mf);
    if (move.promotion != EMPTY) {
        /// The revealed type, lower case whichever side revealed it -- the
        /// jieqi notation writes "b9c7c", not "b9c7C". Piece(.., Side::white)
        /// here upper-cased it, which no reader following the reference
        /// expects. Reading is unaffected: xqCharactorToPieceType() folds case.
        s += piece2String(Piece(move.promotion, Side::black));
    }
    return s;
}

std::string XqBoard::moveString_traditional(const MoveFull& move, Notation notation, const MoveFormat& /*mf*/)
{
    auto notationIdx = static_cast<int>(notation);

    auto moveType = move.piece.type;
    if (moveType == COVERXQ) {
        moveType = originalBoard()[move.from].type;
    }
    std::string str = notationPieceNames[notationIdx][moveType];

    auto f = move.from % 9;
    auto ff = move.piece.side == Side::black ? f : (8 - f);
    str += '1' + ff;

    auto t = static_cast<PieceTypeXq>(moveType);
    if (t == PieceTypeXq::advisor || t == PieceTypeXq::elephant || t == PieceTypeXq::horse) {
        str += (move.piece.side == Side::white && move.dest < move.from)
                               || (move.piece.side == Side::black && move.dest > move.from)
                       ? advanceNames[notationIdx][0]
                       : advanceNames[notationIdx][1];
        auto c = move.dest % 9;
        auto cc = move.piece.side == Side::black ? c : (8 - c);
        str += '1' + cc;
    } else {
        if (move.dest / 9 == move.from / 9) {
            str += advanceNames[notationIdx][2];
            auto c = move.dest % 9;
            auto cc = move.piece.side == Side::black ? c : (8 - c);
            str += '1' + cc;
        } else {
            str += (move.piece.side == Side::white && move.dest < move.from)
                                   || (move.piece.side == Side::black && move.dest > move.from)
                           ? advanceNames[notationIdx][0]
                           : advanceNames[notationIdx][1];
            str += '0' + std::abs(move.from / 9 - move.dest / 9);
        }
    }

    /// Jieqi: a face-down man's move ends with what it revealed, "C3-5=P"
    /// The readers (jieqiMoveFromTraditionalString,
    /// moveFromTraditionalString) take the same "=X" ending.
    if (move.promotion > KING && move.promotion < COVERXQ) {
        std::string name = notationPieceNames[notationIdx][move.promotion];
        while (!name.empty() && name.back() == ' ') {
            name.pop_back();
        }
        str += "=" + name;
    }

    return str;
}


Move XqBoard::moveFromString_san(const std::string& str, const MoveFormat& mf)
{
    std::string s;
    for (auto ch : str) {
        if (ch != '+' && ch != 'x' && ch != '*' && ch != '#') {
            if (ch < ' ' || ch == '.') ch = ' ';
            s += ch;
        }
    }

    auto from = -1, dest = -1, fromCol = -1, fromRow = -1;
    auto promotion = EMPTY;

    auto p = s.find("=");
    if (p != std::string::npos) {
        if (s.length() == p + 1) {
            return Move::illegalMove; /// something wrong
        }
        char ch = s.at(p + 1);
        promotion = xqCharactorToPieceType(ch);

        s = s.substr(0, p);
        if (s.size() < 2 || promotion == EMPTY) {
            return Move::illegalMove;
            ;
        }
    }

    int n = s.length(), t = 2;
    /// as in chess: fewer than two characters cannot hold a destination (substr would throw)
    if (n < t) {
        return Move::illegalMove;
    }
    auto destString = s.substr(n - t, t);
    if (mf.rankFrom1 && destString == "10" && n > 2) { /// Rb10
        t = 3;
        destString = s.substr(n - t, t);
    }
    dest = xqCoordinateStringToPos(destString.c_str(), mf);

    if (!isPositionValid(dest)) {
        return Move::illegalMove;
        ;
    }

    if (n > t) {
        s = s.substr(0, n - t);
    } else {
        s = "";
    }

    auto pieceType = static_cast<int>(PieceTypeXq::pawn);

    if (!s.empty()) {
        size_t k = 0;
        char ch = s.at(0);
        if (ch >= 'A' && ch <= 'Z') {
            k++;
            pieceType = xqCharactorToPieceType(ch);

            if (pieceType == EMPTY) {
                return Move::illegalMove;
                ;
            }
        }

        auto left = s.length() - k;
        if (left > 0) {
            s = s.substr(k, left);
            if (left == 2) {
                from = xqCoordinateStringToPos(s.c_str(), mf);
            } else {
                char ch = s.at(0);
                if (isdigit(ch)) {
                    fromRow = 9 - ch + '0';

                    if (mf.rankFrom1 && fromRow > 0) {
                        fromRow++; /// careful: because of minus 9 -
                    }
                } else if (ch >= 'a' && ch <= 'z') {
                    fromCol = ch - 'a';
                }
            }
        }
    }

    /// Xiangqi has no promotion; only a jieqi cover piece "promotes" (turns into what it is). A
    /// damaged move such as "b2a2=b5f5" is an illegal move, not an assert.
    if (!((pieceType != COVERXQ && promotion == EMPTY) || (pieceType == COVERXQ && promotion > KING))) {
        return Move::illegalMove;
    }

    if (from < 0) {
        std::vector<MoveFull> moveList;
        _gen(moveList, side, pieceType);

        std::vector<Move> goodMoves;
        for (auto&& m : moveList) {
            if (m.dest != dest || m.promotion != static_cast<int>(promotion)
                || _getPiece(m.from).type != static_cast<int>(pieceType)) {
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
                MoveFull move = createFullMove(m.from, dest, static_cast<int>(promotion));
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
        }
    }

    return Move(from, dest, static_cast<int>(promotion));
}


int XqBoard::toPieceCount(int* pieceCnt) const
{
    if (pieceCnt) {
        memset(pieceCnt, 0, 14 * sizeof(int));
    }
    auto totalCnt = 0;
    ;
    for (auto i = 0; i < 90; i++) {
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

Move XqBoard::moveFromString_xq(const std::string& s, const MoveFormat& mf)
{
    return moveFromTraditionalString(s, mf);
}

Move XqBoard::moveFromTraditionalString(const std::string& s, const MoveFormat& mf)
{
    if (s.length() < 3) {
        return Move::illegalMove;
    }

    auto promoting = false; /// for COVERXQ only

    if (variant == ChessVariant::jieqi) {
        auto ch = s.at(s.length() - 1);
        if (std::isalpha(ch)) {
            auto pt = xqCharactorToPieceType(ch);
            if (pt > KING && pt < COVERXQ) {
                ch = s.at(s.length() - 2);
                promoting = !std::isalnum(ch);
            }
        }
    }

    unsigned long i = 0;
    char ch = s.at(i);

    auto pieceType = static_cast<int>(PieceTypeXq::pawn);
    if (isalpha(ch)) {
        i++;
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }

        pieceType = xqCharactorToPieceType(ch);

        if (pieceType < KING || pieceType > COVERXQ) {
            return Move::illegalMove;
        }
    } else {
        return Move::illegalMove;
    }

    auto from = -1;

    if (pieceType == KING) {
        from = findKing(side);
        if (isdigit(s.at(i))) {
            i++;
        }
    } else {
        int fromCol = -1;
        char ch = s.at(i);
        i++;
        if (isdigit(ch)) {
            fromCol = ch - '0';
            if (fromCol < 1 || fromCol > 9) {
                return Move::illegalMove;
            }
            fromCol--;
            if (side == Side::white) {
                fromCol = 8 - fromCol;
            }

            /// Solve some ambiguous cases
            if (variant == ChessVariant::xq) {
                if (pieceType == static_cast<int>(PieceTypeXq::advisor)) {
                    if (fromCol == 4) {
                        from = side == Side::white ? 76 : 13;
                    } else {
                        char ch2 = s.at(i);
                        if (ch2 == mf.xqTra_advance || ch2 == mf.xqTra_backward) {
                            if (side == Side::white) {
                                from = fromCol == 3 ? (ch2 == mf.xqTra_advance ? 84 : 66)
                                                    : (ch2 == mf.xqTra_advance ? 86 : 68);
                            } else {
                                from = fromCol == 3 ? (ch2 == mf.xqTra_advance ? 3 : 21)
                                                    : (ch2 == mf.xqTra_advance ? 5 : 23);
                            }
                        }
                    }
                }
                if (pieceType == static_cast<int>(PieceTypeXq::elephant)) {
                    switch (fromCol) {
                        case 4:
                            from = side == Side::white ? 67 : 22;
                            break;
                        case 0:
                            from = side == Side::white ? 63 : 18;
                            break;
                        case 8:
                            from = side == Side::white ? 71 : 26;
                            break;

                        case 2:
                        case 6: {
                            char ch2 = s.at(i);
                            if (ch2 == mf.xqTra_advance || ch2 == mf.xqTra_backward) {
                                if (side == Side::white) {
                                    from = fromCol == 2 ? (ch2 == mf.xqTra_advance ? 83 : 47)
                                                        : (ch2 == mf.xqTra_advance ? 87 : 51);
                                } else {
                                    from = fromCol == 2 ? (ch2 == mf.xqTra_advance ? 2 : 38)
                                                        : (ch2 == mf.xqTra_advance ? 6 : 42);
                                }
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
            }
        }

        if (from < 0) {
            from = find(pieceType, fromCol, ch, promoting);
        }
    }

    if (from < 0 || from > 89 || i + 2 > s.length()
        || (_getPiece(from).type != pieceType && _getPiece(from).type != COVERXQ)) {
        return Move::illegalMove;
    }

    assert(_getPiece(from).type == pieceType || _getPiece(from).type == COVERXQ);

    char dir = s.at(i);
    i++;

    char target = s.at(i);
    if (!isdigit(target)) {
        return Move::illegalMove;
    }

    int numb = target - '0';

    auto toCol = numb - 1;
    if (side == Side::white) {
        toCol = 8 - toCol;
    }

    auto dest = -1;
    if (dir == mf.xqTra_sideway || dir == '.') {
        dest = from - (from % 9) + toCol;
    } else if (dir == mf.xqTra_advance || dir == mf.xqTra_backward) {
        if (pieceType == static_cast<int>(PieceTypeXq::advisor)
            || pieceType == static_cast<int>(PieceTypeXq::elephant)
            || pieceType == static_cast<int>(PieceTypeXq::horse)) {
        } else {
            dest = from
                   + (((side == Side::white && dir == mf.xqTra_advance)
                       || (side == Side::black && dir == mf.xqTra_backward))
                              ? -9
                              : 9)
                             * numb;
        }
    } else {
        return Move::illegalMove;
    }

    auto moveList = _gen(side);

    for (size_t j = 0; j < moveList.size(); j++) {
        auto move = moveList[j];
        if (move.from != from) {
            continue;
        }

        if ((dest >= 0 && move.dest == dest) || (dest < 0 && move.dest % 9 == toCol)) {
            if (dest < 0) {
                auto smaller = move.from > move.dest;
                if (side == Side::white) {
                    if ((dir == mf.xqTra_advance && !smaller) || (dir == mf.xqTra_backward && smaller)) {
                        continue;
                    }
                } else {
                    if ((dir == mf.xqTra_advance && smaller) || (dir == mf.xqTra_backward && !smaller)) {
                        continue;
                    }
                }
            }

            /// P1+1=R pieceType may be not COVERXQ
            /// Bug: K5.4=A
            auto promotion = EMPTY;
            if (variant == ChessVariant::jieqi && i + 2 <= s.length() && pieceType > KING) {
                char ch = s.at(i + 2);
                if (ch >= 'A' && ch < 'Z') {
                    ch += 'a' - 'A';
                }
                promotion = xqCharactorToPieceType(ch);
            }

            if (move.dest >= 0) {
                return Move(move.from, move.dest, promotion);
            }
        }
    }

    return Move::illegalMove;
}

int XqBoard::find(int pieceType, int atCol, char relative, bool promoting) const
{
    auto from = 89, add = 1;
    if (atCol >= 0) {
        from = 81 + atCol;
        add = 9;
    }

    int array[18], k = 0, COVERXQCnt = 0, lastCOVERXQ = -1;
    for (int i = from; i >= 0; i -= add) {
        auto piece = _getPiece(i);
        if (piece.side != side) {
            continue;
        }
        if (piece.type == static_cast<int>(pieceType)
            || (piece.type == COVERXQ && originalBoard()[i].type == static_cast<int>(pieceType))) {
            array[k++] = i;

            if (piece.type == COVERXQ) {
                COVERXQCnt++;
                lastCOVERXQ = i;
            }
        }
    }

    if (k == 0) {
        return -1;
    }

    auto x = 0;

    if (k > 1) {
        if (promoting && COVERXQCnt == 1) {
            return lastCOVERXQ;
        }
        if (!promoting && k - COVERXQCnt == 1) {
            for (auto i = 0; i < k; i++) {
                auto pos = array[i];
                auto piece = _getPiece(pos);
                if (piece.type != COVERXQ) {
                    return pos;
                }
            }
        }

        if (atCol < 0 && (relative == '+' || relative == '-' || relative == '~')) {
            int colCnt[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            int midCnt[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
            int maxNum = 0, maxCol = 0;
            for (auto i = 0; i < k; i++) {
                assert(array[i] >= 0 && array[i] < 90);
                auto f = array[i] % 9;
                colCnt[f]++;
                if (colCnt[f] == 2) {
                    midCnt[f] = array[i];
                }

                if (colCnt[f] > maxNum) {
                    maxNum = colCnt[f];
                    maxCol = f;
                }
            }

            if (promoting) {
                for (auto i = 0; i < k; i++) {
                    int pos = array[i];
                    int f = pos % 9;
                    if (f == maxCol) {
                        if (_getPiece(pos).type == COVERXQ) {
                            return pos;
                        }
                    }
                }
            }

            if (relative == '~') {
                return midCnt[maxCol];
            }

            int first = -1, last = -1;
            for (auto i = 0; i < k; i++) {
                auto f = array[i] % 9;
                if (f == maxCol) {
                    if (first < 0) {
                        first = array[i];
                    }
                    last = array[i];
                }
            }
            if ((side == Side::white && relative == '+') || (side == Side::black && relative == '-')) {
                return last;
            }
            return first;

        } else {
            if ((side == Side::white && relative == '+') || (side == Side::black && relative == '-')) {
                x = k - 1;
            }
        }
    }


    return array[x];
}

void XqBoard::findAmbiguous(std::vector<Move>& moves, int pieceType, int atCol, char relative,
                            int promoteType)
{
    auto from = 0, add = 1;
    if (atCol >= 0) {
        from = atCol;
        add = 9;
    }

    int array[18], k = 0, COVERXQCnt = 0, lastCOVERXQ = -1;
    for (auto i = from; i < 90; i += add) {
        auto piece = _getPiece(i);
        if (piece.side != side) {
            continue;
        }
        if (piece.type == static_cast<int>(pieceType)
            || (piece.type == COVERXQ && originalBoard()[i].type == static_cast<int>(pieceType))) {
            array[k++] = i;

            if (piece.type == COVERXQ) {
                COVERXQCnt++;
                lastCOVERXQ = i;
            }
        }
    }

    if (k == 0) {
        return;
    }

    if (k == 1) {
        Move move(array[0], -1, static_cast<int>(promoteType));
        moves.push_back(move);
        return;
    }

    if (promoteType != EMPTY && COVERXQCnt == 1) {
        Move move(lastCOVERXQ, -1, static_cast<int>(promoteType));
        moves.push_back(move);
        return;
    }

    if (relative == ' ') {
        for (auto i = 0; i < k; i++) {
            auto pos = array[i];
            auto piece = _getPiece(pos);
            if ((promoteType == EMPTY && piece.type != COVERXQ)
                || (promoteType != EMPTY && piece.type != COVERXQ)) {
                Move move(pos, -1, static_cast<int>(promoteType));
                moves.push_back(move);
            }
        }
        return;
    }

    assert(atCol < 0 && relative != ' ');

    int colCnt[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int posArr[10][15];

    for (auto i = 0; i < k; i++) {
        assert(array[i] >= 0 && array[i] < 90);
        auto pos = array[i];
        auto f = pos % 9;
        posArr[f][colCnt[f]++] = pos;
    }

    for (auto f = 0; f < 9; f++) {
        if (colCnt[f] < 2) {
            continue;
        }
        switch (relative) {
            case '+': {
                auto x = side == Side::black ? colCnt[f] - 1 : 0;
                auto from = posArr[f][x];
                Move move(from, -1, static_cast<int>(promoteType));
                moves.push_back(move);
                break;
            }
            case '-': {
                auto x = side == Side::white ? colCnt[f] - 1 : 0;
                auto from = posArr[f][x];
                Move move(from, -1, static_cast<int>(promoteType));
                moves.push_back(move);
                break;
            }
            case '~': {
                if (colCnt[f] > 2) {
                    auto x = 1;
                    auto from = posArr[f][x];
                    Move move(from, -1, static_cast<int>(promoteType));
                    moves.push_back(move);
                    break;
                } else { /// ambiguous
                    for (auto j = 0; j < colCnt[f]; j++) {
                        auto from = posArr[f][j];
                        Move move(from, -1, static_cast<int>(promoteType));
                        moves.push_back(move);
                    }
                }
            }
        }
    }
}


int XqBoard::getRawMaterialScore() const
{
    static const int rawMaterials[] = { 0, 0, 9, 5, 3, 2, 2, 1 };
    auto score = 0;
    for (auto i = 0; i < 64; i++) {
        auto piece = _getPiece(i);
        if (piece.isEmpty()) continue;
        auto s = rawMaterials[static_cast<int>(piece.type)];
        score += piece.side == side ? s : -s;
    }
    return score;
}

int XqBoard::availableRevelations(Side side, PieceTypeXq revelation) const
{
    if (revelation > PieceTypeXq::king && revelation <= PieceTypeXq::pawn) {
        auto sd = static_cast<int>(side);
        auto t = static_cast<int>(revelation);
        auto a = maxPieceCntsByTypes[t] - pieceCount[sd][t] - capturedCount[sd][t];
        assert(a >= 0 && a <= 5);
        return a;
    }
    return 0;
}



Move XqBoard::xqMoveFromCoordiateString(const std::string& s, const MoveFormat& mf)
{
    if (s.length() < 4) {
        return Move::illegalMove;
    }

    /// A square is two characters, except that rank 10 is written "a10" when
    /// ranks are numbered from 1. Both offsets have to follow from that, or
    /// the revelation suffix is looked for in the wrong place (the "c" of
    /// "b9c7c", the very form moveString_coordinate() writes).
    auto squareLen = [&mf](int pos) {
        return (mf.rankFrom1 && pos >= 0 && pos < 9) ? 3 : 2;
    };

    int from = xqCoordinateStringToPos(s, mf);
    auto k = squareLen(from);
    if (s.length() < static_cast<size_t>(k) + 2) {
        return Move::illegalMove;
    }

    int dest = xqCoordinateStringToPos(s.substr(k), mf);
    auto p = static_cast<size_t>(k) + squareLen(dest);

    auto promotion = EMPTY;
    if (s.length() > p) {
        char ch = s.at(p);
        /// SAN spells a revelation "=C"; the long algebraic form just appends it
        if (ch == '=' && s.length() > p + 1) {
            ch = s.at(p + 1);
        }
        if (ch >= 'A' && ch <= 'Z') {
            ch += 'a' - 'A';
        }
        promotion = xqCharactorToPieceType(ch);
    }

    return Move(from, dest, static_cast<int>(promotion));
}

#define HAVE_COMMENT_BIT (1 << 15)


int XqBoard::attackerCnt() const
{
    auto cnt = 0;
    for (auto&& piece : pieces) {
        if (!piece.isEmpty() && piece.type >= static_cast<int>(PieceTypeXq::rook)) cnt++;
    }
    return cnt;
}

bool XqBoard::hasAttackers() const
{
    for (auto i = 5; i < 16; ++i) { /// from the rooks: king, 2 advisors and 2 elephants ignored
        if (pieceList[0][i] >= 0 || pieceList[1][i] >= 0) {
            return true;
        }
    }
    return false;
}

/// Using 2 digits for the first rank, say, e10f10.
size_t XqBoard::coordinateFromLength(int from, const MoveFormat& mf) const
{
    return mf.rankFrom1 && from < 9 ? 3 : 2;
}

/// Settle from the move list itself whether its ranks are written 0-9 or 1-10:
/// a "0" alone, or a "10", says so; otherwise the first move is tried both ways.
void XqBoard::detectMoveFormat(const std::vector<MoveStringLevel>& moveStringVec, MoveFormat& mf)
{
    if (moveStringVec.empty()) {
        return;
    }

    for (auto&& s : moveStringVec) {
        if (s.moveString.find('0') != std::string::npos) {
            mf.rankFrom1 = s.moveString.find("10") != std::string::npos;
            return;
        }
    }

    auto ss = moveStringVec.front().moveString;
    Move move = Move::illegalMove;

    move = moveFromString_san(ss, mf);
    if (!isValid(move)) {
        move = moveFromString_xq(ss, mf);
    }
    if (!isValid(move)) {
        move = moveFromString_coordinate(ss, mf);
    }

    auto ok = isValid(move);
    if (ok) {
        auto piece = _getPiece(move.from);
        auto cap = _getPiece(move.dest);
        ok = piece.side == side && piece.side != cap.side;
    }
    if (!ok) {
        mf.rankFrom1 = !mf.rankFrom1;
    }
}

/// OCGDB: no endgame tablebases (see probeEgtb).
std::vector<MoveFull> XqBoard::probeEgtbAll() const
{
    return std::vector<MoveFull>();
}

void XqBoard::createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const
{
    if (m.piece.type == COVERXQ) {
        assert(_getPiece(m.from).side == m.piece.side);
        auto sd = static_cast<int>(m.piece.side);
        for (int r = KING + 1; r < COVERXQ; ++r) {
            auto a = maxPieceCntsByTypes[r] - pieceCount[sd][r] - capturedCount[sd][r];
            assert(a >= 0 && a <= 5);

            if (a > 0) {
                m.promotion = r;
                moveList.push_back(m);
            }
        }
    } else {
        moveList.push_back(m);
    }
}

static const int flip_h[90]
        = { 8,  7,  6,  5,  4,  3,  2,  1,  0,  17, 16, 15, 14, 13, 12, 11, 10, 9,  26, 25, 24, 23, 22,
            21, 20, 19, 18, 35, 34, 33, 32, 31, 30, 29, 28, 27, 44, 43, 42, 41, 40, 39, 38, 37, 36, 53,
            52, 51, 50, 49, 48, 47, 46, 45, 62, 61, 60, 59, 58, 57, 56, 55, 54, 71, 70, 69, 68, 67, 66,
            65, 64, 63, 80, 79, 78, 77, 76, 75, 74, 73, 72, 89, 88, 87, 86, 85, 84, 83, 82, 81 };

static const int flip_v[90]
        = { 81, 82, 83, 84, 85, 86, 87, 88, 89, 72, 73, 74, 75, 76, 77, 78, 79, 80, 63, 64, 65, 66, 67,
            68, 69, 70, 71, 54, 55, 56, 57, 58, 59, 60, 61, 62, 45, 46, 47, 48, 49, 50, 51, 52, 53,

            36, 37, 38, 39, 40, 41, 42, 43, 44, 27, 28, 29, 30, 31, 32, 33, 34, 35, 18, 19, 20, 21, 22,
            23, 24, 25, 26, 9,  10, 11, 12, 13, 14, 15, 16, 17, 0,  1,  2,  3,  4,  5,  6,  7,  8 };


int XqBoard::xqflip(int pos, FlipMode flipMode)
{
    if (pos >= 0) {
        switch (flipMode) {
            case FlipMode::none:
                return pos;
            case FlipMode::horizontal:
                return flip_h[pos];
            case FlipMode::vertical:
                return flip_v[pos];
            case FlipMode::rotate:
                return 89 - pos; // around
                //        default:
        }
    }
    return -1;
}

int XqBoard::flip(int pos, FlipMode flipMode) const
{
    return xqflip(pos, flipMode);
}

/// OCGDB: the Asian-rule judge of perpetual checks and chases (Banksia's xqchasejudge.cpp)
/// needs the tournament manager's Game class and is not part of this program; a database stores
/// the results the games were given and never judges a repetition.
Result XqBoard::ruleRepetition(int)
{
    return Result();
}

Result XqBoard::rule()
{
    auto result = BoardCore::rule();
    if (!result.isNone()) {
        return result;
    }

    std::lock_guard<std::mutex> dolock(dataMutex);

    /// Insufficient mating material
    auto attackerCnt = 0;
    for (auto t = ROOKXQ; t <= COVERXQ; t++) {
        attackerCnt += pieceCount[0][t] + pieceCount[1][t];
    }

    if (attackerCnt == 1) {
        /// case KCvsK (without Advisor of the C side, with/without Elephant) is a draw
        if (pieceCount[0][CANNON] + pieceCount[1][CANNON] == 1) {
            auto sd = pieceCount[0][CANNON] ? 0 : 1;
            if (pieceCount[sd][ADVISOR] == 0 && pieceCount[1 - sd][ADVISOR] < 2) {
                attackerCnt = 0;
            }
        } else
            /// old Pawn is a draw
            if (pieceCount[0][PAWNXQ] + pieceCount[1][PAWNXQ] == 1) {
                auto sd = pieceCount[0][PAWNXQ] ? 0 : 1;
                auto from = 0, to = 9;
                if (sd == B) {
                    from = 81;
                    to = 90;
                }

                for (int i = from; i < to; ++i) {
                    if (pieces[i].type == PAWNXQ) {
                        attackerCnt = 0;
                        break;
                    }
                }
            }
    }

    if (!attackerCnt) {
        result.result = ResultType::draw;
        result.reason = ReasonType::insufficientmaterial;
        return result;
    }

    // Repetition
    return result;
}

bool XqBoard::isFenValid(const std::string& fen) const
{
    return isXqFenValid(fen);
}

bool XqBoard::isXqFenValid(const std::string& fen)
{
    if (fen.length() < 10) {
        return false;
    }

    int pieceCnt[2][10];
    memset(pieceCnt, 0, sizeof(pieceCnt));

    auto vec = splitString(fen, ' ');
    auto frontString = vec.front();
    auto thefenVec = splitString(frontString, '[');
    auto thefen = thefenVec.front();

    auto errCnt = 0, pos = 0;
    for (size_t i = 0; i < thefen.length(); i++) {
        char ch = thefen.at(i);

        if (ch == '/') {
            continue;
        }

        if (ch >= '0' && ch <= '9') {
            int num = ch - '0';
            pos += num;
            continue;
        }

        auto side = Side::black;
        if (ch >= 'A' && ch < 'Z') {
            side = Side::white;
            ch += 'a' - 'A';
        }

        auto pieceType = xqCharactorToPieceType(ch);
        /// COVERXQ ('x') is a real piece in jieqi, not an error.
        if (pieceType <= EMPTY || (pieceType > PAWNXQ && pieceType != COVERXQ)) {
            errCnt++;
        } else {
            pieceCnt[static_cast<int>(side)][static_cast<int>(pieceType)]++;
        }
        pos++;
    }

    if (thefenVec.size() > 1) {
        auto capturedString = thefenVec.at(1);
        for (size_t i = 0; i < capturedString.length(); i++) {
            char ch = capturedString.at(i);
            if (ch == ']') break;
            if (ch >= '0' && ch <= '9') continue;
            if (ch >= 'A' && ch < 'Z') {
                ch += 'a' - 'A';
            }
            auto pieceType = xqCharactorToPieceType(ch);
            if (pieceType == EMPTY) {
                errCnt++;
            }
        }
    }

    /// A xiangqi position covers exactly 90 squares and is spelled with xiangqi
    /// letters. A chess FEN (64 squares, two queens) must not reach the board,
    /// whose piece list would no longer match its squares.
    if (pos != 90 || errCnt != 0 || pieceCnt[0][KING] != 1 || pieceCnt[1][KING] != 1) {
        return false;
    }

    /// How many pieces of a type a side can have. The piece list holds exactly this many (see
    /// pieceList_setPiece), so a position with more of them - three Advisors, say - cannot be
    /// represented: the list and the squares then disagree and the first move on the piece that
    /// did not fit corrupted memory. The chess side of the house has always checked this.
    for (auto sd = 0; sd < 2; sd++) {
        if (pieceCnt[sd][ADVISOR] > 2 || pieceCnt[sd][ELEPHANT] > 2 || pieceCnt[sd][ROOKXQ] > 2
            || pieceCnt[sd][CANNON] > 2 || pieceCnt[sd][HORSE] > 2 || pieceCnt[sd][PAWNXQ] > 5) {
            return false;
        }
    }
    return true;
}




/// empty, king, advisor, elephant, rook, cannon, horse, pawn, COVERXQ
static const int seePieceVals[] = { 0, 10000, 200, 200, 800, 400, 400, 100, 0 };

static int see_pieceVal(int type, int /*dest*/)
{
    return seePieceVals[type];
}

std::vector<int> XqBoard::see_genAttack(Side attackerSide, int pos, int posType, const int* usedSet) const
{
    std::map<int, std::vector<int>> theMap {
        { 0, std::vector<int>() }, { 1, std::vector<int>() }, { 2, std::vector<int>() },
        { 3, std::vector<int>() }, { 4, std::vector<int>() }, { 5, std::vector<int>() },
        { 6, std::vector<int>() }, { 7, std::vector<int>() }, { 8, std::vector<int>() },
    };

    auto row = getRow(pos), col = getColumn(pos);

    /// Knight
    {
        auto horse = static_cast<int>(PieceTypeXq::horse);
        const Piece movePiece(horse, attackerSide);
        if (row) {
            if (row > 1) {
                if (col > 0 && _getPiece(pos - 19) == movePiece && !usedSet[pos - 19]
                    && (_isEmpty(pos - 10) || usedSet[pos - 10]))
                    theMap[horse].push_back(pos - 19);
                if (col < 8 && _getPiece(pos - 17) == movePiece && !usedSet[pos - 17]
                    && (_isEmpty(pos - 8) || usedSet[pos - 8]))
                    theMap[horse].push_back(pos - 17);
            }

            if (col > 1 && _getPiece(pos - 11) == movePiece && !usedSet[pos - 11]
                && (_isEmpty(pos - 10) || usedSet[pos - 10]))
                theMap[horse].push_back(pos - 11);
            if (col < 7 && _getPiece(pos - 7) == movePiece && !usedSet[pos - 7]
                && (_isEmpty(pos - 8) || usedSet[pos - 8]))
                theMap[horse].push_back(pos - 7);
        }

        if (row < 9) {
            if (row < 8) {
                if (col > 0 && _getPiece(pos + 17) == movePiece && !usedSet[pos + 17]
                    && (_isEmpty(pos + 8) || usedSet[pos + 8]))
                    theMap[horse].push_back(pos + 17);
                if (col < 8 && _getPiece(pos + 19) == movePiece && !usedSet[pos + 19]
                    && (_isEmpty(pos + 10) || usedSet[pos + 10]))
                    theMap[horse].push_back(pos + 19);
            }

            if (col > 1 && _getPiece(pos + 7) == movePiece && !usedSet[pos + 7]
                && (_isEmpty(pos + 8) || usedSet[pos + 8]))
                theMap[horse].push_back(pos + 7);
            if (col < 7 && _getPiece(pos + 11) == movePiece && !usedSet[pos + 11]
                && (_isEmpty(pos + 10) || usedSet[pos + 10]))
                theMap[horse].push_back(pos + 11);
        }
    }

    /// rook, cannon, pawn, king
    auto rook = static_cast<int>(PieceTypeXq::rook);
    auto cannon = static_cast<int>(PieceTypeXq::cannon);
    auto pawn = static_cast<int>(PieceTypeXq::pawn);

    /* go down */
    for (auto y = pos + 9, f = 0; y < 90; y += 9) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                auto ok = false;
                if (f == 0) {
                    if (piece.type == rook || (piece.type == KING && posType == KING)) {
                        ok = true;
                    } else if (y == pos + 9) {
                        if (piece.type == pawn && attackerSide == Side::white) {
                            ok = true;
                        } else if (piece.type == KING) {
                            auto k = attackerSide == Side::white ? 89 - pos : pos;
                            ok = legalPositions[k] & 1;
                        }
                    }
                } else {
                    ok = piece.type == cannon;
                }

                if (ok) {
                    theMap[piece.type].push_back(y);
                }
            }

            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go up */
    for (auto y = pos - 9, f = 0; y >= 0; y -= 9) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                auto ok = false;
                if (f == 0) {
                    if (piece.type == rook || (piece.type == KING && posType == KING)) {
                        ok = true;
                    } else if (y == pos - 9) {
                        if (piece.type == pawn && attackerSide == Side::black) {
                            ok = true;
                        } else if (piece.type == KING) {
                            auto k = attackerSide == Side::white ? 89 - pos : pos;
                            ok = legalPositions[k] & 1;
                        }
                    }
                } else {
                    ok = piece.type == cannon;
                }

                if (ok) {
                    theMap[piece.type].push_back(y);
                }
            }

            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go left */
    for (int y = pos - 1, f = 0; y >= pos - col; y--) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                auto ok = false;
                if (f == 0) {
                    if (piece.type == rook) {
                        ok = true;
                    } else if (y == pos - 1) {
                        if (piece.type == pawn
                            && ((attackerSide == Side::white && pos < 45)
                                || (attackerSide == Side::black && pos > 44))) {
                            ok = true;
                        } else if (piece.type == KING) {
                            auto k = attackerSide == Side::white ? 89 - pos : pos;
                            ok = legalPositions[k] & 1;
                        }
                    }
                } else {
                    ok = piece.type == cannon;
                }

                if (ok) {
                    theMap[piece.type].push_back(y);
                }
            }

            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /* go right */
    for (int y = pos + 1, f = 0; y < pos - col + 9; y++) {
        if (usedSet[y]) continue;
        auto piece = _getPiece(y);
        if (!piece.isEmpty()) {
            if (piece.side == attackerSide) {
                auto ok = false;
                if (f == 0) {
                    if (piece.type == rook) {
                        ok = true;
                    } else if (y == pos + 1) {
                        if (piece.type == pawn
                            && ((attackerSide == Side::white && pos < 45)
                                || (attackerSide == Side::black && pos > 44))) {
                            ok = true;
                        } else if (piece.type == KING) {
                            auto k = attackerSide == Side::white ? 89 - pos : pos;
                            ok = legalPositions[k] & 1;
                        }
                    }
                } else {
                    ok = piece.type == cannon;
                }

                if (ok) {
                    theMap[piece.type].push_back(y);
                }
            }

            f++;
            if (f > 1) {
                break;
            }
        }
    }

    /// advisor & elephant
    {
        auto advisor = static_cast<int>(PieceTypeXq::advisor);
        auto elephant = static_cast<int>(PieceTypeXq::elephant);

        auto k = attackerSide == Side::white ? 89 - pos : pos;

        /// advisor
        auto abit = legalPosBits[advisor];
        if (legalPositions[k] & abit) {
            if (col > 3) {
                auto y = pos - 10;
                if (y > 0 && !usedSet[y] && _getPiece(y).type == advisor) {
                    theMap[advisor].push_back(y);
                }

                y = pos + 8;
                if (y < 90 && !usedSet[y] && _getPiece(y).type == advisor) {
                    theMap[advisor].push_back(y);
                }
            }
            if (col < 5) {
                auto y = pos - 8;
                if (y > 0 && !usedSet[y] && _getPiece(y).type == advisor) {
                    theMap[advisor].push_back(y);
                }
                y = pos + 10;
                if (y < 90 && !usedSet[y] && _getPiece(y).type == advisor) {
                    theMap[advisor].push_back(y);
                }
            }
        }

        /// elephant
        auto ebit = legalPosBits[elephant];
        if (legalPositions[k] & ebit) {
            static const int offsets[] = { -20, -16, 16, 20 };
            for (auto i = 0; i < 4; i++) {
                auto y = pos + offsets[i], m = (pos + y) / 2;
                if (y > 0 && y < 90 && !usedSet[y] && _getPiece(y).type == elephant
                    && (_isEmpty(m) || usedSet[m])) {
                    theMap[elephant].push_back(y);
                }
            }
        }
    }

    /// OK done
    std::vector<int> vec;
    for (int i = PAWNSTD; i > 0; i--) {
        if (!theMap[i].empty()) {
            vec.insert(vec.end(), theMap[i].begin(), theMap[i].end());
        }
    }
    return vec;
}

int XqBoard::see_capture(int ply, Side side, int pos, int posType, int xPieceVal, int* usedSet) const
{
    seeMaxPly = std::max(seeMaxPly, ply);

    auto vec = see_genAttack(side, pos, posType, usedSet);
    auto xside = xSide(side);
    auto gain = 0;

    for (auto&& x : vec) {
        assert(usedSet[x] == 0);
        usedSet[x] = 1;

        auto fromType = pieces[x].type;
        auto pieceVal = seePieceVals[fromType];

        auto bonus = 0;
        if (fromType == PAWNXQ && ((side == Side::white && pos < 45) || (side == Side::black && pos > 44))) {
            bonus = 100;
        }

        gain = std::max(gain,
                        xPieceVal + bonus - see_capture(ply + 1, xside, pos, fromType, pieceVal, usedSet));
        usedSet[x] = 0;
    }

    return gain;
}

int seeMaxPly_most = 0;

auto pawn0 = true, pawn1 = true;

int XqBoard::see(const Move& m) const
{
    assert(m.isValid() && !pieces[m.from].isEmpty());

    int usedSet[96];
    memset(&usedSet, 0, sizeof(usedSet));
    usedSet[m.from] = 1;

    auto mside = pieces[m.from].side;
    auto xside = xSide(mside);

    auto fromType = pieces[m.from].type;
    auto destType = pieces[m.dest].type;
    auto capVal = see_pieceVal(destType, m.dest);
    auto pieceVal = seePieceVals[fromType];

    if (destType == PAWNXQ
        && ((side == Side::white && m.dest > 44)
            || (side == Side::black && m.dest < 45))) { /// pawn side is opposite
        capVal += 100;
    }

    if (fromType == PAWNXQ
        && ((side == Side::white && m.dest < 45) || (side == Side::black && m.dest > 44))) {
        pieceVal += 100;

        if ((side == Side::white && m.from > 44) || (side == Side::black && m.from < 45)) {
            capVal += 100;
        }
    }

    seeMaxPly = 0;
    auto gain = capVal - see_capture(0, xside, m.dest, fromType, pieceVal, usedSet);
    return gain;
}

/// low 2 bytes is for black, hi for white
int XqBoard::materials() const
{
    auto w = 0, b = 0;

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

bool XqBoard::pieceList_setPiece(int* pieceList, int pos, int type, Side side)
{
    /// A covered jieqi piece has no identity until it is revealed, so there is
    /// no slot for it: the sixteen slots are one king, two each of advisor,
    /// elephant, rook, cannon and horse, and five pawns. pieceCount is [2][9]
    /// and does count them; pieceListStartIdxByType is [8] and indexing it with
    /// COVER read past the end.
    if (type == COVER) {
        return false;
    }

    if (type != EMPTY) {
        auto d = side == Side::white ? 16 : 0;

        auto k = type == KING ? 1 : type == PAWNXQ ? 5 : 2;
        for (auto t = pieceListStartIdxByType[type]; k > 0; t++, k--) {
            assert(t >= 0 && t < 16);
            if (pieceList[d + t] < 0 || pieceList[d + t] == pos) {
                pieceList[d + t] = pos;
                return true;
            }
        }
    }

    return false;
}

bool XqBoard::pieceList_isValid() const
{
    auto cnt = 0;
    for (auto sd = 0; sd < 2; sd++) {
        for (auto i = 0; i < 16; i++) {
            auto k = pieceList[sd][i];
            if (k < 0) {
                continue;
            }
            if (k >= 90) {
                return false;
            }
            auto piece = pieces[k];
            if (static_cast<int>(piece.side) != sd) {
                return false;
            }

            auto tp = pieceListIdxToType[i];

            if (piece.type != static_cast<int>(tp)) {
                return false;
            }
            cnt++;
        }
    }

    /// Covered pieces are on the board but deliberately not in the list, so
    /// they must not be counted against it either.
    for (auto&& p : pieces) {
        if (!p.isEmpty() && p.type != COVER) cnt--;
    }

    return cnt == 0;
}

bool XqBoard::pieceList_setEmpty(int* pieceList, int pos, int type, Side side)
{
    auto k = type == KING ? 1 : type == PAWNXQ ? 5 : 2;
    auto d = side == Side::white ? 16 : 0;
    for (auto t = pieceListStartIdxByType[type]; k > 0; t++, k--) {
        assert(t >= 0 && t < 16);
        if (pieceList[d + t] == pos) {
            pieceList[d + t] = -1;
            return true;
        }
    }

    return false;
}

/// The loops below walk the piece-list slots of one piece type. They stop at the end of the list
/// and report the failure when the piece is not in it, which happens when the board came from a
/// FEN that does not fit it, say a xiangqi position with three Advisors.
bool XqBoard::pieceList_make(const Hist& hist)
{
    /// Covered jieqi pieces are not in the list (see pieceList_setPiece), so
    /// neither a covered mover nor a covered capture is looked up here.
    /// JieqiBoard adds the revealed piece once it knows what it is.
    if (!hist.cap.isEmpty() && hist.cap.type != COVER) {
        for (auto t = pieceListStartIdxByType[hist.cap.type], sd = static_cast<int>(hist.cap.side); t < 16; t++) {
            assert(t >= 0 && t < 16);
            if (pieceList[sd][t] == hist.move.dest) {
                pieceList[sd][t] = -1;
                break;
            }
        }
    }
    if (hist.move.piece.type == COVER) {
        return true;
    }

    for (auto t = pieceListStartIdxByType[static_cast<int>(hist.move.piece.type)],
              sd = static_cast<int>(hist.move.piece.side); t < 16; t++) {
        assert(t >= 0 && t < 16);
        if (pieceList[sd][t] == hist.move.from) {
            pieceList[sd][t] = hist.move.dest;
            return true;
        }
    }
    return false;
}

bool XqBoard::pieceList_takeback(const Hist& hist)
{
    /// The mirror of pieceList_make: a covered mover was never in the list, and
    /// JieqiBoard has already taken the revealed piece back out of it.
    if (hist.move.piece.type != COVER) {
        for (auto t = pieceListStartIdxByType[hist.move.piece.type],
                  sd = static_cast<int>(hist.move.piece.side); t < 16; t++) {
            assert(t >= 0 && t < 16);
            if (pieceList[sd][t] == hist.move.dest) {
                pieceList[sd][t] = hist.move.from;
                break;
            }
        }
    }
    if (hist.cap.isEmpty() || hist.cap.type == COVER) {
        return true;
    }
    for (auto t = pieceListStartIdxByType[hist.cap.type], sd = static_cast<int>(hist.cap.side); t < 16; t++) {
        assert(t >= 0 && t < 16);
        if (pieceList[sd][t] < 0) {
            pieceList[sd][t] = hist.move.dest;
            return true;
        }
    }
    return false;
}

/// Draw if both sides have no attacker. Slots 0-4 are King, Advisors and Elephants, attackers are
/// from slot 5 (see pieceListStartIdxByType). This looped over the piece TYPE values ROOKXQ..PAWNXQ
/// (4..7) as if they were slots, so it looked at an Elephant, the Rooks and one Cannon only: a board
/// left with a Horse or a Pawn counted as a draw.
bool XqBoard::pieceList_isDraw(const int* pieceList) const
{
    for (auto t = pieceListStartIdxByType[ROOKXQ]; t < 16; t++) {
        if (pieceList[t] >= 0 || pieceList[t + 16] >= 0) {
            return false;
        }
    }

    return true;
}
