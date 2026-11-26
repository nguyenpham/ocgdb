/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_XQ_XQ_H
#define BANKSIA_XQ_XQ_H

#include <stdio.h>

#include "base/base.h"
#include "xqbitboard.h"

namespace banksia {

    class XqBoard : public BoardCore
    {
    public:
        using BoardCore::_checkMake;
        using BoardCore::_gen;
        using BoardCore::_make;
        using BoardCore::_takeBack;
        using BoardCore::flip;
        using BoardCore::getFen;
        using BoardCore::getStartingFen;
        using BoardCore::isValid;
        using BoardCore::make;
        using BoardCore::takeBack;
        using BoardCore::toString;

        XqBoard();
        XqBoard(const XqBoard&);
        virtual ~XqBoard() override;

        virtual void _clone(const BoardData* oboard) override;

        virtual std::string toString() const override;
        virtual bool isValid() const override;

        virtual int charactorToPieceType(char ch) const override { return xqCharactorToPieceType(ch); }

        virtual bool _setFen(const std::string& fen) override;
        virtual std::string getFen(bool enpassantLegal, int halfCount, int fullMoveCount,
                                   bool horizontalFlip = false,
                                   const MoveFormat& = MoveFormat()) const override;

        virtual std::string getStartingFen(const MoveFormat&) const override;

        virtual bool isValueSmaller(int type0, int type1) const override;

        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide,
                          int forPieceType) const override;
        virtual bool _isIncheck(Side beingAttackedSide) const override;


        virtual void _make(const MoveFull& move, Hist& hist) override;
        virtual void _takeBack(const Hist& hist) override;

        virtual Result rule() override;
        virtual Result ruleRepetition(int repeatLen) override;

        virtual bool _checkMake(int from, int dest, int promotion) override;

        virtual Result probeEgtb(int maxPieces, bool& tberror) const override;
        virtual std::vector<MoveFull> probeEgtbAll() const override;

        virtual Eco getEco(u64 key) const override;
        virtual bool isEcoName(const std::string name) const override;

        /// OCGDB additions (xq_db.cpp): bitboards for PQL, the binary move fields of a
        /// database, duplicate keys
        virtual BitboardBase* posToBitboards() const override;

        virtual uint16_t encode2Bytes(const Hist&) const override;
        virtual Move decode2Bytes(uint16_t d) const override;
        virtual std::pair<uint16_t, int> encode1Byte(MoveFull move) override;
        virtual std::pair<Move, int> decode1Byte(const int8_t* d) override;

        virtual uint64_t getHashKeyForCheckingDuplicates(int) const override;

        virtual bool isValidPromotion(int promotion, Side side) const override;
        bool setupCaptureNumber(int pieceType, Side side, int newCaptureNumber);
        bool simpleSetupCaptureNumber(int pieceType, Side side, int newCaptureNumber);
        virtual void _setFenComplete() override;

        int pieceCount[2][9], capturedCount[2][9];

        void reset()
        {
            BoardCore::reset();
            memset(pieceCount, 0, sizeof(pieceCount));
            memset(capturedCount, 0, sizeof(capturedCount));
        }

        virtual std::string toString(const Piece&) const override;
        virtual std::string toString(const Move&, const MoveFormat&) const override;
        virtual std::string toString(const MoveFull&, const MoveFormat&) const override;

        virtual std::string toString_coordinate(const MoveFull&, const MoveFormat&) const override;
        virtual std::string toString_traditional(const MoveFull& move, Notation notation,
                                                 const MoveFormat&) const override;

        virtual std::string toString_san(const Hist& hist) const override;
        virtual std::string toString_san(const Hist& hist, const MoveFormat& mf) const override;

        static std::string piece2String(const Piece&);

        static std::string hist2String(const HistBasic&, Notation, const MoveFormat&);
        static std::string moveString_san(const HistBasic&);
        static std::string moveString_coordinate(const Move& move, const MoveFormat&);
        static std::string moveString_traditional(const MoveFull& move, Notation, const MoveFormat&);


        static Move xqMoveFromCoordiateString(const std::string& s, const MoveFormat&);
        static std::vector<Move> coordinateString2Moves(const std::string& str, const MoveFormat&);

        void setPiece(int pos, Piece piece)
        {
            if (piece.cover > EMPTY) {
                piece.side = pos > 45 ? Side::white : Side::black;
            }
            pieces[pos] = piece;

            auto sd = static_cast<int>(piece.side);
            pieceCount[sd][piece.type]++;
            pieceList_setPiece((int*)pieceList, pos, piece.type, piece.side);
        }

        void createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const override;

        static int xqflip(int pos, FlipMode flipMode);

        virtual int flip(int pos, FlipMode flipMode) const override;


        Move moveFromTraditionalString(const std::string& s, const MoveFormat&);

        virtual Move moveFromString_san(const std::string&, const MoveFormat&) override;
        virtual Move moveFromString_xq(const std::string&, const MoveFormat&) override;

        static int xqCharactorToPieceType(char ch);

        int availableRevelations(Side side, PieceTypeXq revelation) const;

        /// The bag: how many men of this type the side has left to reveal.
        /// The one definition -- isValidPromotion(), gen_addMove() and the
        /// draw all go through it.
        int availableRevelationCount(int pieceType, Side side) const;

        /// Jieqi: what the covered man on `from` turns out to be, decided by the
        /// arbiter when a move arrives without one (_checkMake). JieqiBoard draws
        /// it; a board of another variant has no covered men, hence EMPTY.
        virtual int drawRevelation(int /*from*/, Side /*side*/) const { return EMPTY; }


        virtual u64 initHashKey() const override;
        virtual u64 xorSideHashKey() const override;

        /// The FEN of BHGui and chessdb.cn: N and B for the horse and the elephant. Only the board
        /// part changes.
        static std::string convertFenToBHFen(const std::string& fen)
        {
            auto s = fen;
            for (auto& ch : s) {
                if (ch == ' ') break;
                if (ch == 'e')
                    ch = 'b';
                else if (ch == 'E')
                    ch = 'B';
                else if (ch == 'h')
                    ch = 'n';
                else if (ch == 'H')
                    ch = 'N';
            }
            return s;
        }

        bool isOriginal() const;

        virtual char pieceType2Char(int pieceType) const override;
        virtual int coordinateStringToPos(const std::string& str, const MoveFormat&) const override;
        virtual std::string posToCoordinateString(int pos, const MoveFormat&) const override;

        static int xqCoordinateStringToPos(const std::string& str, const MoveFormat&);

        void setupCapturedCount();

        virtual void setupPieceCount() override;
        virtual bool canLocatePiece(int type, Side side, int pos) const override;
        virtual int attackerCnt() const override;
        virtual bool hasAttackers() const override;
        virtual bool stalemateLoses() const override { return true; }
        virtual bool flipSwapsSides() const override { return true; }
        /// Jieqi carries a revelation in the promotion field, so a legality query
        /// naming one must match it. A jieqi query with promotion EMPTY means "is
        /// this from/dest legal at all", which it is if any revelation makes it so.
        virtual bool movesCarryPromotion() const override { return variant == ChessVariant::jieqi; }
        virtual size_t coordinateFromLength(int from, const MoveFormat& mf) const override;

        /// adding
        virtual bool isFenValid(const std::string& fen) const override;
        static bool isXqFenValid(const std::string& fen);

        virtual int getRawMaterialScore() const override;


        virtual bool _quickCheckMake(int from, int dest, int promotion, bool createSanString) override;
        bool _quickCheck_cannon(int from, int dest, bool checkMiddle) const;


        virtual int materials() const override;
        virtual int see(const Move&) const override;

        std::string getSanString(const Hist& hist, const MoveFormat& mf) const;

    private:
        void findAmbiguous(std::vector<Move>& moves, int pieceType, int atCol, char relative,
                           int promoteType);
        int find(int pieceType, int atCol, char relative, bool promoting) const;

        std::vector<int> _attackByRook(int from, int dest, const Piece& movePiece);
        std::vector<int> _moveByCannon(int from, int dest, const Piece& movePiece, bool cap);
        std::vector<int> _attackByAdvisor(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByElephant(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByPawn(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByHorse(int from, int dest, const Piece& movePiece);

        virtual bool createSanStringForLastMove(const MoveFormat&) override;
        /// OCGDB addition (jieqi): other covered men that could have made the last move
        std::vector<int> _moveByCover(const Hist&) const;

        std::vector<int> see_genAttack(Side side, int pos, int posType, const int* usedSet) const;
        int see_capture(int ply, Side side, int pos, int posType, int xPieceVal, int* usedSet) const;


    protected:
        virtual u64 xorHashKey(int pos) const override;
        virtual void gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool capOnly) const;

    private:
        mutable int seeMaxPly = 0;

        int toPieceCount(int* pieceCnt) const;

    public:
        using BoardData::pieceList_isDraw;
        virtual bool pieceList_isDraw(const int* pieceList) const override;

    protected:
        virtual bool pieceList_make(const Hist& hist) override;
        virtual bool pieceList_takeback(const Hist& hist) override;

        virtual bool pieceList_setEmpty(int* pieceList, int pos, int type, Side side) override;
        virtual bool pieceList_setPiece(int* pieceList, int pos, int type, Side side) override;
        virtual bool pieceList_isValid() const override;

        /// The PGN move-text parser: xiangqi's notations use '.' and '-' ("C2.5", "H2-3"),
        /// and a coordinate move list says by itself whether its ranks start from 0 or 1.
        virtual bool isMoveTextChar(char ch) const override { return ch == '.' || ch == '-'; }
        virtual void detectMoveFormat(const std::vector<MoveStringLevel>& moveStringVec, MoveFormat&) override;
    };

} // namespace banksia

#endif /* board_hpp */
