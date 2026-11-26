/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "chess960.h"

using namespace banksia;

////////////////////////////
ChessBoard960::ChessBoard960() : ChessBoard(ChessVariant::chess960)
{
    setFenCastleRights_clear();
}

ChessBoard960::~ChessBoard960() { }

//https://en.wikipedia.org/wiki/Fischer_random_chess_numbering_scheme
//Direct derivation
//White's starting array can be derived from its number N (0 ... 959) as follows:
//a) Divide N by 4, yielding quotient N2 and remainder B1. Place a Bishop upon the bright square corresponding to B1 (0=b, 1=d, 2=f, 3=h).
//b) Divide N2 by 4 again, yielding quotient N3 and remainder B2. Place a second Bishop upon the dark square corresponding to B2 (0=a, 1=c, 2=e, 3=g).
//c) Divide N3 by 6, yielding quotient N4 and remainder Q. Place the Queen according to Q, where 0 is the first free square starting from a, 1 is the second, etc.
//d) N4 will be a single digit, 0 ... 9. Place the Knights according to its value by consulting the following table:
//Digit	Knight positioning
//0	N	N	-	-	-
//1	N	-	N	-	-
//2	N	-	-	N	-
//3	N	-	-	-	N
//4	-	N	N	-	-
//5	-	N	-	N	-
//6	-	N	-	-	N
//7	-	-	N	N	-
//8	-	-	N	-	N
//9	-	-	-	N	N
//e) There are three blank squares remaining; place a Rook in each of the outer two and the King in the middle one.

std::string ChessBoard960::index2string(int n)
{
    if (n < 0 || n >= 960) {
        return "";
    }

    char s[10];
    memset(s, 0, sizeof(s));

    /// a)
    auto n2 = n / 4, b1 = n % 4;
    s[b1 * 2 + 1] = 'B';

    /// b)
    auto n3 = n2 / 4, b2 = n2 % 4;
    s[b2 * 2] = 'B';

    /// c)
    auto n4 = n3 / 6, q = n3 % 6;
    for (auto i = 0; i < 9; i++) {
        if (!s[i]) {
            if (q)
                q--;
            else {
                s[i] = 'Q';
                break;
            }
        }
    }

    const static int knightSpaces[10][2] = { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 0 },
                                             { 1, 1 }, { 1, 2 }, { 2, 0 }, { 2, 1 }, { 3, 0 } };

    auto k0 = knightSpaces[n4][0], k1 = knightSpaces[n4][1];

    for (auto i = 0; i < 8; i++) {
        if (s[i]) continue;
        if (k0) {
            k0--;
            continue;
        }
        s[i] = 'N';
        for (int j = i + 1; j < 8; j++) {
            if (s[j]) continue;
            if (k1) {
                k1--;
                continue;
            }
            s[j] = 'N';
            break;
        }
        break;
    }

    for (auto i = 0, c = 0; i < 8; i++) {
        if (!s[i]) {
            s[i] = c == 1 ? 'K' : 'R';
            c++;
        }
    }

    return s;
}

std::string ChessBoard960::indexString2Fen(const std::string& s)
{
    assert(s.length() == 8);
    auto s0 = s;
    toLower(s0);

    auto string = s0 + "/pppppppp/8/8/8/8/PPPPPPPP/" + s + " w KQkq - 0 1";
    return string;
}


std::string ChessBoard960::index2Fen(int idx)
{
    return indexString2Fen(index2string(idx));
}

std::vector<ChessBoard960PosRecord> ChessBoard960::allPos()
{
    std::vector<ChessBoard960PosRecord> vec;
    for (auto i = 0; i < 960; ++i) {
        ChessBoard960PosRecord r;
        r.idx = i;
        r.string = index2string(i);
        r.fen = indexString2Fen(r.string);
        vec.push_back(r);
    }

    return vec;
}


bool ChessBoard960::_newGame(std::string fen)
{
    if (fen.empty()) {
        auto idx = rand() % 960;
        fen = index2Fen(idx);
    }

    return ChessBoard::_newGame(fen);
}

bool ChessBoard960::isValidCastleRights() const
{
    auto c = castleRights[0] | castleRights[1];
    if (c) {
        if ((c & CastleRight_long)
            && (castleRights_column_rook_left < 0 || castleRights_column_rook_left >= 8)) {
            return false;
        }
        if ((c & CastleRight_short)
            && (castleRights_column_rook_right < 0 || castleRights_column_rook_right >= 8)) {
            return false;
        }
        return ChessBoard::isValidCastleRights();
    }
    return true;
}


Move ChessBoard960::moveFromString_castling(const std::string& str, Side side) const
{
    assert(str == "O-O" || str == "O-O+" || str == "0-0" || str == "O-O-O" || str == "O-O-O+"
           || str == "0-0-0");

    auto shortCastling = str.length() < 5;
    auto dest = shortCastling ? castleRights_column_rook_right : castleRights_column_rook_left;
    auto n0 = 0, n1 = 8;

    if (side == Side::white) {
        dest += 56;
        n0 = 56;
        n1 = 64;
    }

    auto from = -1;
    for (int i = n0 + 1; i < n1; i++) {
        auto p = pieces.at(i);
        if (p.type == KING && p.side == side) {
            from = i;
            break;
        }
    }

    return Move(from, dest, EMPTY);
}

void ChessBoard960::setFenCastleRights_clear()
{
    ChessBoard::setFenCastleRights_clear();
    castleRights_column_king = castleRights_column_rook_left = castleRights_column_rook_right = -1;
}

void ChessBoard960::setFenCastleRights(const std::string& str)
{
    for (auto&& ch : str) {
        if (castleRights_column_king < 0 && ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) {
            int from, to;
            Side side;
            if (ch >= 'A' && ch <= 'H') {
                from = 56;
                to = 63;
                side = Side::white;
            } else {
                from = 0;
                to = 7;
                side = Side::black;
            }
            for (int i = from; i < to; ++i) {
                if (pieces.at(i).side != side) continue;
                auto type = pieces.at(i).type;
                if (type == KING) {
                    castleRights_column_king = i - from;
                    break;
                }
            }
        }
        if ((ch >= 'A' && ch <= 'H') || (ch >= 'a' && ch <= 'h')) {
            int col, sd;
            if (ch >= 'A' && ch <= 'H') {
                col = ch - 'A';
                sd = W;
            } else {
                col = ch - 'a';
                sd = B;
            }

            auto s = (ch >= 'A' && ch <= 'H') ? Side::white : Side::black;
            auto p = pieces[col + (s == Side::white ? 56 : 0)];

            if (p.type == ROOKSTD && p.side == s) {
                if (col < castleRights_column_king) {
                    castleRights_column_rook_left = col;
                    castleRights[sd] |= CastleRight_long;
                } else {
                    castleRights_column_rook_right = col;
                    castleRights[sd] |= CastleRight_short;
                }
            }
            continue;
        }


        if (ch == 'K' || ch == 'Q' || ch == 'k' || ch == 'q') {
            int from, to, d = 1;
            Side side;
            if (ch == 'K' || ch == 'Q') {
                from = 56;
                to = 63;
                side = Side::white;
            } else {
                from = 0;
                to = 7;
                side = Side::black;
            }
            if (ch == 'K' || ch == 'k') {
                std::swap(from, to);
                d = -1;
            }
            for (int i = from; i != to; i += d) {
                if (pieces.at(i).type == static_cast<int>(PieceTypeStd::rook) && pieces.at(i).side == side) {
                    if (ch == 'K' || ch == 'k') {
                        castleRights_column_rook_right = i % 8;
                        castleRights[ch == 'K' ? W : B] |= CastleRight_short;
                    } else {
                        castleRights_column_rook_left = i % 8;
                        castleRights[ch == 'Q' ? W : B] |= CastleRight_long;
                    }
                    break;
                }
            }
        }
    }
}


// X-FEN
std::string ChessBoard960::getFenCastleRights() const
{
    assert(castleRights_column_rook_left >= 0
           && castleRights_column_rook_right - castleRights_column_rook_left >= 2
           && castleRights_column_rook_right < 8);
    std::string s;
    if (castleRights[W] + castleRights[B]) {
        if (castleRights[W]) {
            int kingPos = findKing(banksia::Side::white);
            if (castleRights[W] & CastleRight_short) {
                auto outer = true; // find outer rook
                for (auto i = 63; i > kingPos; i--) {
                    auto p = _getPiece(i);
                    if (p.type == static_cast<int>(PieceTypeStd::rook) && p.side == banksia::Side::white) {
                        outer = getColumn(i) == castleRights_column_rook_right;
                        break;
                    }
                }

                if (outer) {
                    s += "K";
                } else {
                    s += char('A' + castleRights_column_rook_right);
                }
            }
            if (castleRights[W] & CastleRight_long) {
                auto outer = true;
                for (auto i = kingPos - 1; i >= 56; i--) {
                    auto p = _getPiece(i);
                    if (p.type == static_cast<int>(PieceTypeStd::rook) && p.side == banksia::Side::white) {
                        outer = getColumn(i) == castleRights_column_rook_left;
                        break;
                    }
                }

                if (outer) {
                    s += "Q";
                } else {
                    s += char('A' + castleRights_column_rook_left);
                }
            }
        }
        if (castleRights[B]) {
            int kingPos = findKing(banksia::Side::black);
            if (castleRights[B] & CastleRight_short) {
                auto outer = true;
                for (auto i = 7; i > kingPos; --i) {
                    auto p = _getPiece(i);
                    if (p.type == static_cast<int>(PieceTypeStd::rook) && p.side == banksia::Side::black) {
                        outer = getColumn(i) == castleRights_column_rook_right;
                        break;
                    }
                }

                if (outer) {
                    s += "k";
                } else {
                    s += char('a' + castleRights_column_rook_right);
                }
            }
            if (castleRights[B] & CastleRight_long) {
                auto outer = true;
                for (auto i = kingPos - 1; i >= 0; i--) {
                    auto p = _getPiece(i);
                    if (p.type == static_cast<int>(PieceTypeStd::rook) && p.side == banksia::Side::black) {
                        outer = getColumn(i) == castleRights_column_rook_left;
                        break;
                    }
                }

                if (outer) {
                    s += "q";
                } else {
                    s += char('a' + castleRights_column_rook_left);
                }
            }
        }
    } else {
        s = "-";
    }

    return s;
}


void ChessBoard960::gen_castling(std::vector<MoveFull>& moves, int kingPos) const
{
    int kingTo[] = { -1, -1 };

    if (kingPos == castleRights_column_king) {
        if (castleRights[B] & CastleRight_long) {
            kingTo[1] = 2;
        }
        if (castleRights[B] & CastleRight_short) {
            kingTo[0] = 6;
        }
    } else if (kingPos == castleRights_column_king + 56 && castleRights[W]) {
        if (castleRights[W] & CastleRight_long) {
            kingTo[1] = 58;
        }
        if (castleRights[W] & CastleRight_short) {
            kingTo[0] = 62;
        }
    }

    if (kingTo[0] < 0 && kingTo[1] < 0) {
        return;
    }


    for (auto k = 0; k < 2; ++k) {
        auto nextPos = kingTo[k];
        if (nextPos < 0) continue;

        auto rookFrom = k == 1 ? castleRights_column_rook_left : castleRights_column_rook_right;
        auto rookTo = k == 1 ? 3 : 5;

        auto bad = false;
        Side side, opSide;
        if (kingPos > 32) {
            side = Side::white;
            opSide = Side::black;
            rookFrom += 56;
            rookTo += 56;
        } else {
            side = Side::black;
            opSide = Side::white;
        }

        /// Check rook way must be empty
        if (rookFrom != rookTo) {
            for (int d = rookFrom < rookTo ? 1 : -1, pos = rookFrom + d;; pos += d) {
                auto p = pieces[pos];
                if (!p.isEmpty() && (p.type != KING || p.side != side)) {
                    bad = true;
                    break;
                }

                if (pos == rookTo) {
                    break;
                }
            }

            if (bad) continue;
        }

        /// Check king way must be empty & not being attacked
        if (kingPos == nextPos) {
            bad = beAttacked(kingPos, opSide);
        } else {
            auto d = kingPos < nextPos ? 1 : -1;

            /// Check king way must be empty
            for (int pos = kingPos + d;; pos += d) {
                if (pos != rookFrom && !pieces[pos].isEmpty()) {
                    bad = true;
                    break;
                }

                if (pos == nextPos) {
                    break;
                }
            }

            if (bad) continue;

            /// Check king way must not be attacked
            for (int pos = kingPos;; pos += d) {
                if (beAttacked(pos, opSide)) {
                    bad = true;
                    break;
                }
                if (pos == nextPos) {
                    break;
                }
            }
        }

        if (bad) continue;

        auto movingPiece = _getPiece(kingPos);
        moves.push_back(MoveFull(movingPiece, kingPos, rookFrom));
    }
}


bool ChessBoard960::_quickCheck_king(int from, int dest, bool checkMore) const
{
    auto d = std::abs(from - dest);
    auto rf = getRow(from), rd = getRow(dest);

    if (rf == rd && _getPiece(from).side == _getPiece(dest).side) { // O-O, O-O-O
        if (_getPiece(dest).type == ROOKSTD
            && ((from >= 56 && side == Side::white) || (from < 8 && side == Side::black))) {
            if (checkMore) {
                int begin = rf * 8, kingPos, rookPos;
                if (from < dest) { // O-O
                    kingPos = begin + 6;
                    rookPos = begin + 5;
                } else { // O-O-O
                    kingPos = begin + 2;
                    rookPos = begin + 3;
                }

                // Last positions of King and Rook should be empty
                if ((!_isEmpty(kingPos) && kingPos != from && kingPos != dest)
                    || (!_isEmpty(rookPos) && rookPos != from && rookPos != dest)) {
                    return false;
                }

                // King could move from current location to standard castle location
                auto d = kingPos - from;
                if (std::abs(d) > 2) {
                    for (int a = d > 0 ? 1 : -1, i = from + a; i != kingPos; i += a) {
                        if (!_isEmpty(i) && i != dest) {
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        return false;
    }

    if (d == 1 || (d >= 7 && d <= 9)) {
        if (d == 1) {
            return rf == rd;
        }

        return std::abs(rf - rd) == 1;
    }

    return false;
}

extern u64* RandomCastle;

void ChessBoard960::_make(const MoveFull& move, Hist& hist)
{
    assert(move.piece == pieces[move.from]);
    auto cap = pieces[move.dest];
    if (move.piece.type != KING || cap.type != static_cast<int>(PieceTypeStd::rook)
        || move.piece.side != cap.side) {
        if (move.piece.type == KING && std::abs(move.from - move.dest) == 2) {
            printOut("Something wird");
            [[maybe_unused]] auto i = 0;
        }
        return ChessBoard::_make(move, hist);
    }

    assert(_isHashKeyValid());

    hist.enpassant = enpassant;
    hist.status = status;
    hist.castleRights[0] = castleRights[0];
    hist.castleRights[1] = castleRights[1];

    hist.move = move;
    hist.cap = Piece::emptyPiece;
    hist.hashKey = hashKey;
    hist.quietCnt = quietCnt;

    hashKey ^= hashKeyEnpassant(enpassant);

    auto kingFrom = move.from, rookFrom = move.dest, rookCol = rookFrom % 8;
    int kingTo, rookTo;
    if (rookCol == castleRights_column_rook_left) {
        kingTo = 2;
        rookTo = 3;
        hist.castled = CastleRight_long;
    } else {
        kingTo = 6;
        rookTo = 5;
        hist.castled = CastleRight_short;
    }

    if (kingFrom > 32) {
        kingTo += 56;
        rookTo += 56;
        castleRights[W] &= ~(CastleRight_long | CastleRight_short);
    } else {
        castleRights[B] &= ~(CastleRight_long | CastleRight_short);
    }

    auto rook = pieces[rookFrom];
    if (kingFrom != kingTo) {
        hashKey ^= xorHashKey(move.from);
        pieces[move.from].setEmpty();

        if (kingTo == rookFrom) {
            hashKey ^= xorHashKey(kingTo);
        }
        pieces[kingTo] = move.piece;
        hashKey ^= xorHashKey(kingTo);
    }

    if (rookFrom != rookTo) {
        if (kingTo != rookFrom) {
            hashKey ^= xorHashKey(rookFrom);
            pieces[rookFrom].setEmpty();
        }

        pieces[rookTo] = rook;
        hashKey ^= xorHashKey(rookTo);
    }

    if (hist.castleRights[W] != castleRights[W]) {
        if ((hist.castleRights[W] & CastleRight_short) != (castleRights[W] & CastleRight_short)) {
            hashKey ^= RandomCastle[0];
        }
        if ((hist.castleRights[W] & CastleRight_long) != (castleRights[W] & CastleRight_long)) {
            hashKey ^= RandomCastle[1];
        }
    }
    if (hist.castleRights[B] != castleRights[B]) {
        if ((hist.castleRights[B] & CastleRight_short) != (castleRights[B] & CastleRight_short)) {
            hashKey ^= RandomCastle[2];
        }
        if ((hist.castleRights[B] & CastleRight_long) != (castleRights[B] & CastleRight_long)) {
            hashKey ^= RandomCastle[3];
        }
    }

    quietCnt = 0;
    enpassant = -1;

    hashKey ^= hashKeyEnpassant(enpassant);

    /// OCGDB: the piece list follows the castling. ChessBoard::_make() keeps it up to date move
    /// by move, but this path did not touch it: after every chess960 castling the list still had
    /// the king and the rook on their old squares (ChessBoard::isValid() false, an assertion in a
    /// debug build at the next move). Rebuilt from the squares: a castling is rare enough.
    pieceList_setupAll();
}


void ChessBoard960::_takeBack(const Hist& hist)
{
    if (!hist.castled) {
        return ChessBoard::_takeBack(hist);
    }

    assert((hist.move.dest % 8) == castleRights_column_rook_left
           || (hist.move.dest % 8) == castleRights_column_rook_right);

    auto kingTo = hist.move.from < hist.move.dest ? 6 : 2;
    auto rookTo = hist.move.from < hist.move.dest ? 5 : 3;
    if (hist.move.from > 32) {
        kingTo += 56;
        rookTo += 56;
    }

    auto rook = _getPiece(rookTo);

    if (hist.move.from != kingTo) {
        auto king = _getPiece(kingTo);
        _setPiece(hist.move.from, king);
        _setEmpty(kingTo);
    }

    if (hist.move.dest != rookTo) {
        _setPiece(hist.move.dest, rook);

        if (rookTo != hist.move.from) {
            _setEmpty(rookTo);
        }
    }

    status = hist.status;
    castleRights[0] = hist.castleRights[0];
    castleRights[1] = hist.castleRights[1];
    enpassant = hist.enpassant;
    quietCnt = hist.quietCnt;

    hashKey = hist.hashKey;

    /// OCGDB: as in _make(), the piece list rebuilt after the castling
    pieceList_setupAll();
}
