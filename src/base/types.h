/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_TYPES_H
#define BANKSIA_BASE_TYPES_H

#include <cstdint>

/// Short names for the fixed-width integer types, used throughout the tree.
///
/// They are aliases at global scope, not inside namespace banksia, because
/// much of the tree (banksia/fegtb, most of ui/) uses them outside that
/// namespace.
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using u8 = std::uint8_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

/// Still a macro: it pastes a suffix onto a literal, which no alias can do.
#ifdef _MSC_VER
#define U64(u) (u##ui64)
#else
#define U64(u) (u##ULL)
#endif

namespace banksia {

    /// The four variants Banksia plays. The order is load-bearing: comm.cpp
    /// indexes its name, starting-position and hash-key tables by it, and guards
    /// that with static_assert.
    enum class ChessVariant { xq, jieqi, standard, chess960, none };

    const int ScoreNull = 0xfffffff;

    enum class FENCharactorSet { standard, set2, max };

    enum Notation { san, lan, algebraic_coordinate, traditional_en };

    /// OCGDB addition: what the PGN move-text parser does with a variation, "( ... )":
    /// read it into the history (parse), keep it as a comment (asComment) or drop it (discard).
    enum ParseVariation { parse, asComment, discard, max };

    enum class ScoreResultType { point, percent, both, max };


    enum class XqCharSetStd { ucci, wiki, custom, max };

    enum class PieceTypeStd { empty, king, queen, rook, bishop, knight, pawn };

    enum class PieceTypeXq { empty, king, advisor, elephant, rook, cannon, horse, pawn, cover };

#ifdef _CHESS_
#define PieceType PieceTypeStd

#define BOARD_SZ 64
#define COLUMN_COUNT 8
#define ROW_COUNT 8

#else
#define PieceType PieceTypeXq

#define BOARD_SZ 90
#define COLUMN_COUNT 9
#define ROW_COUNT 10

#endif

    const int EMPTY = 0;
    const int KING = 1;

    const int PAWNSTD = static_cast<int>(PieceTypeStd::pawn);
    const int PAWNXQ = static_cast<int>(PieceTypeXq::pawn);
    const int COVERXQ = static_cast<int>(PieceTypeXq::cover);

    const int ELEPHANT = static_cast<int>(PieceTypeXq::elephant);
    const int ADVISOR = static_cast<int>(PieceTypeXq::advisor);
    const int HORSE = static_cast<int>(PieceTypeXq::horse);
    const int CANNON = static_cast<int>(PieceTypeXq::cannon);
    const int ROOKXQ = static_cast<int>(PieceTypeXq::rook);

    const int ROOKSTD = static_cast<int>(PieceTypeStd::rook);
    const int QUEEN = static_cast<int>(PieceTypeStd::queen);
    const int KNIGHT = static_cast<int>(PieceTypeStd::knight);
    const int BISHOP = static_cast<int>(PieceTypeStd::bishop);

#ifdef _CHESS_
    const int PAWN = PAWNSTD;
    const int COVER = PAWN;
#else
    const int PAWN = PAWNXQ;
    const int COVER = COVERXQ;
#endif

    const int B = 0;
    const int W = 1;

    enum class Side { black = 0, white = 1, none = 2 };

    enum class ResultType { noresult, win, draw, loss };

#ifdef _CHESS_
    enum class FlipMode {
        none,
        horizontal,
        vertical,
        flipVH,
        flipHV, /// for chess only
        rotate90, /// for chess only
        rotate, /// 180
        rotate270 /// for chess only
    };
#else
    enum class FlipMode { none, horizontal, vertical, rotate };
#endif

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
        adjudication_egtb_online,
        adjudication_score,
        adjudication_manual,
        perpetualchase,
        bothperpetualchase,
        extracomment,
        crash,
        abort
    };

    enum class EgtbSource { syzygy, felicity, none, max };

} // namespace banksia

#endif // BANKSIA_BASE_TYPES_H
