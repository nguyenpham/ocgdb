/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef TYPES_H
#define TYPES_H

namespace bslib {

enum class ChessVariant {
    standard, chess960, none
};

const int ScoreNull = 0xfffffff;

enum class CommentComputerInfoType {
    none, standard, tcec, max
};

enum class FENCharactorSet {
    standard, set2
};

enum Notation {
    san, lan, algebraic_coordinate
};

enum class ConnectMode {
    noconnect, editboard // , hack
};

enum class PieceTypeStd {
    empty, king, queen, rook, bishop, knight, pawn
};

enum class MoveEvaluationSymbol
{
    blunder, // ?? (Blunder)
    mistake,  //? (Mistake)
    dubious, // ?! (Dubious move)
    interesting, // !? (Interesting move)
    good, // ! (Good move)
    brilliant, //‼ (Brilliant move)
    none
};

const int EMPTY = 0;
const int KING = 1;

const int PAWNSTD = static_cast<int>(PieceTypeStd::pawn);
const int ROOKSTD = static_cast<int>(PieceTypeStd::rook);

const int B = 0;
const int W = 1;

enum class Side {
    black = 0, white = 1, none = 2
};

enum class ResultType {
    noresult, win, draw, loss
};

enum class FlipMode {
    none, horizontal, vertical, rotate
};

enum class ReasonType {
    noreason,
    mate,
    stalemate,
    repetition,
    resign,
    fiftymoves,
    insufficientmaterial,
    illegalmove,
    timeout,
    adjudication_length,
    adjudication_egtb,
    adjudication_score,
    adjudication_manual,
    perpetualchase,
    bothperpetualchase,
    extracomment,
    crash,
    abort
};

enum ChessPos {
    pos_a8, pos_b8, pos_c8, pos_d8, pos_e8, pos_f8, pos_g8, pos_h8,
    pos_a7, pos_b7, pos_c7, pos_d7, pos_e7, pos_f7, pos_g7, pos_h7,
    pos_a6, pos_b6, pos_c6, pos_d6, pos_e6, pos_f6, pos_g6, pos_h6,
    pos_a5, pos_b5, pos_c5, pos_d5, pos_e5, pos_f5, pos_g5, pos_h5,
    pos_a4, pos_b4, pos_c4, pos_d4, pos_e4, pos_f4, pos_g4, pos_h4,
    pos_a3, pos_b3, pos_c3, pos_d3, pos_e3, pos_f3, pos_g3, pos_h3,
    pos_a2, pos_b2, pos_c2, pos_d2, pos_e2, pos_f2, pos_g2, pos_h2,
    pos_a1, pos_b1, pos_c1, pos_d1, pos_e1, pos_f1, pos_g1, pos_h1,
};

} // namespace ocgdb

#endif // TYPES_H
