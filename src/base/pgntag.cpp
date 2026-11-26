/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include "pgntag.h"


extern const std::vector<PGNTagRecord> pgnTagRecordVec;
const std::vector<PGNTagRecord> pgnTagRecordVec {
    // 7 basic tags
    { PGNTag::event, "Event", TagFlag_basic | TagFlag_db }, // basic 0

    { PGNTag::event_date, "EventDate", 0 },
    { PGNTag::event_sponsor, "EventSponsor", 0 },

    { PGNTag::site, "Site", TagFlag_basic | TagFlag_db }, // basic 1

    { PGNTag::date, "Date", TagFlag_basic | TagFlag_update | TagFlag_db }, // basic 2
    { PGNTag::utcdate, "UTCDate", TagFlag_db },
    { PGNTag::time, "Time", TagFlag_update | TagFlag_db },
    { PGNTag::utctime, "UTCTime", TagFlag_db },

    { PGNTag::round, "Round", TagFlag_basic | TagFlag_db }, // basic 3


    { PGNTag::section, "Section", 0 },
    { PGNTag::stage, "Stage", 0 },
    { PGNTag::board, "Board", 0 },

    { PGNTag::white_title, "WhiteTitle", 0 },
    { PGNTag::white, "White", TagFlag_basic | TagFlag_update | TagFlag_db }, // basic 4
    { PGNTag::white_elo, "WhiteElo", TagFlag_db },

    { PGNTag::black_title, "BlackTitle", 0 },
    { PGNTag::black, "Black", TagFlag_basic | TagFlag_update | TagFlag_db }, // basic 5
    { PGNTag::black_elo, "BlackElo", TagFlag_db },

    { PGNTag::result, "Result", TagFlag_basic | TagFlag_update | TagFlag_db }, // basic 6
    { PGNTag::termination, "Termination", TagFlag_update | TagFlag_db },

    { PGNTag::eco, "ECO", TagFlag_update | TagFlag_db },
    { PGNTag::opening, "Opening", TagFlag_update | TagFlag_db },
    { PGNTag::variation, "Variation", TagFlag_update | TagFlag_db },

    { PGNTag::timecontrol, "TimeControl", TagFlag_update | TagFlag_db },

    { PGNTag::annotator, "Annotator", 0 },
    { PGNTag::plycount, "PlyCount", TagFlag_update | TagFlag_db },
    { PGNTag::variant, "Variant", TagFlag_update | TagFlag_db },

};
