/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "bitboardbase.h"

#include <bit>

using namespace banksia;

int popCount(uint64_t x)
{
    return std::popcount(x);
}

BitboardBase::BitboardBase() { }


BitboardBase::~BitboardBase() { }
