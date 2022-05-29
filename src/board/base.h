/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef base_h
#define base_h

#include <stdio.h>
#include <set>
#include <unordered_map>

#include <iomanip> // for setfill, setw

#include "types.h"
#include "chesstypes.h"

namespace bslib {
    
    ///////////////////////////////////

    class BoardData {
    public:
        ChessVariant variant;

        Side side;

        int status;
        Result result;

        int repetitionThreatHold = 1;
        int quietCnt, fullMoveCnt = 1;
        uint64_t hashKey;

        std::string startFen;

        mutable std::mutex dataMutex;

    protected:
        std::vector<Piece> pieces;
        std::vector<Hist> histList;
        std::string firstComment;

    public:

        int getHistListSize() const {
            return static_cast<int>(histList.size());
        }

        void histListClear() {
            std::lock_guard<std::mutex> dolock(dataMutex);
            histList.clear();
        }

        std::vector<Hist> getHistList() const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return histList;
        }

        bool isEngineScoreValid() const {
            for(auto && hist : histList) {
                if (!hist.isEngineScoreValid()) return false;
            }
            return true;
        }

        void updateLastHist(const Hist& h) {
            std::lock_guard<std::mutex> dolock(dataMutex);
            if (!histList.empty()) {
                histList.back() = h;
            }
        }

        Hist getLastHist() const {
            return getHistAt(static_cast<int>(histList.size()) - 1);
        }

        Hist _getLastHist() const {
            return _getHistAt(static_cast<int>(histList.size()) - 1);
        }

        Hist getHistAt(int idx) const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getHistAt(idx);
        }
        Hist _getHistAt(int idx) const {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? histList.at(idx) : Hist();
        }

        Hist* _getLastHistPointer() {
            return _getHistPointerAt(static_cast<int>(histList.size()) - 1);
        }
 
        const Hist* _getLastHistPointer() const {
            return _getHistPointerAt(static_cast<int>(histList.size()) - 1);
        }

        Hist* _getHistPointerAt(int idx) {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? &histList[idx] : nullptr;
        }
        
        const Hist* _getHistPointerAt(int idx) const {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? &histList[idx] : nullptr;
        }

        MoveFull getMoveAt(int idx) const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getMoveAt(idx);
        }
        MoveFull _getMoveAt(int idx) const {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? histList.at(idx).move : MoveFull();
        }
        void setFirstComment(const std::string& comment) {
            firstComment = comment;
        }
        std::string getFirstComment() const {
            return firstComment;
        }

        // low word is #computer info, high word is #comment
        int hist_countInfo() const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            auto k = 0;
            for(auto && hist : histList) {
                k += hist.countInfo();
            }
            return k;
        }

        bool hist_removeAllInfo(bool compInfo, bool comment) {
            std::lock_guard<std::mutex> dolock(dataMutex);
            auto cnt = 0;
            for(auto && hist : histList) {
                if (hist.removeInfo(compInfo, comment)) {
                    cnt++;
                }
            }
            return cnt;
        }

        int size() const {
            return int(pieces.size());
        }

        virtual bool isPositionValid(int pos) const {
            return pos >= 0 && pos < int(pieces.size());
        }

        Piece getPiece(int pos) const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getPiece(pos);
        }

        Piece _getPiece(int pos) const {
            assert(isPositionValid(pos));
            return pieces.at(size_t(pos));
        }

        bool isEmpty(int pos) const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _isEmpty(pos);
        }
        bool _isEmpty(int pos) const {
            assert(isPositionValid(pos));
            return pieces[size_t(pos)].type == EMPTY;
        }

        bool isPiece(int pos, int type, Side side) const {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _isPiece(pos, type, side);
        }

        bool _isPiece(int pos, int type, Side side) const {
            assert(isPositionValid(pos));
            auto p = pieces[size_t(pos)];
            return p.type == type && p.side == side;
        }

        virtual uint64_t key() const {
            return hashKey;
        }

        virtual void clone(const BoardData* oboard) {
            std::lock_guard<std::mutex> dolocko(oboard->dataMutex);
            std::lock_guard<std::mutex> dolock(dataMutex);
            _clone(oboard);
        }

        virtual void _clone(const BoardData* oboard) {
            pieces = oboard->pieces; assert(pieces.size() == oboard->pieces.size());
            side = oboard->side;
            status = oboard->status;
            histList = oboard->histList;
            result = oboard->result;
            variant = oboard->variant;

            repetitionThreatHold = oboard->repetitionThreatHold;
            quietCnt = oboard->quietCnt;
            fullMoveCnt = oboard->fullMoveCnt;
            hashKey = oboard->hashKey;
            startFen = oboard->startFen;
            firstComment = oboard->firstComment;
        }

        int getTotalPieceCount() const {
            auto cnt = 0;
            for(auto && p : pieces) {
                if (!p.isEmpty()) cnt++;
            }
            return cnt;
        }
    };

    class PgnRecord
    {
    public:
        int gameID = -1, eventID = -1, siteID = -1, whiteID = -1, blackID = -1;
        std::unordered_map<std::string, std::string> tags;
        std::string fenText, result, moveString;
        const char* moveText = nullptr;
    };

    /////////////////////////
    class BoardCore : public BoardData {

    public:
        BoardCore() {}
        virtual ~BoardCore() {}

        void reset() {
            for (auto && p : pieces) {
                p.setEmpty();
            }

            histList.clear();
            quietCnt = 0;
            result.result = ResultType::noresult;
        }

        virtual std::string toString() const {
            return "";
        }

        bool isValid(const Move& move) const {
            return move.isValid() && isPositionValid(move.from) && isPositionValid(move.dest);
        }
        bool isValid(const MoveFull& move) const {
            Move m = move;
            return isValid(m);
        }

        virtual int columnCount() const = 0;
        virtual int getColumn(int pos) const = 0;
        virtual int getRank(int pos) const = 0;

        virtual bool isValid() const {
            return false;
        }

        void setPiece(int pos, Piece piece) {
            std::lock_guard<std::mutex> dolock(dataMutex);
            _setPiece(pos, piece);
        }

        void _setPiece(int pos, Piece piece) {
            assert(isPositionValid(pos));
            pieces[size_t(pos)] = piece;
        }

        void setEmpty(int pos) {
            std::lock_guard<std::mutex> dolock(dataMutex);
            _setEmpty(pos);
        }

        void _setEmpty(int pos) {
            assert(isPositionValid(pos));
            pieces[size_t(pos)].setEmpty();
        }
        
        static Side xSide(Side side) {
            return side == Side::white ? Side::black : Side::white;
        }
        
        virtual std::string toString(const Piece&) const = 0;

        virtual std::string toString(const MoveFull&) const = 0;
        virtual std::string toString(const Move&) const = 0;
        virtual std::string toString(const Hist& hist, Notation notation) const;
        virtual std::string toString_coordinate(const MoveFull&) const;
        virtual std::string toString_traditional(const MoveFull& move, Notation notation) const;
        virtual std::string toString_san(const Hist& hist) const;

        virtual std::string toString_lan(const Hist& hist) const;
        static std::string toString_lan(const MoveFull&, ChessVariant);

        Move flip(const Move& move, FlipMode flipMode) const;
        MoveFull flip(const MoveFull& move, FlipMode flipMode) const;
        static FlipMode flip(FlipMode oMode, FlipMode flipMode);

        virtual int flip(int pos, FlipMode flipMode) const = 0;
        virtual void flip(FlipMode flipMode);

        virtual void flipPieceColors();

        virtual void createFullMoves(std::vector<MoveFull>& moveList, MoveFull m) const = 0;

        virtual bool canLocatePiece(int type, Side side, int pos) const = 0;
        virtual int attackerCnt() const;
        virtual void setupPieceCount() {}

        virtual bool isValidPromotion(int promotion, Side side) const = 0;;

        int getQuietCnt() const { return quietCnt; }
        void setQuietCnt(int k) { quietCnt = k; }

        int getFullMoveCnt() const { return fullMoveCnt; }
        void setFullMoveCnt(int k) { fullMoveCnt = k; }

        virtual std::vector<Move> getTransitionList(Hist hist) const {
            return std::vector<Move>({hist.move});
        }

        virtual bool sameContent(BoardCore*) const;

        virtual bool isValueSmaller(int type0, int type1) const = 0;

    public:
        void setFen(const std::string& fen);
        virtual void _setFen(const std::string& fen) = 0;
        virtual bool isFenValid(const std::string& fen) const = 0;

        virtual std::string getFen() const;
        virtual std::string getFen(int halfCount, int fullMoveCount) const = 0;

        virtual std::string getEPD() const;
        virtual std::string getEPD(const Hist&) const;

        virtual uint64_t initHashKey() const = 0;
        
        bool isHashKeyValid();
        bool _isHashKeyValid();

        void setHashKey(uint64_t key);
        
        virtual uint64_t getHashKeyForCheckingDuplicates(int = -1) const = 0;
        bool equalMoveLists(const BoardCore*, bool embeded) const;
        
        virtual std::string getLastEcoString() const = 0;

    public:
        bool fromOriginPosition() const;
        virtual std::string getStartingFen() const;
        std::string getUciPositionString(const Move& pondermove = Move::illegalMove) const;
        
        void newGame(std::string fen = "");
        virtual void _newGame(std::string fen = "");

        MoveFull createFullMove(int from, int dest, int promote) const;
        virtual int charactorToPieceType(char ch) const = 0;
        virtual bool isLegalMove(int from, int dest, int promotion = EMPTY);
        virtual bool _isLegalMove(int from, int dest, int promotion = EMPTY);

        Move moveFromString_coordinate(const std::string& moveString) const;
        virtual Move moveFromString_san(const std::string&) = 0;

        virtual Result rule() = 0;

        virtual int getRawMaterialScore() const = 0;

        void genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide);
        void genLegal(std::vector<MoveFull>& moves, Side side, int from = -1, int dest = -1, int promotion = EMPTY);
        bool isIncheck(Side beingAttackedSide) const;
        void gen(std::vector<MoveFull>& moveList, Side attackerSide) const;

        virtual void _genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide);
        virtual void _genLegal(std::vector<MoveFull>& moves, Side side, int from = -1, int dest = -1, int promotion = EMPTY);

        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide) const = 0;
        virtual bool _isIncheck(Side beingAttackedSide) const = 0;

        bool checkMake(int from, int dest, int promotion);
        virtual bool _checkMake(int from, int dest, int promotion) = 0;
        virtual bool _quickCheckMake(int from, int dest, int promotion, bool createSanString) = 0;
        virtual bool _quickCheck_rook(int from, int dest, bool checkMiddle) const;

        enum ParseMoveListFlag {
            ParseMoveListFlag_quick_check       = 1 << 0,
            ParseMoveListFlag_create_san        = 1 << 1,
            ParseMoveListFlag_create_fen        = 1 << 2,
            ParseMoveListFlag_create_bitboard   = 1 << 3,
            ParseMoveListFlag_discardComment    = 1 << 4,
            ParseMoveListFlag_parseComment      = 1 << 5,
            
            ParseMoveListFlag_move_size_1_byte  = 1 << 6, // for the 2nd function one only
        };
        
        virtual bool fromMoveList(const PgnRecord* record, Notation, int flag,
                                  std::function<bool(const std::vector<uint64_t>&, const BoardCore*, const PgnRecord*)> = nullptr);

        virtual bool fromMoveList(const PgnRecord*, const std::vector<int8_t>& moveVec, int flag, std::function<bool(const std::vector<uint64_t>& bitboardVec, const BoardCore*, const PgnRecord*)> = nullptr);

        std::vector<HistBasic> parsePv(const std::string& pvString, bool isCoordinateOnly);
        std::vector<HistBasic> _parsePv(const std::string& pvString, bool isCoordinateOnly);

        void _parseComment(const std::string& comment, Hist&);
        void _parseComment_standard(const std::string& comment, Hist&);
        void _parseComment_tcec(const std::string& comment, Hist&);

        virtual std::string toMoveListString(Notation notation, int itemPerLine = 10000000, bool moveCounter = false, CommentComputerInfoType computingInfo = CommentComputerInfoType::none, bool pawnUnit = false, int precision = 1) const;
        static std::string toMoveListString(const std::vector<Hist>& histList, ChessVariant variant, Notation notation, int itemPerLine, bool moveCounter, CommentComputerInfoType computingInfo, bool pawnUnit, int precision);

        virtual std::string toSimplePgn() const;
        virtual std::string toPgn(const PgnRecord*, bool useBoard = true) const;

        static void addCompulsoryPGNTags(std::unordered_map<std::string, std::string>& tags);

        virtual int16_t move2i16(int from, int dest, int promotion, bool haveComment) const = 0;
        virtual void i16ToMove(int data, int& from, int& dest, int& promotion, bool& haveComment) const = 0;

        virtual char pieceType2Char(int pieceType) const = 0;
        virtual int coordinateStringToPos(const std::string& str) const = 0;
        virtual std::string posToCoordinateString(int pos) const = 0;

        void make(const MoveFull& move);
        void takeBack();

        virtual void _make(const MoveFull& move);
        virtual void _takeBack();

        void make(const MoveFull& move, Hist& hist);
        void takeBack(const Hist& hist);

        virtual void _make(const MoveFull& move, Hist& hist) = 0;
        virtual void _takeBack(const Hist& hist) = 0;

        virtual int findKing(Side side) const;
        virtual std::vector<uint64_t> posToBitboards() const = 0;

    protected:
        virtual bool createSanStringForLastMove() = 0;

        void setupPieceIndexes();

        virtual uint64_t xorHashKey(int pos) const = 0;
        virtual uint64_t xorSideHashKey() const = 0;

    };
    

} // namespace ocgdb

#endif /* board_hpp */


