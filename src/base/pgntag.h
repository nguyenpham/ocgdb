/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_PGNTAG_H
#define BANKSIA_BASE_PGNTAG_H

#include <string>
#include <vector>

enum class PGNTag {
    event,
    event_date,
    event_sponsor,

    site,

    date,
    utcdate,
    time,
    utctime,

    round,

    section,
    stage,
    board,

    white_title,
    white,
    white_elo,

    black_title,
    black,
    black_elo,

    result,
    termination,

    eco,
    opening,
    variation,

    timecontrol,

    annotator,
    plycount,
    variant,

    max
};

#define TagFlag_basic 1 // of 7 basic item
#define TagFlag_noedit (1 << 1)
#define TagFlag_update (1 << 2) // chess GUI will update by its values
#define TagFlag_db (1 << 3) // OCGDB-db could auto update value

class PGNTagRecord
{
public:
    PGNTag tag;
    std::string name;
    int prop;
};

class PGNTagValue
{
public:
    PGNTagValue() { }
    PGNTagValue(PGNTag tag, const std::string& string = "") : tag(tag), string(string) { }

    PGNTag tag;
    std::string string;
};


extern const std::vector<PGNTagRecord> pgnTagRecordVec;

#endif // BANKSIA_BASE_PGNTAG_H
