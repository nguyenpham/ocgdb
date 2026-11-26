/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_CHESS_CHESS_H
#define BANKSIA_CHESS_CHESS_H

#include <stdio.h>
#include "base/base.h"
#include "chessbitboard.h"

namespace banksia {

    const int CastleRight_long = (1 << 0);
    const int CastleRight_short = (1 << 1);
    const int CastleRight_mask = (CastleRight_long | CastleRight_short);


    class ChessBoard : public BoardCore
    {
    protected:
        int enpassant = 0;
        int8_t castleRights[2];
        int castleRights_column_king = 4, castleRights_column_rook_left = 0,
            castleRights_column_rook_right = 7;

    public:
        ChessBoard(ChessVariant _variant = ChessVariant::standard);
        ChessBoard(const ChessBoard&);
        virtual ~ChessBoard() override;

        virtual std::string toString() const override;
        virtual bool isValid() const override;

        virtual bool _setFen(const std::string& fen) override;

        using BoardCore::getFen;
        virtual std::string getFen(bool enpassantLegal, int halfCount, int fullMoveCount, bool horizontalFlip,
                                   const MoveFormat&) const override;

        using BoardCore::_gen;
        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide, int pieceType) const override;
        virtual bool _isIncheck(Side beingAttackedSide) const override;

        using BoardCore::_checkMake;
        using BoardCore::_make;
        using BoardCore::_takeBack;

        virtual void _make(const MoveFull& move, Hist& hist) override;

        virtual void _takeBack(const Hist& hist) override;
        virtual bool _checkMake(int from, int dest, int promotion) override;
        virtual bool _quickCheckMake(int from, int dest, int promotion, bool createSanString) override;

        bool _quickCheck_bishop(int from, int dest, bool checkMiddle) const;
        virtual bool _quickCheck_king(int from, int dest, bool checkMiddle) const;

        virtual Eco getEco(u64 key) const override;
        virtual bool isEcoName(const std::string name) const override;

        /// OCGDB additions (chess_db.cpp): bitboards for PQL, the binary move fields of a
        /// database, duplicate keys. staticInit() must be called once before any of them.
        static void staticInit();
        static uint64_t _posToBitboard[64];
        static uint64_t bb_edge_left, bb_edge_right, bb_edge_top, bb_edge_bottom;

        static uint64_t posToBitboard(const char* s);
        static std::string bitboard2string(uint64_t bb);

        virtual BitboardBase* posToBitboards() const override;

        virtual uint16_t encode2Bytes(const Hist&) const override;
        virtual Move decode2Bytes(uint16_t d) const override;
        virtual std::pair<uint16_t, int> encode1Byte(MoveFull move) override;
        virtual std::pair<Move, int> decode1Byte(const int8_t* d) override;

        virtual uint64_t getHashKeyForCheckingDuplicates(int) const override;

        /// PQL's "#" patterns: every place a pattern can stand on the board
        static void pattern_shift(std::vector<BitboardBase*>& patternBitBoards);
        static bool pattern_shift_up(ChessBitboard*);
        static bool pattern_shift_down(ChessBitboard*);
        static bool pattern_shift_left(ChessBitboard*);
        static bool pattern_shift_right(ChessBitboard*);


        virtual Result probeEgtb(int maxPieces, bool& tberror) const override;

        virtual int attackerCnt() const override;
        virtual bool hasAttackers() const override;
        virtual bool movesCarryPromotion() const override { return true; }
        virtual bool canMoveOntoOwnPiece(const Piece& piece, const Piece& target) const override;
        virtual bool isPawnMove(const MoveFull& move) const override { return move.piece.type == PAWNSTD; }
        virtual std::vector<MoveFull> probeEgtbAll() const override;

        virtual int charactorToPieceType(char ch) const override;

        static std::vector<Move> coordinateString2Moves(const std::string& str);

        using BoardCore::toString;

        virtual char pieceType2Char(int pieceType) const override;
        virtual std::string piece2String(const Piece& piece, bool alwayLowerCase) const;

        static std::string chessPiece2String(const Piece& piece, bool alwayLowerCase);
        virtual std::string toString(const Piece&) const override;
        virtual std::string toString(const Move&, const MoveFormat&) const override;
        virtual std::string toString(const MoveFull&, const MoveFormat&) const override;
        virtual std::string toString_san(const Hist& hist, const MoveFormat&) const override;

        static std::string moveString_coordinate(const Move& move);
        static std::string hist2String(const HistBasic&, Notation);

        /// Chess has no traditional notation: a move list asked for one gets coordinates.
        virtual std::string toString_traditional(const MoveFull& move, Notation,
                                                 const MoveFormat& mf) const override
        {
            return toString(move, mf);
        }

        virtual int coordinateStringToPos(const std::string& str, const MoveFormat&) const override;
        virtual std::string posToCoordinateString(int pos, const MoveFormat&) const override;

        static Move chessMoveFromCoordiateString(const std::string& moveString);

        using BoardCore::flip;
        virtual void flip(FlipMode flipMode) override;
        virtual int flip(int pos, FlipMode flipMode) const override { return csFlip(pos, flipMode); }

        static int csFlip(int pos, FlipMode flipMode);

        virtual int getRawMaterialScore() const override;

        virtual void createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const override;
        virtual bool canLocatePiece(int type, Side side, int pos) const override;

        virtual void _clone(const BoardData* oboard) override;

        int8_t getCastleRights(int sd) const { return castleRights[sd]; }

        void setCastleRights(int sd, int8_t rights) { castleRights[sd] = rights; }

        int getEnpassant() const { return enpassant; }

        void setEnpassant(int _enpassant) { enpassant = _enpassant; }


        virtual Move moveFromString_san(const std::string&, const MoveFormat&) override;
        virtual Move moveFromString_castling(const std::string& str, Side side) const;

        static bool isChessFenValid(const std::string& fen);

        virtual int materials() const override;
        virtual int see(const Move&) const override;

    protected:
        bool canRivalCaptureEnpassant() const;

        virtual bool isValidPromotion(int promotion, Side) const override
        {
            return promotion > KING && promotion < static_cast<int>(PieceTypeStd::pawn);
        }

        virtual bool isFenValid(const std::string& fen) const override;
        virtual void _setFenComplete() override;


        Result rule() override;
        virtual Result ruleRepetition(int repeatLen) override;

        virtual bool beAttacked(int pos, Side attackerSide) const;

        virtual void genPawn(std::vector<MoveFull>& moves, Side side, int pos) const;
        virtual void genKnight(std::vector<MoveFull>& moves, Side side, int pos) const;
        virtual void genRook(std::vector<MoveFull>& moves, Side side, int pos, bool oneStep) const;
        virtual void genBishop(std::vector<MoveFull>& moves, Side side, int pos, bool oneStep) const;


        virtual void gen_castling(std::vector<MoveFull>& moveList, int kingPos) const;

        virtual bool isValidCastleRights() const;
        virtual void setFenCastleRights_clear();
        virtual void setFenCastleRights(const std::string& string);
        virtual std::string getFenCastleRights() const;
        virtual void clearCastleRights(int rookPos, Side rookSide);
        virtual bool isValueSmaller(int type0, int type1) const override;

        virtual void gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool capOnly) const;


        u64 hashKeyEnpassant(int enpassant) const;
        virtual u64 xorHashKey(int pos) const override;
        virtual u64 xorSideHashKey() const override;

        std::vector<int> see_genAttack(Side side, int pos, const int* usedSet) const;
        int see_capture(Side side, int pos, int xPieceVal, int* usedSet) const;


        void checkEnpassant();

        virtual u64 initHashKey() const override;

        int toPieceCount(int* pieceCnt) const;

    private:
        std::vector<int> _attackByRook(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByBishop(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByQueen(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByPawn(int from, int dest, const Piece& movePiece, const Piece& cap,
                                       int enpassant);
        std::vector<int> _attackByKnight(int dest, const Piece& movePiece);

        bool createStringForLastMove(const std::vector<MoveFull>& moveList);
        virtual bool createSanStringForLastMove(const MoveFormat&) override;

        void gen_addPawnMove(std::vector<MoveFull>& moveList, int from, int dest, bool capOnly) const;

    public:
        using BoardData::pieceList_isDraw;
        virtual bool pieceList_isDraw(const int* pieceList) const override;

    protected:
        virtual bool pieceList_make(const Hist& hist) override;
        virtual bool pieceList_takeback(const Hist& hist) override;

        virtual bool pieceList_setEmpty(int* pieceList, int pos, int type, Side side) override;
        virtual bool pieceList_setPiece(int* pieceList, int pos, int type, Side side) override;
        virtual bool pieceList_isValid() const override;
    };

    extern const char* pieceTypeFullNames[8];

} // namespace banksia

#endif /* board_hpp */
