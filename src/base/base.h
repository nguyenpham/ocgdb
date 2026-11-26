/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_BASE_H
#define BANKSIA_BASE_BASE_H

#include <stdio.h>
#include <set>
#include <unordered_map>
#include <mutex>      // std::mutex, std::lock_guard (used below; do not rely on transitive includes)

#include <iomanip> // for setfill, setw
#include <functional>
#include <cstring>

#include "comm.h"
#include "enginescore.h"
#include "bitboardbase.h"


namespace banksia {

    class BoardCore;
    class PgnRecord;

    /// OCGDB addition (from Banksia GUI's core): four moves of a game's main line (the first,
    /// two in the middle, the last), a quick key for finding duplicate games
    /// (BoardCore::createCheckingDuplicateRec).
    class DupRec
    {
    public:
        DupRec() { reset(); }

        int32_t idx = -1;
        uint16_t moves[4];

        void reset() { memset(moves, 0, sizeof(moves)); }

        bool operator==(const DupRec& o) const { return memcmp(moves, o.moves, sizeof(moves)) == 0; }
    };

    ///////////////////////////////////


    class BoardData
    {
    public:
        Side side;

        int status;
        Result result;

        int repetitionThreatHold = 2; // 2 + 1 itself = 3 times
        bool xqRepetitionFofeiteForLastMoveOnly = false;
        int quietCnt, fen_quietCnt, fen_fullMoveCnt = 1;
        u64 hashKey;

        std::string startFen;

        mutable std::mutex dataMutex;

        int pieceList[2][16];

        ChessVariant variant;

    protected:
        std::vector<Piece> pieces;
        std::vector<Hist> histList;

        std::string firstComment;

    public:
        void reset()
        {
            for (auto&& p : pieces) {
                p.setEmpty();
            }

            histList.clear();
            quietCnt = 0;
            result.result = ResultType::noresult;
            pieceList_reset((int*)pieceList);
        }


        virtual void _clone(const BoardData* oboard)
        {
            pieces = oboard->pieces;
            assert(pieces.size() == oboard->pieces.size());
            side = oboard->side;
            status = oboard->status;
            histList = oboard->histList;
            result = oboard->result;
            variant = oboard->variant;

            repetitionThreatHold = oboard->repetitionThreatHold;
            quietCnt = oboard->quietCnt;
            fen_quietCnt = oboard->fen_quietCnt;
            fen_fullMoveCnt = oboard->fen_fullMoveCnt;
            hashKey = oboard->hashKey;
            startFen = oboard->startFen;
            firstComment = oboard->firstComment;

            memcpy(pieceList, oboard->pieceList, sizeof(pieceList));
            assert(hashKey == oboard->hashKey);
        }


        int getHistListSize() const { return static_cast<int>(histList.size()); }

        /// OCGDB addition. The hists of the main line, in order: the history also keeps the
        /// moves of the variations read from a PGN or a database, so getHistListSize() of an
        /// annotated game counts them too (a PlyCount of 96 for a main line of 61).
        std::vector<const Hist*> _mainLineHists() const
        {
            std::vector<const Hist*> v;
            v.reserve(histList.size());
            for (auto&& h : histList) {
                if (h.variationRec.level == 0) {
                    v.push_back(&h);
                }
            }
            return v;
        }

        /// OCGDB addition: the number of half-moves of the main line
        int getMainLineSize() const
        {
            int n = 0;
            for (auto&& h : histList) {
                if (h.variationRec.level == 0) {
                    ++n;
                }
            }
            return n;
        }

        void histListClear()
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            histList.clear();
        }

        std::vector<Hist> getHistList() const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return histList;
        }

        void copyHistListTo(std::vector<Hist>& toHistList) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            toHistList = histList;
            assert(toHistList.size() == histList.size());
        }

        bool isEngineScoreValid() const
        {
            for (auto&& hist : histList) {
                if (!hist.isEngineScoreValid()) return false;
            }
            return true;
        }

        void updateLastHist(const Hist& h)
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            if (!histList.empty()) {
                auto sanString = histList.back().sanString;
                histList.back() = h;
                if (h.sanString.empty()) {
                    histList.back().sanString = sanString;
                }
            }
        }

        Hist getLastHist() const { return getHistAt(static_cast<int>(histList.size()) - 1); }

        Hist _getLastHist() const { return _getHistAt(static_cast<int>(histList.size()) - 1); }

        Hist getHistAt(int idx) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getHistAt(idx);
        }

        Hist _getHistAt(int idx) const
        {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? histList.at(idx) : Hist();
        }

        Hist* _getLastHistPointer() { return _getHistPointerAt(static_cast<int>(histList.size()) - 1); }

        const Hist* _getLastHistPointer() const
        {
            return _getHistPointerAt(static_cast<int>(histList.size()) - 1);
        }

        Hist* _getHistPointerAt(int idx)
        {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? &histList[idx] : nullptr;
        }

        const Hist* _getHistPointerAt(int idx) const
        {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? &histList[idx] : nullptr;
        }

        bool _removeHist(int atIdx)
        {
            if (atIdx >= 0 && atIdx < static_cast<int>(histList.size())) {
                histList.erase(histList.begin() + atIdx);
                return true;
            }
            return false;
        }

        MoveFull getMoveAt(int idx) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getMoveAt(idx);
        }
        MoveFull _getMoveAt(int idx) const
        {
            return idx >= 0 && idx < static_cast<int>(histList.size()) ? histList.at(idx).move : MoveFull();
        }

        void setFirstComment(const std::string& comment) { firstComment = comment; }

        std::string getFirstComment() const { return firstComment; }

        bool hasComments() const
        {
            if (!firstComment.empty()) {
                return true;
            }
            for (auto&& hist : histList) {
                if (!hist.comment.empty()) {
                    return true;
                }
            }
            return false;
        }

        /// low word is #computer info, high word is #comment
        int hist_countInfo() const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            auto k = 0;
            for (auto&& hist : histList) {
                k += hist.countInfo();
            }
            return k;
        }

        bool hist_removeAllInfo(bool compInfo, bool comment)
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            auto cnt = 0;

            if (comment && !firstComment.empty()) {
                firstComment = "";
                cnt++;
            }
            for (auto&& hist : histList) {
                if (hist.removeInfo(compInfo, comment)) {
                    cnt++;
                }
            }
            return cnt;
        }

        int size() const { return int(pieces.size()); }

        virtual bool isPositionValid(int pos) const { return pos >= 0 && pos < int(pieces.size()); }

        virtual void _setPiece(int pos, Piece piece) = 0;

        Piece getPiece(int pos) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _getPiece(pos);
        }

        std::vector<Piece> getAllPieces() const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return pieces;
        }

        std::vector<Piece> _getAllPieces() const { return pieces; }

        Piece _getPiece(int pos) const
        {
            assert(isPositionValid(pos));
            return pieces.at(size_t(pos));
        }

        bool isEmpty(int pos) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _isEmpty(pos);
        }
        bool _isEmpty(int pos) const
        {
            assert(isPositionValid(pos));
            return pieces[size_t(pos)].type == EMPTY;
        }
        bool _isEmpty() const
        {
            for (auto&& piece : pieces) {
                if (!piece.isEmpty()) return false;
            }
            return true;
        }

        bool isPiece(int pos, int type, Side side) const
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            return _isPiece(pos, type, side);
        }

        bool _isPiece(int pos, int type, Side side) const
        {
            assert(isPositionValid(pos));
            auto p = pieces[size_t(pos)];
            return p.type == type && p.side == side;
        }

        virtual u64 key() const { return hashKey; }

        virtual void clone(const BoardData* oboard)
        {
            std::lock_guard<std::mutex> dolocko(oboard->dataMutex);
            std::lock_guard<std::mutex> dolock(dataMutex);
            _clone(oboard);
        }

        int getTotalPieceCount() const
        {
            auto cnt = 0;
            for (auto&& p : pieces) {
                if (!p.isEmpty()) cnt++;
            }
            return cnt;
        }


        static void pieceList_reset(int* pieceList);
        bool pieceList_isDraw() const;

        void pieceList_setupAll();

    protected:
        static bool pieceList_setEmpty(int* pieceList, int pos);
        static bool pieceList_setEmpty(int* pieceList, int pos, int sd);
        static bool pieceList_hasPos(const int* pieceList, int pos);
        static bool pieceList_hasPos(const int* pieceList, int pos, Side side);
        static bool pieceList_hasDoublePos(const int* pieceList);

        virtual bool pieceList_make(const Hist& hist) = 0;
        virtual bool pieceList_takeback(const Hist& hist) = 0;

        virtual bool pieceList_setEmpty(int* pieceList, int pos, int type, Side side) = 0;
        virtual bool pieceList_setPiece(int* pieceList, int pos, int type, Side side) = 0;
        virtual bool pieceList_isValid() const = 0;
        virtual bool pieceList_isDraw(const int* pieceList) const = 0;
    };

    /////////////////////////

    enum ParseMoveListFlag {
        ParseMoveListFlag_quick_check = 1 << 0,
        ParseMoveListFlag_create_san = 1 << 1,
        ParseMoveListFlag_create_fen = 1 << 2,
        /// OCGDB addition: build the bitboards of every position and give them to the
        /// fromMoveList() callback (PQL searches)
        ParseMoveListFlag_create_bitboard = 1 << 3,

        ParseMoveListFlag_discardComment = 1 << 4,
        ParseMoveListFlag_smartComment = 1 << 5,

        ParseMoveListFlag_move_size_1_byte = 1 << 7, // for the 2nd function one only

        ParseMoveListFlag_ignore_high_levels = 1 << 8, // for the 2nd function one only

    };

    class PgnRecord
    {
    public:
        int32_t gameID = -1, eventID = -1, siteID = -1, whiteID = -1, blackID = -1;
        int errCnt = 0;
        std::unordered_map<std::string, std::string> tags;
        std::string fenText, result, moveString, errString;
        const char* moveText = nullptr;
        banksia::ChessVariant variant;
    };

    /// OCGDB addition: a move of a PGN move text, as text, with its variation
    class MoveStringLevel
    {
    public:
        MoveStringLevel() { }
        MoveStringLevel(const std::string& moveString, const VariationRec& variationRec)
            : moveString(moveString), variationRec(variationRec)
        {
        }

        std::string moveString;
        VariationRec variationRec;
    };

    /// OCGDB addition: a move decoded from a database's binary move field, with its variation
    class MoveWithLevel
    {
    public:
        MoveWithLevel() { }
        MoveWithLevel(const Move& move, const VariationRec& variationRec)
            : move(move), variationRec(variationRec)
        {
        }

        Move move;
        VariationRec variationRec;
        /// a 2-byte code that can only be decoded on the board of its move (a jieqi reveal names
        /// its piece by index): decoded when the move is made; 0 when `move` is decoded already
        uint16_t code = 0;
    };

    /// OCGDB addition: called by fromMoveList() at every position of a game (before each move
    /// and at the end) with the position's bitboards (null without
    /// ParseMoveListFlag_create_bitboard), the board and the game. Returning true stops the parse.
    using PositionCallback = std::function<bool(const BitboardBase*, const BoardCore*, const PgnRecord*)>;

    /////////////////////////
    class BoardCore : public BoardData, public Obj
    {
    public:
        BoardCore() { }
        virtual ~BoardCore() { }

        using Obj::isValid;

        bool isValid(const Move& move) const
        {
            return move.isValid() && isPositionValid(move.from) && isPositionValid(move.dest);
        }
        bool isValid(const MoveFull& move) const
        {
            Move m = move;
            return isValid(m);
        }


        static int columnCount() { return COLUMN_COUNT; }
        static int rankCount() { return ROW_COUNT; }

        static int getColumn(int pos)
        {
            assert(pos >= 0 && pos < BOARD_SZ);
            return pos % COLUMN_COUNT;
        }

        static int getRow(int pos)
        {
            assert(pos >= 0 && pos < BOARD_SZ);
            return pos / COLUMN_COUNT;
        }

        void setPiece(int pos, Piece piece)
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            _setPiece(pos, piece);
        }

        virtual void _setPiece(int pos, Piece piece) override
        {
            assert(isPositionValid(pos));
            pieces[size_t(pos)] = piece;

            pieceList_setPiece((int*)pieceList, pos, piece.type, piece.side);
        }

        void setEmpty(int pos)
        {
            std::lock_guard<std::mutex> dolock(dataMutex);
            _setEmpty(pos);
        }

        void _setEmpty(int pos)
        {
            assert(isPositionValid(pos));
            pieces[size_t(pos)].setEmpty();
        }

        static Side xSide(Side side) { return side == Side::white ? Side::black : Side::white; }

        static Side getXSide(Side side) { return xSide(side); }

        using Obj::toString;
        virtual std::string toString(const Piece&) const = 0;

        virtual std::string toString(const MoveFull&);
        virtual std::string toString(const Move&);

        virtual std::string toString(const MoveFull&, const MoveFormat&) const = 0;
        virtual std::string toString(const Move&, const MoveFormat&) const = 0;

        virtual std::string toString(const Hist& hist, Notation) const;
        virtual std::string toString(const Hist& hist, Notation, const MoveFormat&) const;
        virtual std::string toString_coordinate(const MoveFull&) const;
        virtual std::string toString_coordinate(const MoveFull&, const MoveFormat&) const;

        virtual std::string toString_traditional(const MoveFull& move, Notation notation,
                                                 const MoveFormat&) const;
        virtual std::string toString_san(const Hist& hist) const;
        virtual std::string toString_san(const Hist& hist, const MoveFormat&) const = 0;

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
        virtual int attackerCnt() const = 0;
        virtual bool hasAttackers() const = 0;

        /// Variant rules the shared code asks about.
        /// Stalemate loses (xiangqi family) rather than draws (chess family).
        virtual bool stalemateLoses() const { return false; }
        /// Flipping vertically/rotating also swaps the sides of the men (xiangqi family),
        /// so the flipped game is a real game and its history can be replayed.
        virtual bool flipSwapsSides() const { return false; }
        /// Moves carry a promotion that tells them apart (chess promotions, jieqi
        /// revelations), so a legality query naming one must match it.
        virtual bool movesCarryPromotion() const { return false; }
        /// A move may land on a man of its own side (chess960 castling: king takes rook).
        virtual bool canMoveOntoOwnPiece(const Piece& /*piece*/, const Piece& /*target*/) const { return false; }
        /// A pawn move: it ends the quiet stretch draw adjudication looks for (chess).
        virtual bool isPawnMove(const MoveFull&) const { return false; }
        virtual void setupPieceCount() { }

        virtual bool isValidPromotion(int promotion, Side side) const = 0;

        int getQuietCnt() const { return quietCnt; }
        void setQuietCnt(int k) { quietCnt = k; }

        int getFullMoveCnt() const { return fen_fullMoveCnt; }
        void setFullMoveCnt(int k) { fen_fullMoveCnt = k; }

        virtual bool sameContent(BoardCore*) const;

        virtual bool isValueSmaller(int type0, int type1) const = 0;


        static std::set<ChessVariant> detectVariantFromFENString(const std::string&);

        int getAttackerCount() const
        {
            if (isChessFamily(variant)) {
                return getTotalPieceCount();
            }
            auto cnt = 0;
            for (auto&& p : pieces) {
                if (p.type >= ROOKXQ) {
                    cnt++;
                }
            }
            return cnt;
        }


        void setFen(const std::string& fen);
        virtual bool _setFen(const std::string& fen) = 0;
        virtual void _setFenComplete() = 0;

        virtual bool isFenValid(const std::string& fen) const = 0;

        // enpassantLegal: true = cound enpassant only if the rival can capture the enpassant Pawn
        virtual std::string getFen(bool enpassantLegal = false, bool horizontalFlip = false) const;
        virtual std::string getFen(bool enpassantLegal, int halfCount, int fullMoveCount,
                                   bool horizontalFlip) const;

        virtual std::string getEPD(bool enpassantLegal, bool withRecords, bool horizontalFlip = false) const;
        virtual std::string getEPD(bool enpassantLegal, bool withRecords, const Hist&,
                                   bool horizontalFlip = false) const;
        std::map<std::string, std::string> getEPDMap(bool enpassantLegal, const Hist&,
                                                     bool horizontalFlip) const;

        virtual std::string getFen(bool enpassantLegal, bool horizontalFlip, const MoveFormat&) const;
        virtual std::string getFen(bool enpassantLegal, int halfCount, int fullMoveCount, bool horizontalFlip,
                                   const MoveFormat&) const = 0;

        virtual std::string getEPD(bool enpassantLegal, bool withRecords, bool horizontalFlip,
                                   const MoveFormat&) const;
        virtual std::string getEPD(bool enpassantLegal, bool withRecords, const Hist&, bool horizontalFlip,
                                   const MoveFormat&) const;
        std::map<std::string, std::string> getEPDMap(bool enpassantLegal, const Hist&, bool horizontalFlip,
                                                     const MoveFormat&) const;
        static std::string EPDMap2String(const std::map<std::string, std::string>&);

        virtual u64 initHashKey() const = 0;

        bool isHashKeyValid();
        bool _isHashKeyValid();

        void setHashKey(u64 key);

        void setupPlyCounters();
        void _setupPlyCounters();


        bool fromOriginPosition() const;
        virtual std::string getStartingFen() const;
        virtual std::string getStartingFen(const MoveFormat&) const;
        std::string getUciPositionString(const Move& pondermove = Move::illegalMove,
                                         const MoveFormat& = MoveFormat(),
                                         bool forceFen = false) const;

        bool newGame(std::string fen = "");
        virtual bool _newGame(std::string fen = "");

        bool newGame(std::string fen, const std::vector<Hist>& startMoves, bool createSanString);
        virtual bool _newGame(std::string fen, const std::vector<Hist>& startMoves, bool createSanString);

        void _clear();

        MoveFull createFullMove(int from, int dest, int promote) const;
        virtual int charactorToPieceType(char ch) const = 0;
        virtual bool isLegalMove(int from, int dest, int promotion = EMPTY);
        virtual bool _isLegalMove(int from, int dest, int promotion = EMPTY);

        virtual Move moveFromString_coordinate(const std::string& moveString, const MoveFormat&) const;
        /// Characters of the from-square in a coordinate move ("e2" = 2); xiangqi's
        /// "rank from 1" format writes the tenth rank with two digits.
        virtual size_t coordinateFromLength(int /*from*/, const MoveFormat&) const { return 2; }
        virtual Move moveFromString_san(const std::string&, const MoveFormat&) = 0;
        virtual Move moveFromString_xq(const std::string&, const MoveFormat&) { return Move::illegalMove; }

        virtual Result rule();
        virtual Result ruleRepetition(int repeatLen) = 0;

        virtual Result adjudicationDraw(bool oneSide, int minPly, int plyRange, int scoreRange) const;
        virtual Result adjudicationScoreResign(bool oneSide, int plyRange, int scoreRange) const;

        virtual int getRawMaterialScore() const = 0;

        void genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide);
        void genLegal(std::vector<MoveFull>& moves, Side side, int from = -1, int dest = -1,
                      int promotion = EMPTY);
        bool isIncheck(Side beingAttackedSide) const;

        void gen(std::vector<MoveFull>& moveList, Side attackerSide) const;
        std::vector<MoveFull> gen(Side attackerSide) const;

        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide) const;
        virtual std::vector<MoveFull> _gen(Side attackerSide) const;
        virtual void _gen(std::vector<MoveFull>& moveList, Side attackerSide, int pieceType) const = 0;
        virtual void _genLegalOnly(std::vector<MoveFull>& moveList, Side attackerSide);
        virtual void _genLegal(std::vector<MoveFull>& moves, Side side, int from = -1, int dest = -1,
                               int promotion = EMPTY);

        virtual bool _isIncheck(Side beingAttackedSide) const = 0;

        bool checkMake(int from, int dest, int promotion);
        virtual bool _checkMake(int from, int dest, int promotion) = 0;
        bool checkMake(const Hist& hist, bool createSanString);
        bool _checkMake(const Hist& hist, bool createSanString);

        /// OCGDB addition: make a move of the given variation, taking the board to that
        /// variation's position first
        bool checkMake(const Move& move, const VariationRec& vr, bool createSanString);
        bool _checkMake(const Move& move, const VariationRec& vr, bool createSanString);

        void _insertHist(const Hist& hist, int atIdx);

        int _removeHistList(int atPly, int n);

        virtual bool _quickCheckMake(int from, int dest, int promotion, bool createSanString) = 0;
        virtual bool _quickCheck_rook(int from, int dest, bool checkMiddle) const;

        int parseMoveErrorCnt = 0;

        /// OCGDB: the move-text parsers read variations (ParseVariation) and call back at every
        /// position (PositionCallback), as in Banksia GUI's core; Banksia's skip variations.
        virtual bool fromMoveList(const PgnRecord& record, Notation, ParseVariation, const MoveFormat&,
                                  int flag = ParseMoveListFlag_quick_check | ParseMoveListFlag_create_san,
                                  PositionCallback = nullptr);
        virtual bool fromMoveList(const PgnRecord& record, Notation, ParseVariation,
                                  int flag = ParseMoveListFlag_quick_check | ParseMoveListFlag_create_san,
                                  PositionCallback = nullptr);
        virtual bool fromMoveList(const PgnRecord& record, Notation notation,
                                  int flag = ParseMoveListFlag_quick_check | ParseMoveListFlag_create_san)
        {
            return fromMoveList(record, notation, ParseVariation::discard, flag);
        }

        /// OCGDB addition: the moves of a database's binary move field (Moves1 with
        /// ParseMoveListFlag_move_size_1_byte, otherwise Moves2), variations included unless
        /// ParseMoveListFlag_ignore_high_levels
        virtual bool fromMoveList(const PgnRecord&, const std::vector<int8_t>& moveVec, int flag,
                                  PositionCallback = nullptr);

        /// OCGDB addition: stop fromMoveList() after this many plies of the main line; 0 parses
        /// the whole game
        int maxParsePly = 0;

        std::vector<HistBasic> _parsePv(const std::string& pvString, bool isCoordinateOnly,
                                        const MoveFormat&);


        virtual Result probeEgtb(int maxPieces, bool& tberror) const = 0;
        virtual std::vector<MoveFull> probeEgtbAll() const;

        /// OCGDB: the move lists are written with their variations, in brackets
        virtual std::string toMoveListString(Notation, int itemPerLine = 10000000, int fullMoveCnt = -1,
                                             bool withComputingInfo = false, bool pawnUnit = false,
                                             int precision = 1) const;
        virtual std::string toMoveListString(Notation, const MoveFormat&, int itemPerLine = 10000000,
                                             int fullMoveCnt = -1, bool withComputingInfo = false,
                                             bool pawnUnit = false, int precision = 1) const;
        /// A move list from histList, each move written by moveString(move, notation, mf)
        /// for every notation but SAN (which is stored in the Hist). The member forms pass
        /// this board's own toString_*(); a caller with no board passes its variant's.
        using MoveStringFunc = std::function<std::string(const MoveFull&, Notation, const MoveFormat&)>;
        static std::string toMoveListString(const std::vector<Hist>& histList, Notation, const MoveFormat&,
                                            int itemPerLine, int fullMoveCnt, bool withComputingInfo,
                                            bool pawnUnit, int precision, const MoveStringFunc& moveString);

        virtual std::string toPgn(const PgnRecord*) const;


        virtual char pieceType2Char(int pieceType) const = 0;
        virtual int coordinateStringToPos(const std::string& str, const MoveFormat&) const = 0;
        virtual std::string posToCoordinateString(int pos, const MoveFormat&) const = 0;

        void make(const MoveFull& move);
        void takeBack();

        virtual void _make(const MoveFull& move);
        virtual void _takeBack();

        void make(const MoveFull& move, Hist& hist);
        void takeBack(const Hist& hist);

        virtual void _make(const MoveFull& move, Hist& hist) = 0;
        virtual void _takeBack(const Hist& hist) = 0;
        void flipSide();

        virtual int findKing(Side side) const;


        bool equalMoveLists(const BoardCore*, bool embeded) const;

        /// OCGDB additions: the encodings of the binary move fields of a database:
        /// Moves2, two bytes a move; Moves1, one byte for most moves
        virtual uint16_t encode2Bytes(const Hist&) const = 0;
        virtual Move decode2Bytes(uint16_t d) const = 0;
        /// OCGDB addition: whether the 2-byte code needs the board of its move to be decoded
        virtual bool isBoardDependent2Bytes(uint16_t) const { return false; }
        virtual std::pair<uint16_t, int> encode1Byte(MoveFull move) = 0;
        virtual std::pair<Move, int> decode1Byte(const int8_t* d) = 0;

        /// OCGDB additions: keys for finding duplicate games
        virtual uint64_t getHashKeyForCheckingDuplicates(int = -1) const = 0;
        virtual DupRec createCheckingDuplicateRec(int fromPly = 0) const;

        /// OCGDB addition: the position as bitboards, for PQL (the caller deletes it)
        virtual BitboardBase* posToBitboards() const = 0;

        virtual Eco getEco(u64 key) const = 0;
        virtual bool isEcoName(const std::string name) const = 0;

        virtual std::string getEcoBriefString(u64 key) const;


        virtual Eco getLastEco() const;
        virtual Eco commentEco();


        virtual int see(const Move&) const = 0;

        // low 2 bytes is for black, hi for white
        virtual int materials() const = 0;


    protected:
        virtual bool createSanStringForLastMove(const MoveFormat&) = 0;
        void _setFen_basic_2ndpart(const std::vector<std::string>& vec);

        void setupPieceIndexes();

        virtual u64 xorHashKey(int pos) const = 0;
        virtual u64 xorSideHashKey() const = 0;


        /// OCGDB additions: going between the variations of a game's history
        void _removeAllVariations();
        void _adjustBoardToMoveLevel(const VariationRec&);
        void _adjustBoardToMoveBackLevel(const VariationRec& vr);

        /// A move of a VARIATION that cannot be read or made (an annotator's typo) must not make
        /// the parsers give up on the whole game, main line included. _fromMoveList() notes where
        /// (brokenVariationIdx) and fromMoveList() removes that variation, nested ones included,
        /// and parses the game again from its start.
        int brokenVariationIdx = -1;

        /// removes the variation that holds item `idx` (from its first move to its last, with the
        /// variations nested in it); returns the [from, to) range it had
        template<class V> static std::pair<size_t, size_t> eraseVariation(std::vector<V>& vec, int idx)
        {
            const auto vr = vec.at(idx).variationRec;
            auto ours = [&vr](const VariationRec& x) { return x.level == vr.level && x.groupIdx == vr.groupIdx; };
            size_t from = idx;
            while (from > 0 && !(ours(vec.at(from).variationRec) && vec.at(from).variationRec.startNewLevel)) {
                --from;
            }
            size_t to = idx + 1;
            while (to < vec.size()
                   && (vec.at(to).variationRec.level > vr.level
                       || (ours(vec.at(to).variationRec) && !vec.at(to).variationRec.startNewLevel))) {
                ++to;
            }
            vec.erase(vec.begin() + from, vec.begin() + to);
            return { from, to };
        }

        virtual bool _fromMoveList(const PgnRecord& record, Notation, MoveFormat, int flag,
                                   const PositionCallback& shouldStop, int startIdx,
                                   const std::vector<MoveStringLevel>& moveStringVec,
                                   const std::map<size_t, std::string>& commentMap);

        bool _fromMoveList(const PgnRecord& record, int flag, const PositionCallback& shouldStop, int startIdx,
                           const std::vector<MoveWithLevel>& moveWithLevelVec);
        bool _fromMoveList1Byte(const PgnRecord& record, const std::vector<int8_t>& moveVec, int flag,
                                const PositionCallback& shouldStop);

        /// Complete a move decoded from a database before it is made (jieqi: a reveal).
        virtual void completeDecodedMove(Move&) const { }

        /// Variant hooks of the PGN move-text parser (fromMoveList, _fromMoveList).
        /// A character, besides letters, digits and SAN's "=+", that belongs to a move.
        virtual bool isMoveTextChar(char /*ch*/) const { return false; }
        /// Settle what the move list itself says about its format (xiangqi: ranks from 0 or 1).
        virtual void detectMoveFormat(const std::vector<MoveStringLevel>& /*moveStringVec*/, MoveFormat&) { }
        /// A parsed move the variant cannot accept even though from/dest are on the board.
        virtual bool isParsedMoveValid(const Move&) const { return true; }
    };


} // namespace banksia

#endif /* board_hpp */
