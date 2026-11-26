/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "pgnread.h"

using namespace ocgdb;

PGNRead::~PGNRead()
{
    free(halfBuf_Pgn);
    halfBuf_Pgn = nullptr;
}

bool PGNRead::reachedGameLimit() const
{
    return paraRecord.gameNumberLimit > 0 && pushedGameCnt >= paraRecord.gameNumberLimit;
}

// the game between two blocks, first half (WXF)
void PGNRead::processHalfBegin(char* buffer, long len)
{
    halfBufSz_Pgn = 0;
    if (!buffer || len <= 0 || len >= halfBlockSz_Pgn) {
        return;
    }

    if (!halfBuf_Pgn) {
        halfBuf_Pgn = (char*)malloc(halfBlockSz_Pgn + 16);
    }

    memcpy(halfBuf_Pgn, buffer, len);
    halfBufSz_Pgn = len;
    halfBuf_Pgn[len] = 0;
    halfBuf_Pgn[len + 1] = 0;
}

/// Parse the complete game held in pending_Pgn (it may contain NULs written by the tag parser of
/// the block it came from; the parser treats a NUL like the character it replaced). Waits for the
/// pool: the tasks point into the copy.
void PGNRead::processPending()
{
    pendingState_Pgn = 0;
    if (!pending_Pgn.empty()) {
        std::vector<char> tmp(pending_Pgn.begin(), pending_Pgn.end());
        tmp.push_back(0);
        tmp.push_back(0);
        processDataBlock(tmp.data(), (long)pending_Pgn.size(), false);
        pool->wait_for_tasks();
    }
    pending_Pgn.clear();
    pendingHasMoves_Pgn = false;
}

/// Parse a block of PGN text. With connectBlock, the block is part of a file read in fixed-size
/// pieces: text before the block's first new game completes the game the previous block ended in
/// (pending_Pgn), and the game this block ends in becomes the new pending_Pgn.
void PGNRead::processDataBlock(char* buffer, long sz, bool connectBlock)
{
    assert(buffer && sz > 0);

    buffer[sz] = 0;
    buffer[sz + 1] = 0;

    std::unordered_map<const char*, char*> tagMap;

    char *tagName = nullptr, *tagContent = nullptr, *gameStart = nullptr, *moves = nullptr;

    /// in the tail of the pending game: before this block's first new game
    auto leading = connectBlock && !pending_Pgn.empty();
    auto leadingMoves = false;

    enum class ParsingState { none, tagName, tag_after, tag_content, tag_content_after, comment };

    auto st = leading ? static_cast<ParsingState>(pendingState_Pgn) : ParsingState::none;

    for (char *p = buffer, *end = buffer + sz; p < end && working; p++) {
        char ch = *p;

        switch (st) {
            case ParsingState::none: {
                if (ch == '[') {
                    p++;
                    /// Check carefully to avoid [ in middle of a line or without tag name
                    if (p >= end || *p < 'A' || *p > 'Z' || (p > buffer + 1 && *(p - 2) >= ' ')) {
                        continue;
                    }
                    auto tagStart = p - 1;

                    if (leading) {
                        if (pendingHasMoves_Pgn || leadingMoves) {
                            // the pending game ends here: complete it and parse it
                            if (pending_Pgn.size() + (tagStart - buffer) > maxPendingSz_Pgn) {
                                std::cerr << "Warning: a game longer than " << maxPendingSz_Pgn
                                          << " bytes across a block boundary was dropped" << std::endl;
                                pending_Pgn.clear();
                            } else {
                                pending_Pgn.append(buffer, tagStart - buffer);
                            }
                            leading = false;
                            processPending();
                            gameStart = tagStart;
                        }
                        // else: still in the pending game's header
                    } else if (moves) {
                        // the tag begins the next game: the current one is complete
                        if (tagStart > buffer) {
                            *(tagStart - 1) = 0;
                        }
                        processPGNGame(tagMap, moves);
                        tagMap.clear();
                        moves = nullptr;
                        gameStart = tagStart;
                    } else if (!gameStart) {
                        gameStart = tagStart;
                    }

                    tagName = p;
                    st = ParsingState::tagName;
                } else if (ch > ' ') {
                    // Comments by semicolon or escape mechanism
                    if (ch == ';' || (ch == '%' && (p == buffer || *(p - 1) == '\n' || *(p - 1) == '\r'))) {
                        st = ParsingState::comment;
                    } else if (leading) {
                        leadingMoves = true;
                    } else if (!moves && gameStart) {
                        moves = p;
                    }
                }
                break;
            }
            case ParsingState::tagName: // name tag
            {
                assert(tagName || leading);
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
                } else if (ch == '\n') { // a tag with no value: skip it
                    st = ParsingState::none;
                }
                break;
            }
            case ParsingState::tag_content: {
                /// an escaped quote or backslash (PGN standard 8.1.1) does not end the value; the
                /// value keeps its escapes here (the text may be parsed again: a game cut by a block
                /// boundary), the user of the tag removes them (Builder)
                if (ch == '\\' && p + 1 < end && (p[1] == '"' || p[1] == '\\')) {
                    p++;
                    break;
                }
                if (ch == '"' || ch == 0) { // == 0: a quote the parser of an earlier block replaced
                    *p = 0;
                    if (!leading && gameStart) {
                        tagMap[tagName] = tagContent;
                    }
                    tagName = tagContent = nullptr;
                    st = ParsingState::tag_content_after;
                }
                break;
            }

            /// comments by semicolon or escape mechanism % in the header,
            /// just ignore still the end of that line
            case ParsingState::comment: {
                if (ch == '\n' || ch == '\r' || ch == 0) {
                    st = ParsingState::none;
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
        if (leading) {
            // no new game began in this whole block: all of it belongs to the pending game
            if (pending_Pgn.size() + sz > maxPendingSz_Pgn) {
                std::cerr << "Warning: a game longer than " << maxPendingSz_Pgn
                          << " bytes across a block boundary was dropped" << std::endl;
                pending_Pgn.clear();
                pendingHasMoves_Pgn = false;
            } else {
                pending_Pgn.append(buffer, sz);
                pendingHasMoves_Pgn = pendingHasMoves_Pgn || leadingMoves;
                pendingState_Pgn = static_cast<int>(st);
            }
        } else if (gameStart) {
            // the last game of the block is completed by the next one (or by the end of file)
            pending_Pgn.assign(gameStart, buffer + sz - gameStart);
            pendingHasMoves_Pgn = moves != nullptr;
            pendingState_Pgn = static_cast<int>(st);
        }
    } else if (moves) {
        processPGNGame(tagMap, moves);
    }
}

uint64_t PGNRead::processPgnFiles(const std::vector<std::string>& pathList)
{
    uint64_t cnt = 0;
    for (auto&& path : pathList) {
        if (!working || reachedGameLimit()) break;
        cnt += processPgnFile(path);
    }
    return cnt;
}

uint64_t PGNRead::processPgnFile(const std::string& path)
{
    std::cout << "Reading: " << path << std::endl;
    assert(pool);

    auto f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Error: can't open file: " << path << std::endl;
        errCnt++;
        return 0;
    }

    std::fseek(f, 0, SEEK_END);
    fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (fileSize <= 0) {
        std::cerr << "Error: file empty: " << path << std::endl;
        std::fclose(f);
        return 0;
    }

    auto blockSz = blockSz_Pgn;
    /// OCGDB_PGN_BLOCK=<bytes> overrides the block size: a test that puts block boundaries
    /// everywhere (as Banksia GUI's BANKSIA_PGN_BLOCK and OOBS's OOBS_PGN_BLOCK)
    if (auto e = std::getenv("OCGDB_PGN_BLOCK")) {
        auto v = std::strtoull(e, nullptr, 10);
        if (v >= 64) blockSz = static_cast<size_t>(v);
    }
    auto buffer = (char*)malloc(blockSz + 16);

    auto startCnt = processedCnt.load();
    pgnGameIndex = 0;
    blockCnt = processedPgnSz = 0;
    pending_Pgn.clear();
    pendingHasMoves_Pgn = false;
    pendingState_Pgn = 0;

    /// A PGN block ends at its last newline and the partial line starts the next block, so a
    /// tag line is never cut (the parser skips a '[' with nothing after it).
    std::string carry;
    const auto cutAtNewline = cutBlocksAtNewline();
    for (int64_t sz = 0; (sz < fileSize || !carry.empty()) && working && !reachedGameLimit();) {
        auto c = carry.size();
        memcpy(buffer, carry.data(), c);
        auto k = std::min<int64_t>(static_cast<int64_t>(blockSz - c), fileSize - sz);
        int64_t rsz = k > 0 ? static_cast<int64_t>(std::fread(buffer + c, 1, k, f)) : 0;
        sz += rsz;
        auto len = static_cast<int64_t>(c) + rsz;
        if (len <= 0 || (rsz == 0 && k > 0)) {
            break;
        }

        auto cut = len;
        if (cutAtNewline && sz < fileSize) {
            for (auto i = len; i > 0; i--) {
                if (buffer[i - 1] == '\n') {
                    cut = i;
                    break;
                }
            }
        }
        carry.assign(buffer + cut, static_cast<size_t>(len - cut));

        buffer[cut] = 0;
        buffer[cut + 1] = 0;
        buffer[cut + 2] = 0;
        buffer[cut + 3] = 0;

        blockCnt++;
        processedPgnSz += cut;
        processDataBlock(buffer, cut, true);

        /// the games point into the buffer
        pool->wait_for_tasks();
        printProgress();
    }

    // the last game of the file (PGN)
    if (working && !reachedGameLimit()) {
        processPending();
    }
    pending_Pgn.clear();
    pool->wait_for_tasks();

    // WXF's reader carries its cut game in halfBuf_Pgn
    if (halfBuf_Pgn) {
        if (halfBufSz_Pgn > 0 && working) {
            processDataBlock(halfBuf_Pgn, halfBufSz_Pgn, false);
            pool->wait_for_tasks();
        }

        free(halfBuf_Pgn);
        halfBuf_Pgn = nullptr;
        halfBufSz_Pgn = 0;
    }

    std::fclose(f);
    free(buffer);
    fileSize = 0;

    return processedCnt - startCnt;
}


thread_local int64_t PGNRead::currentGameIndex = 0;

static void doProcessPGNGame(PGNRead* instance, const std::unordered_map<const char*, char*> tagMap, const char* moves,
                             int64_t gameIndex)
{
    assert(instance);
    instance->processPGNGameByAThread(tagMap, moves, gameIndex);
}

/// on the reading thread, in the order of the file
void PGNRead::processPGNGame(const std::unordered_map<const char*, char*>& tagMap, const char* moves)
{
    if (pool && working && !reachedGameLimit()) {
        ++pushedGameCnt;
        pool->push_task(doProcessPGNGame, this, tagMap, moves, ++pgnGameIndex);
    }
}

void PGNRead::processPGNGameByAThread(const std::unordered_map<const char*, char*>& tagMap, const char* moves,
                                      int64_t gameIndex)
{
    currentGameIndex = gameIndex;
    if (working) {
        ++processedCnt;
        auto t = getThreadRecord();
        assert(t);
        processPGNGameWithAThread(t, tagMap, moves);
    }
}

void PGNRead::processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<const char*, char*>&, const char*)
{
    assert(false);
}
