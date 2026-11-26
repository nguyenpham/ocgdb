/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_CHESS_CHESS960_H
#define BANKSIA_CHESS_CHESS960_H

#include "chess.h"

namespace banksia {

    class ChessBoard960PosRecord
    {
    public:
        int idx;
        std::string string, fen;
    };

    class ChessBoard960 : public ChessBoard
    {
    public:
        ChessBoard960();
        virtual ~ChessBoard960() override;

        static std::string index2string(int idx);
        static std::string indexString2Fen(const std::string&);
        static std::string index2Fen(int idx);
        static std::vector<ChessBoard960PosRecord> allPos();


        virtual bool _quickCheck_king(int from, int dest, bool checkMiddle) const override;

        virtual void _make(const MoveFull& move, Hist& hist) override;
        virtual void _takeBack(const Hist& hist) override;


        virtual bool _newGame(std::string fen = "") override;

    protected:
        virtual bool isValidCastleRights() const override;

        virtual Move moveFromString_castling(const std::string& str, Side side) const override;
        virtual void setFenCastleRights_clear() override;
        virtual void setFenCastleRights(const std::string& string) override;
        virtual std::string getFenCastleRights() const override;

        virtual void gen_castling(std::vector<MoveFull>& moveList, int kingPos) const override;
    };

} // namespace banksia

#endif // BANKSIA_CHESS_CHESS960_H
