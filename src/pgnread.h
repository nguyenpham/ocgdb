/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_PGNREAD_H
#define OCGDB_PGNREAD_H

#include "core.h"

namespace ocgdb {


class PGNRead : public virtual Core
{
public:
    virtual void processPGNGame(const std::unordered_map<char*, char*>&, const char *);
    virtual void processPGNGameWithAThread(ThreadRecord*, const std::unordered_map<char*, char*>&, const char *);
    virtual void processPGNGameByAThread(const std::unordered_map<char*, char*>&, const char *);

protected:
    virtual uint64_t processPgnFiles(const std::vector<std::string>& pgnPaths);
    virtual uint64_t processPgnFile(const std::string& path);
    virtual void processDataBlock(char* buffer, long sz, bool);

private:
    void processHalfBegin(char* buffer, long len);
    void processHalfEnd(char* buffer, long len);

private:
    const size_t blockSz = 8 * 1024 * 1024;
    const int halfBlockSz = 16 * 1024;
    char* halfBuf = nullptr;
    long halfBufSz = 0;
};

} // namespace ocdb

#endif /* OCGDB_PGNREAD_H */
