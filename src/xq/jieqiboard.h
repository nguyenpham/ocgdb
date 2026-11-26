/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef JIEQIBOARD_H
#define JIEQIBOARD_H

#include <random>

#include "xq.h"

namespace banksia {

    class JieqiBoard : public XqBoard
    {
    public:
        JieqiBoard();
        JieqiBoard(const JieqiBoard&);
        virtual ~JieqiBoard() override;

        virtual void _clone(const BoardData* oboard) override;

        virtual bool canLocatePiece(int type, Side side, int pos) const override;
        virtual void _make(const MoveFull& move, Hist& hist) override;
        virtual void _takeBack(const Hist& hist) override;

        /// OCGDB additions: a reveal is written with the piece's index and the type it turned
        /// out to be (jieqiboard.cpp)
        virtual uint16_t encode2Bytes(const Hist&) const override;
        virtual Move decode2Bytes(uint16_t d) const override;
        virtual bool isBoardDependent2Bytes(uint16_t d) const override { return (d & 0x8000) != 0; }

        /// This board's own generator for drawing revelations.
        ///
        /// The shared one in rng.h cannot do the job. It is drawn from by every
        /// game running at once, so with a concurrency above 1 the games
        /// interleave their draws in whatever order the threads happen to run
        /// and a tournament does not repeat from its seed -- which matters far
        /// more for reveals, drawn throughout a game, than for openings, drawn
        /// once at the start.
        ///
        /// Seeded per game from the tournament seed and the game's index, so a
        /// game reproduces whatever else is running beside it.
        void seedRevealRng(uint64_t seed);

        /// Put the generator back where the seed left it. assignReveals() calls
        /// this first so the shuffle is a pure function of the seed: Game
        /// starts a new game more than once per game, and without the re-seed
        /// each call carried on from wherever the generator had got to, so the
        /// two games of a pair ended up with different shuffles depending on
        /// how many times each had been started.
        void resetRevealRng();

        bool hasRevealRng() const { return revealRngSeeded; }

        /// Mutable because drawing does not change the position, and the
        /// functions that draw are const.
        mutable std::mt19937_64 revealRng;
        uint64_t revealSeed = 0;
        bool revealRngSeeded = false;

        /// The identity of every covered man, decided once at the start of the
        /// game by shuffling each side's complement over its covered squares.
        /// EMPTY where nothing is assigned.
        ///
        /// This is what the rules actually describe -- the men are shuffled
        /// face down before a move is played -- and drawing lazily at the
        /// moment of revelation is only an equivalent of it, because no player
        /// can condition on an identity before it is revealed. The two differ
        /// in one way that matters for measurement: with the shuffle fixed, two
        /// games played from the same seed have the same hidden board even
        /// after their move sequences diverge, and a covered man on a given
        /// square turns out to be the same piece in both.
        int8_t revealAssign[90];
        bool revealAssigned = false;

        /// Shuffle the unrevealed complement over the covered squares, using
        /// this board's own generator. Call after the start position is set.
        void assignReveals();

        virtual int drawRevelation(int from, Side side) const override;
        int getRandomAvailableRevelations(Side side) const;

    protected:
        virtual void gen_addMove(std::vector<MoveFull>& moveList, int from, int dest,
                                 bool capOnly) const override;
        virtual bool isParsedMoveValid(const Move&) const override;
        virtual void completeDecodedMove(Move&) const override;
    };

} // namespace banksia

#endif // JIEQIBOARD_H
