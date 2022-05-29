/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include "pgnread.h"

using namespace ocgdb;

// the game between two blocks, first half
void PGNRead::processHalfBegin(char* buffer, long len)
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
void PGNRead::processHalfEnd(char* buffer, long len)
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

void PGNRead::processDataBlock(char* buffer, long sz, bool connectBlock)
{
    assert(buffer && sz > 0);
    
    std::unordered_map<char*, char*> tagMap;

    auto evtCnt = 0;
    auto hasEvent = false;
    char *tagName = nullptr, *tagContent = nullptr, *event = nullptr, *moves = nullptr;

    enum class ParsingState {
        none, tagName, tag_after, tag_content, tag_content_after, comment
    };
    
    auto st = ParsingState::none;
    
    for(char *p = buffer, *end = buffer + sz; p < end; p++) {
        char ch = *p;
        
        switch (st) {
            case ParsingState::none:
            {
                if (ch == '[') {
                    p++;
                    // Check carefully to avoid [ in middle of a line or without tag name
                    if (*p < 'A' || *p > 'Z' || (p > buffer + 1 && *(p - 2) >= ' ')) { // if (!isalpha(*p)) {
                        continue;
                    }
                    
                    // has a tag
                    if (moves) {
                        if (hasEvent && p - buffer > 2) {
                            *(p - 2) = 0;
                            
                            processPGNGame(tagMap, moves);
                        }

                        tagMap.clear();
                        hasEvent = false;
                        moves = nullptr;
                    }

                    tagName = p;
                    st = ParsingState::tagName;
                } else if (ch > ' ') {
                    // Comments by semicolon or escape mechanism
                    if (ch == ';'
                        || (ch == '%' && (p == buffer || *p == '\n' || *p == '\r'))) {
                        st = ParsingState::comment;
                    } else
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

            // comments by semicolon or escape mechanism % in the header,
            // just ignore still the end of that line
            case ParsingState::comment:
            {
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
        processHalfBegin(event, (long)sz - (event - buffer));
    } else if (moves) {
        processPGNGame(tagMap, moves);
    }
}

uint64_t PGNRead::processPgnFiles(const std::vector<std::string>& pgnPaths)
{
    uint64_t cnt = 0;
    for(auto && path : pgnPaths) {
        cnt += processPgnFile(path);
    }
    return cnt;
}

uint64_t PGNRead::processPgnFile(const std::string& path)
{
    std::cout << "Processing PGN file: '" << path << "'" << std::endl;

//    auto transactionCnt = 0;

    {
        char *buffer = (char*)malloc(blockSz + 16);
        auto size = bslib::Funcs::getFileSize(path);

        std::ifstream file(path, std::ios::binary);

        if (!file || size == 0) {
            std::cerr << "Error: Can't open file: '" << path << "'" << std::endl;
            return 0;
        }
        
        blockCnt = processedPgnSz = 0;
        for (size_t sz = 0, idx = 0; sz < size && gameCnt < paraRecord.gameNumberLimit; idx++) {
            auto k = std::min(blockSz, size - sz);
            if (k == 0) {
                break;
            }
            
            buffer[k] = 0;
            if (file.read(buffer, k)) {

                blockCnt++;
                processedPgnSz += k;
                processDataBlock(buffer, k, true);

                pool->wait_for_tasks();
                
                if (idx && (idx & 0xf) == 0) {
                    printStats();
                }
            }
            sz += k;
        }

        file.close();
        free(buffer);

        if (halfBuf) {
            if (halfBufSz > 0) {
                processDataBlock(halfBuf, halfBufSz, false);
            }
            
            free(halfBuf);
            halfBuf = 0;
        }
    }
    
//    if (mDb && transactionCnt > 0) {
//        mDb->exec("COMMIT");
//    }

    printStats();

    return gameCnt;
}


void doProcessPGNGame(PGNRead* instance, const std::unordered_map<char*, char*>& tagMap, const char* moves)
{
    assert(instance);
    instance->processPGNGameByAThread(tagMap, moves);
}

void PGNRead::processPGNGame(const std::unordered_map<char*, char*>& tagMap, const char* moves)
{
    pool->submit(doProcessPGNGame, this, tagMap, moves);
}

void PGNRead::processPGNGameByAThread(const std::unordered_map<char*, char*>& tagMap, const char* moves)
{
    auto t = getThreadRecord(); assert(t);
    processPGNGameWithAThread(t, tagMap, moves);
}


void PGNRead::processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *)
{
    assert(false);
}
