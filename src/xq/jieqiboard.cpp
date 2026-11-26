/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "jieqiboard.h"
#include "base/rng.h"

using namespace banksia;

JieqiBoard::JieqiBoard() : XqBoard()
{
    variant = ChessVariant::jieqi;
}

JieqiBoard::JieqiBoard(const JieqiBoard& other)
{
    _clone(&other);
}

JieqiBoard::~JieqiBoard() { }

void JieqiBoard::_clone(const BoardData* oboard)
{
    XqBoard::_clone(oboard);

    /// A JieqiBoard is cloned only from another (Game::createBoard), but an
    /// XqBoard's data may be cloned into one: it brings no assignment.
    auto ob = dynamic_cast<const JieqiBoard*>(oboard);
    if (ob) {
        memcpy(revealAssign, ob->revealAssign, sizeof(revealAssign));
        revealAssigned = ob->revealAssigned;
    } else {
        revealAssigned = false;
    }
}

void JieqiBoard::seedRevealRng(uint64_t seed)
{
    revealSeed = seed;
    revealRng.seed(static_cast<std::mt19937_64::result_type>(seed));
    revealRngSeeded = true;
}

void JieqiBoard::resetRevealRng()
{
    if (revealRngSeeded) {
        revealRng.seed(static_cast<std::mt19937_64::result_type>(revealSeed));
    }
}

/// Decide every covered man's identity now, by shuffling what is left to
/// reveal over the squares that are still face down.
///
/// Equivalent to drawing lazily as far as either player can tell -- nobody can
/// condition on an identity before it is revealed, so deferring the draw is
/// sound -- but not equivalent for measuring engines against each other. Two
/// games seeded alike now share the hidden board itself, not merely a stream of
/// random numbers, so a man on a given square is the same piece in both however
/// early the games diverge. They diverge at the first move: pairing on the
/// stream alone reduced the variance of a 400-game match by 4%.
void JieqiBoard::assignReveals()
{
    std::fill(std::begin(revealAssign), std::end(revealAssign), int8_t(EMPTY));
    revealAssigned = false;

    /// From the seed, every time -- see resetRevealRng().
    resetRevealRng();

    if (variant != ChessVariant::jieqi) {
        return;
    }

    for (auto sd = 0; sd < 2; sd++) {
        auto side = static_cast<Side>(sd);

        std::vector<int> squares;
        for (auto i = 0; i < 90; i++) {
            auto p = _getPiece(i);
            if (p.type == COVERXQ && p.side == side) {
                squares.push_back(i);
            }
        }
        if (squares.empty()) {
            continue;
        }

        /// The bag: what this side has still to reveal, counted the one way
        /// (initial less revealed, whether standing or since captured).
        std::vector<int8_t> bag;
        for (auto t = KING + 1; t < COVERXQ; t++) {
            for (auto n = availableRevelationCount(t, side); n > 0; n--) {
                bag.push_back(int8_t(t));
            }
        }

        /// The bag can hold more than there are covered men standing, because a
        /// man captured while covered never had its identity determined and so
        /// never left the bag. Shuffle the whole bag and take as many as there
        /// are squares; which entries are left over is exactly the information
        /// the game never reveals.
        for (size_t i = bag.size(); i > 1; i--) {
            std::uniform_int_distribution<size_t> dist(0, i - 1);
            std::swap(bag[i - 1], bag[dist(revealRng)]);
        }

        for (size_t i = 0; i < squares.size() && i < bag.size(); i++) {
            revealAssign[squares[i]] = bag[i];
        }
    }

    revealAssigned = true;
}


/// The shuffle decided this at the start of the game; the draw is the
/// fallback for a board that never had one (the self-tests, a GUI's
/// analysis board).
int JieqiBoard::drawRevelation(int from, Side side) const
{
    return revealAssigned && revealAssign[from] != EMPTY ? int(revealAssign[from])
                                                         : getRandomAvailableRevelations(side);
}

/// Draw one revelation from the bag, each type weighted by how many of it are
/// left. This is the arbiter's decision and nobody else's: in jieqi a covered
/// man's identity was fixed by the shuffle and is merely discovered now, so an
/// engine does not get to say what its man turns out to be.
///
/// The draw comes from the tournament's shared generator, so `openings.base.seed`
/// reproduces the reveals along with everything else it already reproduces.
int JieqiBoard::getRandomAvailableRevelations(Side side) const
{
    std::vector<int> vec;

    for (auto t = KING + 1; t < COVERXQ; ++t) {
        auto a = availableRevelationCount(t, side);
        assert(a >= 0 && a <= 5);
        for (auto i = 0; i < a; ++i) {
            vec.push_back(t);
        }
    }
    if (vec.empty()) {
        return EMPTY;
    }

    /// This board's own generator when the game gave it one, so a tournament
    /// repeats from its seed at any concurrency; the shared one otherwise, for
    /// a board used outside a game (the self-tests, a GUI's analysis board).
    if (revealRngSeeded) {
        std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
        return vec.at(dist(revealRng));
    }

    return vec.at(rngIndex(vec.size()));
}

/// A covered man reveals (the move names what it turned out to be), a revealed
/// one does not.
bool JieqiBoard::isParsedMoveValid(const Move& move) const
{
    auto pieceType = _getPiece(move.from).type;
    return !((move.promotion <= KING && pieceType == COVER) || (move.promotion != EMPTY && pieceType != COVER));
}


void JieqiBoard::gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool captureOnly) const
{
    auto toSide = _getPiece(dest).side;
    auto movingPiece = _getPiece(from);
    auto fromSide = movingPiece.side;

    if (fromSide != toSide && (!captureOnly || toSide != Side::none)) {
        if (movingPiece.type == COVER) {
            /// One candidate per type still in the bag. Which of them actually
            /// happens is drawn by the arbiter in XqBoard::_checkMake(), not
            /// chosen here and not chosen by the engine.
            ///
            /// availableRevelationCount() is the one definition of the bag: it
            /// counts what has been revealed, not only what is on the board, so a
            /// revealed and lost chariot does not free a slot for a third one.
            for (auto promotion = KING + 1; promotion < COVER; promotion++) {
                if (availableRevelationCount(promotion, fromSide) > 0) {
                    moveList.push_back(MoveFull(movingPiece, from, dest, promotion));
                }
            }
        } else {
            moveList.push_back(MoveFull(movingPiece, from, dest));
        }
    }
}

bool JieqiBoard::canLocatePiece(int type, Side side, int pos) const
{
    return type != KING || XqBoard::canLocatePiece(KING, side, pos);
}


void JieqiBoard::_make(const MoveFull& move, Hist& hist)
{
    XqBoard::_make(move, hist);

    if (move.piece.type == COVER) {
        hashKey ^= xorHashKey(move.dest);

        assert(hist.move.promotion != EMPTY && hist.move.promotion == move.promotion);
        pieces[move.dest].type = move.promotion;
        auto sd = static_cast<int>(move.piece.side);
        pieceCount[sd][COVER]--;
        pieceCount[sd][static_cast<int>(move.promotion)]++;

        /// The piece has an identity now, so it joins the piece list. It was
        /// not in it while covered -- XqBoard::pieceList_make() left it alone.
        pieceList_setPiece(&pieceList[0][0], move.dest, move.promotion, move.piece.side);

        hashKey ^= xorHashKey(move.dest);
    } else {
        assert(hist.move.promotion == EMPTY);
    }

}

void JieqiBoard::_takeBack(const Hist& hist)
{
    assert(_isHashKeyValid());
    assert(pieces[hist.move.dest].idx == hist.move.piece.idx);

    if (hist.move.piece.type == COVER) {
        auto sd = static_cast<int>(hist.move.piece.side);
        auto t = pieces[hist.move.dest].type;
        assert(t > KING && t <= static_cast<int>(PieceTypeXq::pawn));

        /// Undo the reveal: out of the piece list, and back to being covered.
        pieceList_setEmpty(&pieceList[0][0], hist.move.dest, t, hist.move.piece.side);

        pieceCount[sd][static_cast<int>(PieceTypeXq::cover)]++;
        pieceCount[sd][static_cast<int>(t)]--;
        assert(pieceCount[sd][static_cast<int>(t)] >= 0);
        assert(pieceCount[sd][COVER] < 16);
    }

    XqBoard::_takeBack(hist);
}


/// OCGDB additions (from Banksia GUI's core)
/// A move read from a database without its reveal: a covered man becomes a pawn.
void JieqiBoard::completeDecodedMove(Move& move) const
{
    if (move.promotion == EMPTY && _getPiece(move.from).type == COVER) {
        move.promotion = PAWNXQ;
    }
}

uint16_t JieqiBoard::encode2Bytes(const Hist& hist) const
{
    if (hist.move.promotion && variant == ChessVariant::jieqi) {
        assert(hist.move.promotion > KING && hist.move.promotion < COVER);
        assert(hist.move.piece.type == COVER);
        return 0x8000 | hist.move.piece.idx | hist.move.promotion << 4
               | hist.move.dest << 7; // | move.promotion << 14;
    }
    return hist.move.from | hist.move.dest << 7; // | move.promotion << 14;
}

Move JieqiBoard::decode2Bytes(uint16_t d) const
{
    Move m;

    if (d & 0x8000) {
        m.dest = (d >> 7) & 0x7f;
        m.promotion = (d >> 4) & 7;
        assert(m.promotion > KING && m.promotion < COVER);

        auto idx = d & 0xf;
        for (auto from = 0; from < 90; ++from) {
            if (pieces[from].idx == idx && pieces[from].side == side) {
                assert(pieces[from].type == COVER);
                m.from = from;
                break;
            }
        }
    } else {
        m.from = d & 0x7f;
        m.dest = (d >> 7) & 0x7f;
        m.promotion = 0;
    }
    return m;
}
