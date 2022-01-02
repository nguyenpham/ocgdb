/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <set>
#include <fstream>

#include "builder.h"
#include "board/chess.h"

using namespace ocgdb;

Builder* builder = nullptr;

static int _popCount(uint64_t x) {
   int count = 0;
   while (x) {
       count++;
       x &= x - 1; // reset LS1B
   }
   return count;
}

static const int lsb_64_table[64] =
{
   63, 30,  3, 32, 59, 14, 11, 33,
   60, 24, 50,  9, 55, 19, 21, 34,
   61, 29,  2, 53, 51, 23, 41, 18,
   56, 28,  1, 43, 46, 27,  0, 35,
   62, 31, 58,  4,  5, 49, 54,  6,
   15, 52, 12, 40,  7, 42, 45, 16,
   25, 57, 48, 13, 10, 39,  8, 44,
   20, 47, 38, 22, 17, 37, 36, 26
};

static int _bitScanForward(uint64_t bb) {
   unsigned int folded;
   assert (bb != 0);
   bb ^= bb - 1;
   folded = (int) bb ^ (bb >> 32);
   return lsb_64_table[folded * 0x78291ACF >> 26];
}

static int _bitScanForwardWithReset(uint64_t &bb) {
    int idx = _bitScanForward(bb);
    bb &= bb - 1; // reset bit outside
    return idx;
}


void ThreadRecord::init()
{
    if (board) return;
    errCnt = 0;
    board = Builder::createBoard(bslib::ChessVariant::standard);
}

ThreadRecord::~ThreadRecord()
{
    delete board;
}

Builder::Builder()
{
    builder = this;
}

Builder::~Builder()
{
    threadMap.clear();
}

std::chrono::steady_clock::time_point getNow()
{
    return std::chrono::steady_clock::now();
}

bslib::BoardCore* Builder::createBoard(bslib::ChessVariant variant)
{
    return variant == bslib::ChessVariant::standard ? new bslib::ChessBoard : nullptr;
}


// the game between two blocks, first half
void Builder::processHalfBegin(char* buffer, long len)
{
    halfBufSz = 0;
    if (!buffer || len <= 0 || len >= halfBlockSz) {
        return;
    }
    
    if (!halfBuf) {
        halfBuf = (char*)malloc(halfBlockSz + 16);
    }
    
    memcpy(halfBuf, buffer, len);
    halfBufSz = len;
}

// the game between two blocks, second half
void Builder::processHalfEnd(char* buffer, long len)
{
    if (!buffer || !halfBuf) {
        return;
    }
    
    if (len > 0 && len + halfBufSz > halfBlockSz) {
        halfBufSz = 0;
        return;
    }
    
    if (len > 0) {
        memcpy(halfBuf + halfBufSz, buffer, len);
        halfBufSz += len;
    }
    
    halfBuf[halfBufSz] = 0;
    
    processDataBlock(halfBuf, halfBufSz, false);
    halfBufSz = 0;
}

void Builder::processDataBlock(char* buffer, long sz, bool connectBlock)
{
    assert(buffer && sz > 0);
    
    std::unordered_map<char*, char*> tagMap;

    auto evtCnt = 0;
    auto hasEvent = false;
    char *tagName = nullptr, *tagContent = nullptr, *event = nullptr, *moves = nullptr;

    enum class ParsingState {
        none, tagName, tag_after, tag_content, tag_content_after
    };
    
    auto st = ParsingState::none;
    
    for(char *p = buffer, *end = buffer + sz; p < end; p++) {
        char ch = *p;
        
        switch (st) {
            case ParsingState::none:
            {
                if (ch == '[') {
                    p++;
                    if (!isalpha(*p)) {
                        continue;
                    }
                    
                    // has a tag
                    if (moves) {
                        if (hasEvent && p - buffer > 2) {
                            *(p - 2) = 0;

                            threadAddGame(tagMap, moves);
                        }

                        tagMap.clear();
                        hasEvent = false;
                        moves = nullptr;
                    }

                    tagName = p;
                    st = ParsingState::tagName;
                } else if (ch > ' ') {
                    if (!moves && hasEvent) {
                        moves = p;
                    }
                }
                break;
            }
            case ParsingState::tagName: // name tag
            {
                assert(tagName);
                if (!isalpha(ch)) {
                    if (ch <= ' ') {
                        *p = 0; // end of the tag name
                        st = ParsingState::tag_after;
                    } else { // something wrong
                        st = ParsingState::none;
                    }
                }
                break;
            }
            case ParsingState::tag_after: // between name and content of a tag
            {
                if (ch == '"') {
                    st = ParsingState::tag_content;
                    tagContent = p + 1;
                }
                break;
            }
            case ParsingState::tag_content:
            {
                if (ch == '"' || ch == 0) { // == 0 trick to process half begin+end
                    *p = 0;
                    
                    if (strcmp(tagName, "Event") == 0) {
                        event = tagName - 1;
                        if (evtCnt == 0 && connectBlock) {
                            long len =  (event - buffer) - 1;
                            processHalfEnd(buffer, len);
                        }
                        hasEvent = true;
                        evtCnt++;
                    }

                    if (hasEvent) {
                        tagMap[tagName] = tagContent;
                    }

                    tagName = tagContent = nullptr;
                    st = ParsingState::tag_content_after;
                }
                break;
            }
            default: // the rest of the tag
            {
                if (ch == '\n' || ch == 0) {
                    st = ParsingState::none;
                }
                break;
            }
        }
    }
    
    if (connectBlock) {
        processHalfBegin(event, (long)sz - (event - buffer));
    } else if (moves) {
        threadAddGame(tagMap, moves);
    }
}

uint64_t Builder::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;
    
    succCount = gameCnt = 0;

    {
        auto startTime = getNow();

        char *buffer = (char*)malloc(blockSz + 16);

        FILE *stream = fopen(path.c_str(), "r");
        assert(stream != NULL);
        auto size = bslib::Funcs::getFileSize(stream);
        
        auto blkIdx = 0;
        for (size_t sz = 0; sz < size; blkIdx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (fread(buffer, k, 1, stream)) {
                processDataBlock(buffer, k, true);
                pool->wait_for_tasks();

//                if (blkIdx && (blkIdx & 0xf) == 0) {
//                    printStats();
//                }
            }
            sz += k;
        }

        fclose(stream);
        free(buffer);

        if (halfBuf) {
            if (halfBufSz > 0) {
                processDataBlock(halfBuf, halfBufSz, false);
                pool->wait_for_tasks();
            }

            free(halfBuf);
            halfBuf = 0;
        }
        
        
        int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(getNow() - startTime).count() + 1;
        std::cout << "Elapsed: " << elapsed << " ms, "
                  << bslib::Funcs::secondToClockString(static_cast<int>(elapsed / 1000), ":")
                  << ", total size: " << size << "; Block sz: " << blockSz << ", block count: " << blkIdx
                  << ", time per block: " << elapsed / std::max<int64_t>(1, blkIdx)  << " ms"
                  << ", gameCnt: " << gameCnt
                  << ", succCount: " << succCount
                  << ", time per succ: " << elapsed / std::max<int64_t>(1, succCount)  << " ms"
                  << std::endl;

    }

    return gameCnt;
}

const char* tagNames[] = {
    "Event", "Site", "Date", "Round",
    "White", "WhiteElo", "Black", "BlackElo",
    "Result", "TimeControl", "ECO", "PlyCount", "FEN",
    nullptr, nullptr
};

enum {
    TagIdx_Event, TagIdx_Site, TagIdx_Date, TagIdx_Round,
    TagIdx_White, TagIdx_WhiteElo, TagIdx_Black, TagIdx_BlackElo,
    TagIdx_Result, TagIdx_Timer, TagIdx_ECO, TagIdx_PlyCount,
    TagIdx_FEN, TagIdx_Moves,
    TagIdx_GameID,
    TagIdx_Max
};

void doAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    assert(builder);
    builder->addGame(itemMap, moveText);
}

void Builder::threadAddGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    pool->submit(doAddGame, itemMap, moveText);
}

bool Builder::addGame(const std::unordered_map<char*, char*>& itemMap, const char* moveText)
{
    auto threadId = std::this_thread::get_id();
    auto t = &threadMap[threadId];
    t->init();
    assert(t->board);

    if (itemMap.size() < 3) {
        t->errCnt++;
        return false;
    }
    
    auto whiteElo = 0, blackElo = 0, plyCount = 0; // eventId = 1,
    char* whiteName = nullptr, *blackName = nullptr, *date = nullptr;
    std::string fenString;

    for(auto && it : itemMap) {
        for(auto i = 0; tagNames[i]; i++) {
            if (strcmp(it.first, tagNames[i]) != 0) {
                if (strcmp(it.first, "Variant") == 0) {
                    auto variant = bslib::Funcs::string2ChessVariant(it.second);
                    // at this moment, support only the standard variant
                    if (variant != bslib::ChessVariant::standard) {
                        t->errCnt++;
                        return false;
                    }
                }
                if (strcmp(it.first, "UTCDate") == 0 && !date) {
                    date = it.second;
                }
                continue;
            }

            auto s = it.second;
            while(*s <= ' ' && *s > 0) s++; // trim left
            assert(strlen(s) < 1024);
                            
            switch (i) {
                case TagIdx_Event:
                case TagIdx_Site:
                case TagIdx_Result:
                case TagIdx_Timer:
                case TagIdx_ECO:
                case TagIdx_Round:
                {
                    break;
                }
                case TagIdx_White:
                {
                    whiteName = s;
                    break;
                }
                case TagIdx_Black:
                {
                    blackName = s;
                    break;
                }

                case TagIdx_Date:
                {
                    date = s;
                    break;
                }

                case TagIdx_FEN:
                {
                    fenString = s;
                    break;
                }
                case TagIdx_WhiteElo:
                {
                    whiteElo = std::atoi(s);
                    break;
                }
                case TagIdx_BlackElo:
                {
                    blackElo = std::atoi(s);
                    break;
                }
                case TagIdx_PlyCount:
                {
                    plyCount = std::atoi(s);
                    break;
                }

                default:
                    assert(false);
                    break;
            }
            break;
        }
    }
    
    // trim left
    while(*moveText <= ' ') moveText++;

    int64_t gameID;
    {
        std::lock_guard<std::mutex> dolock(gameMutex);
        ++gameCnt;
        gameID = gameCnt;
    }


    // Parse moves
    {
        //assert(t->board);
        t->board->newGame(fenString);
        t->board->fromMoveList(gameID, moveText, bslib::Notation::san, false, bslib::BoardCore::CreateExtra::bitboard, checkToStop);
    }

    return true;
}


void Builder::bench(const std::string& pgnPath, int cpu)
{
    std::cout << "Test Querying Speed..." << std::endl;
    // init
    {
        startTime = getNow();

        if (cpu < 0) cpu = std::thread::hardware_concurrency();
        pool = new thread_pool(cpu);
        std::cout << "Thread count: " << pool->get_thread_count() << std::endl;
    }

    // Read, parse all moves (not asnwering for any query)
    {
        std::cout << "Read and parse all moves..." << std::endl;
        checkToStop = nullptr;
        processPgnFile(pgnPath);
        std::cout << "Completed reading and parsing all moves." << std::endl;
    }

    // 3 White Queens
    {
        std::cout << "Querying 3 White Queens..." << std::endl;
        checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board) -> bool {
            assert(board && bitboardVec.size() >= 8);
            auto White = bitboardVec[bslib::W];
            auto Queens = bitboardVec[static_cast<int>(bslib::PieceTypeStd::queen) + 1];

            if (_popCount(White & Queens) == 3) {
                succCount++;
                std::cout << succCount << ". gameId: " << gameId << ", fen: " << board->getFen() << std::endl;
                return true;
            }

            return false;
        };

        processPgnFile(pgnPath);
        std::cout << "Completed querying 3 White Queens." << std::endl;
    }

    // query positions with 2 Black Rooks in the middle squares
    {
        std::cout << "Querying 2 Black Rooks in the middle squares..." << std::endl;
        
        auto maskMids = bslib::ChessBoard::posToBitboard("d4") | bslib::ChessBoard::posToBitboard("d5")
                      | bslib::ChessBoard::posToBitboard("e4") | bslib::ChessBoard::posToBitboard("e5");

        checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board) -> bool {
            assert(board && bitboardVec.size() >= 8);
            auto midBlackRooks = bitboardVec[static_cast<int>(bslib::PieceTypeStd::rook) + 1] & bitboardVec[bslib::B] & maskMids;

            if (_popCount(midBlackRooks) == 2) {
                succCount++;
                
                if ((succCount & 0xff) == 0) {
                    std::cout << succCount << ". gameId: " << gameId << ", fen: " << board->getFen() << std::endl;
                }
                return true;
            }

            return false;
        };

        processPgnFile(pgnPath);
        std::cout << "Completed querying 2 Black Rooks in the middle squares." << std::endl;
    }
    
    
    // query games with White Pawns in d4, e5, f4, g4, Black King in b7
    {
        std::cout << "Querying 2 White Pawns in the middle squares & Black King..." << std::endl;
        
        auto maskPawns = bslib::ChessBoard::posToBitboard("d4") | bslib::ChessBoard::posToBitboard("e5")
                       | bslib::ChessBoard::posToBitboard("f4") | bslib::ChessBoard::posToBitboard("g4");
        
        auto bKing = bslib::ChessBoard::posToBitboard("b7");

        checkToStop = [=](int64_t gameId, const std::vector<uint64_t>& bitboardVec, const bslib::BoardCore* board) -> bool {
            assert(board && bitboardVec.size() >= 8);
            auto whitePawns = bitboardVec[static_cast<int>(bslib::PieceTypeStd::pawn) + 1] & bitboardVec[bslib::W] & maskPawns;
            auto blackKing = bitboardVec[static_cast<int>(bslib::PieceTypeStd::king) + 1] & bitboardVec[bslib::B];

            if (whitePawns == maskPawns && blackKing == bKing) {
                succCount++;
                std::cout << succCount << ". gameId: " << gameId << ", fen: " << board->getFen() << std::endl;
                return true;
            }

            return false;
        };

        processPgnFile(pgnPath);
        std::cout << "Completed querying 2 White Pawns in the middle squares & Black King." << std::endl;
    }

    std::cout << "All tests are DONE." << std::endl;
}
    
void Builder::updateBoard(bslib::BoardCore* board, const std::vector<uint64_t>& bbvec)
{
    // bitboards of black, white, king must not be zero
    assert(board && bbvec.size() >= 9 && bbvec[0] && bbvec[1] && bbvec[2]);
    
    for(auto i = 0; i < 64; ++i) {
        board->setEmpty(i);
    }

    auto bbblack = bbvec[0];
    for(int type = bslib::KING; type <= bslib::PAWNSTD; ++type) {
        auto bb = bbvec.at(type + 1);
        
        while(bb) {
            auto pos = _bitScanForwardWithReset(bb); assert(pos >= 0 && pos <  64);
            auto side = (bbblack & bslib::ChessBoard::_posToBitboard[pos]) ? bslib::Side::black : bslib::Side::white;
            bslib::Piece piece(type, side);
            board->setPiece(pos, piece);
        }
    }
    
    auto chessBoard = static_cast<bslib::ChessBoard*>(board);
    auto prop = bbvec.at(8);
    auto enpassant = static_cast<int8_t>(prop & 0xff);
    chessBoard->setEnpassant(enpassant);
    chessBoard->setCastleRights(0, (prop >> 8) & bslib::CastleRight_mask);
    chessBoard->setCastleRights(1, (prop >> 10) & bslib::CastleRight_mask);
}


