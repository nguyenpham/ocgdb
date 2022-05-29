/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef chess_h
#define chess_h

#include <stdio.h>
#include "base.h"

namespace bslib {

    const int CastleRight_long  = (1<<0);
    const int CastleRight_short = (1<<1);
    const int CastleRight_mask  = (CastleRight_long|CastleRight_short);


    class ChessBoard : public BoardCore {
    protected:
        int enpassant = 0;
        int8_t castleRights[2];
        int castleRights_column_king = 4, castleRights_column_rook_left = 0, castleRights_column_rook_right = 7;

    public:
        ChessBoard(ChessVariant _variant = ChessVariant::standard);
        ChessBoard(const ChessBoard&);
        virtual ~ChessBoard() override;

        virtual std::string toString() const override;
        virtual bool isValid() const override;

        virtual int columnCount() const override;
        virtual int getColumn(int pos) const override;
        virtual int getRank(int pos) const override;

        virtual void _setFen(const std::string& fen) override;

        using BoardCore::getFen;
        virtual std::string getFen(int halfCount, int fullMoveCount) const override;

        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide) const override;
        virtual bool _isIncheck(Side beingAttackedSide) const override;

        using BoardCore::_make;
        using BoardCore::_takeBack;
        using BoardCore::_checkMake;

        virtual void _make(const MoveFull& move, Hist& hist) override;
        virtual void _takeBack(const Hist& hist) override;
        virtual bool _checkMake(int from, int dest, int promotion) override;
        virtual bool _quickCheckMake(int from, int dest, int promotion, bool createSanString) override;

        virtual int charactorToPieceType(char ch) const override;
        
        static std::vector<Move> coordinateString2Moves(const std::string& str);
        
        using BoardCore::toString;
        
        virtual char pieceType2Char(int pieceType) const override;
        virtual std::string piece2String(const Piece& piece, bool alwayLowerCase);

        static std::string chessPiece2String(const Piece& piece, bool alwayLowerCase);
        static std::string chessPieceType2FullString(int pieceType);
        virtual std::string toString(const Piece&) const override;
        virtual std::string toString(const Move&) const override;
        virtual std::string toString(const MoveFull&) const override;
                
        static std::string moveString_coordinate(const Move& move);
        static std::string hist2String(const HistBasic&, Notation);

        virtual std::string toString_traditional(const MoveFull& move, Notation) const override
        {
            assert(false);
            return toString(move);
        }

        virtual int coordinateStringToPos(const std::string& str) const override;
        virtual std::string posToCoordinateString(int pos) const override;

        static Move chessMoveFromCoordiateString(const std::string& moveString);
        virtual int16_t move2i16(int from, int dest, int promotion, bool haveComment) const override;
        virtual void i16ToMove(int data, int& from, int& dest, int& promotion, bool& haveComment) const override;

        using BoardCore::flip;
        virtual int flip(int, FlipMode) const override {
            assert(0);
            return 0;
        }

        virtual int getRawMaterialScore() const override;

        virtual void createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const override;
        virtual bool canLocatePiece(int type, Side side, int pos) const override;

        virtual std::vector<Move> getTransitionList(Hist hist) const override;
        virtual void _clone(const BoardData* oboard) override;

        int8_t getCastleRights(int sd) const {
            return castleRights[sd];
        }

        void setEnpassant(int _enpassant) {
            enpassant = _enpassant;
        }

        void setCastleRights(int sd, int8_t rights) {
            castleRights[sd] = rights;
        }

        int getEnpassant() const {
            return enpassant;
        }

        virtual Move moveFromString_san(const std::string&) override;
        virtual Move moveFromString_castling(const std::string& str, Side side) const;

        static bool isChessFenValid(const std::string& fen);
        static void staticInit();

        static uint64_t _posToBitboard[64];

        static uint64_t posToBitboard(const char* s);
        static std::string bitboard2string(uint64_t bb);

        static uint16_t encode2Bytes(Move move);
        static Move decode2Bytes(uint16_t d);
        static std::pair<uint16_t, int> encode1Byte(MoveFull move);
        std::pair<Move, int> decode1Byte(const int8_t* d);

        virtual uint64_t getHashKeyForCheckingDuplicates(int) const override;

        virtual std::string getLastEcoString() const override;

    protected:
        bool _quickCheck_bishop(int from, int dest, bool checkMiddle) const;
        virtual bool _quickCheck_king(int from, int dest, bool checkMiddle) const;


        std::vector<int> _attackByRook(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByBishop(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByQueen(int from, int dest, const Piece& movePiece);
        std::vector<int> _attackByPawn(int from, int dest, const Piece& movePiece, const Piece& cap, int enpassant);
        std::vector<int> _attackByKnight(int dest, const Piece& movePiece);
        virtual bool createSanStringForLastMove() override;

        virtual bool isValidPromotion(int promotion, Side) const override {
            return promotion > KING && promotion < static_cast<int>(PieceTypeStd::pawn);
        }

        virtual bool isFenValid(const std::string& fen) const override;


        Result rule() override;
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

        void gen_addMove(std::vector<MoveFull>& moveList, int from, int dest, bool capOnly) const;

        virtual std::vector<uint64_t> posToBitboards() const override;

    protected:
        uint64_t hashKeyEnpassant(int enpassant) const;
        virtual uint64_t xorHashKey(int pos) const override;
        virtual uint64_t xorSideHashKey() const override;

    private:
        void checkEnpassant();
        
        virtual uint64_t initHashKey() const override;

        int toPieceCount(int* pieceCnt) const;
        
    private:
        bool createStringForLastMove(const std::vector<MoveFull>& moveList);
        
        void gen_addPawnMove(std::vector<MoveFull>& moveList, int from, int dest, bool capOnly) const;
    };
    
    extern const char* pieceTypeFullNames[8];

} // namespace ocgdb

#endif /* board_h */


