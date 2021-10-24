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

namespace ocgdb {

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

} // namespace ocgdb

#endif // TYPES_H
