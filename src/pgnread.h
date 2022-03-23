/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef PGNREAD_H
#define PGNREAD_H

#include "dbread.h"
#include "parser.h"

namespace ocgdb {


class PGNRead : public virtual DbCore
{
public:
    PGNRead();
    ~PGNRead();

    virtual void processPGNGame(const std::unordered_map<char*, char*>&, const char *);
    virtual void processPGNGameByAThread(const std::unordered_map<char*, char*>&, const char *);

protected:
    uint64_t processPgnFiles(const std::vector<std::string>& pgnPaths);
    uint64_t processPgnFile(const std::string& path);

private:
    void processDataBlock(char* buffer, long sz, bool);
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);

private:
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;
};

} // namespace ocdb

#endif /* PGNREAD_H */
