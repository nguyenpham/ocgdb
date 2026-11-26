/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_STRUTIL_H
#define BANKSIA_BASE_STRUTIL_H

#include <string>
#include <vector>
#include <ctime>

namespace banksia {

    /// Case conversion, in place. ASCII only, which is all the callers need:
    /// engine names, option keys, PGN tags and FEN fields.
    void toLower(std::string& str);
    void toLower(char* str);
    void toUpper(std::string& str);

    /// Whitespace trimming, in place, returning the same string for chaining.
    /// The trimmed set is " \t\n\r\f\v".
    std::string& trim(std::string& s);
    std::string& rtrim(std::string& s);

    /// In-place trim of a C string. Returns a pointer into the original buffer,
    /// which is NOT the address passed in when there was leading whitespace --
    /// so the result must be used, and the original pointer kept for free().
    char* trim(char* s);

    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);

    /// Every occurrence, non-overlapping, left to right.
    std::string replaceString(std::string subject, const std::string& search, const std::string& replace);

    /// Split on a single character. Empty fields are dropped and each field is
    /// trimmed -- convenient for parsing engine output, wrong for anything that
    /// must round-trip. splitString(const std::string&) below keeps them.
    std::vector<std::string> splitString(const std::string& s, char delim);

    /// Split on a multi-character delimiter. Keeps empty fields and does not
    /// trim, so the result rejoins to the original.
    std::vector<std::string> splitString(const std::string& s, const std::string& del);

    /// Rewrites xiangqi piece letters from the standard set to the Betza/wiki
    /// set: kNight -> Horse, Bishop -> Elephant. See MoveFormat in comm.h.
    std::string convert2BHString(std::string str);

    /// Time as text
    ///
    /// localtime_xp() is the portable, thread-safe std::localtime: the C
    /// library's own returns a pointer to a shared static buffer, so two
    /// threads formatting a time at once corrupt each other's result.
    std::tm localtime_xp(std::time_t timer);

    /// "2026.09.05 14:30:00"
    std::string tmTime2String(std::time_t t);

    /// A duration as "3d 4:05:06", omitting the leading units that are zero.
    std::string formatPeriod(int seconds);

    /// A duration as a clock: "1:02:03", "2:03", "-0:05". `spString` separates
    /// the fields.
    std::string secondToClockString(int second, const std::string& spString = ":");

} // namespace banksia

#endif /* BANKSIA_BASE_STRUTIL_H */
