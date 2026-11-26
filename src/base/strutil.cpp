/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>

#include "strutil.h"

namespace banksia {

    static const char* trimChars = " \t\n\r\f\v";

    void toLower(std::string& str)
    {
        for (auto& ch : str) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    void toLower(char* str)
    {
        if (!str) return;
        for (size_t i = 0; str[i]; ++i) {
            str[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(str[i])));
        }
    }

    void toUpper(std::string& str)
    {
        for (auto& ch : str) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
    }

    /// Internal: trim() is the only caller.
    static std::string& ltrim(std::string& s)
    {
        s.erase(0, s.find_first_not_of(trimChars));
        return s;
    }

    std::string& rtrim(std::string& s)
    {
        s.erase(s.find_last_not_of(trimChars) + 1);
        return s;
    }

    std::string& trim(std::string& s)
    {
        return ltrim(rtrim(s));
    }

    char* trim(char* s)
    {
        if (s) {
            while (*s <= ' ' && *s > 0)
                s++;

            for (auto len = strlen(s); len > 0; --len) {
                auto ch = s[len - 1];
                if (ch <= ' ' && ch > 0)
                    s[len - 1] = 0;
                else
                    break;
            }
        }
        return s;
    }

    bool startsWith(const std::string& str, const std::string& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    bool endsWith(const std::string& str, const std::string& suffix)
    {
        return str.size() >= suffix.size()
               && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    std::string replaceString(std::string subject, const std::string& search, const std::string& replace)
    {
        if (search.empty()) {
            return subject;
        }
        size_t pos = 0;
        while ((pos = subject.find(search, pos)) != std::string::npos) {
            subject.replace(pos, search.length(), replace);
            pos += replace.length();
            if (pos >= subject.size()) break;
        }
        return subject;
    }

    std::vector<std::string> splitString(const std::string& s, char delim)
    {
        std::vector<std::string> elems;
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            auto trimmed = trim(item);
            if (!trimmed.empty()) elems.push_back(trimmed);
        }
        return elems;
    }

    std::vector<std::string> splitString(const std::string& s, const std::string& del)
    {
        std::vector<std::string> vec;
        if (del.empty()) {
            vec.push_back(s);
            return vec;
        }

        /// start/end were int, holding the result of find(), which returns
        /// size_t. The loop compared against -1 rather than npos and truncated
        /// any offset past 2 GB.
        std::string::size_type start = 0, end;
        while ((end = s.find(del, start)) != std::string::npos) {
            vec.push_back(s.substr(start, end - start));
            start = end + del.size();
        }
        vec.push_back(s.substr(start));
        return vec;
    }

    std::string convert2BHString(std::string str)
    {
        std::replace(str.begin(), str.end(), 'n', 'h');
        std::replace(str.begin(), str.end(), 'N', 'H');
        std::replace(str.begin(), str.end(), 'b', 'e');
        /// This was a second 'b' -> 'E', which could never match: the line
        /// above has already replaced every 'b'. Upper-case bishops were left
        /// as 'B' in what is supposed to be a fully converted string.
        std::replace(str.begin(), str.end(), 'B', 'E');
        return str;
    }

    // https://stackoverflow.com/questions/38034033/c-localtime-this-function-or-variable-may-be-unsafe
    std::tm localtime_xp(std::time_t timer)
    {
        std::tm bt { };
#if defined(_MSC_VER)
        localtime_s(&bt, &timer);
#elif defined(__unix__) || defined(__APPLE__)
        /// __APPLE__ was missing, so macOS -- where most of this was developed
        /// -- took the locking fallback below instead of the reentrant call it
        /// has had all along.
        localtime_r(&timer, &bt);
#else
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        bt = *std::localtime(&timer);
#endif
        return bt;
    }

    std::string tmTime2String(std::time_t t)
    {
        /// Not std::localtime(): its static tm is shared with every other thread
        /// formatting a time.
        std::tm tm = localtime_xp(t);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y.%m.%d %H:%M:%S", &tm);
        return std::string(buf);
    }

    std::string formatPeriod(int seconds)
    {
        int s = seconds % 60, minutes = seconds / 60, m = minutes % 60, hours = minutes / 60, h = hours % 24,
            d = hours / 24;

        std::ostringstream stringStream;
        if (d > 0) {
            stringStream << d << "d ";
        }
        if (h > 0) {
            stringStream << h << ":" << std::setfill('0') << std::setw(2);
        }

        stringStream << m << ":" << std::setfill('0') << std::setw(2) << s << std::setfill(' ')
                     << std::setw(0);
        return stringStream.str();
    }

    std::string secondToClockString(int second, const std::string& spString)
    {
        auto as = std::abs(second);
        auto h = as / (60 * 60), r = as % (60 * 60), m = r / 60, s = r % 60;

        std::string str, mstring, sstring;
        if (m == 0)
            mstring = h ? "00" : "0";
        else {
            if (m < 10 && h) mstring = "0";
            mstring += std::to_string(m);
        }

        if (s == 0)
            sstring = "00";
        else {
            if (s < 10) sstring = "0";
            sstring += std::to_string(s);
        }

        if (second < 0) str = "-";

        if (h > 0) {
            str += std::to_string(h) + spString;
        }

        str += mstring + spString + sstring;
        return str;
    }

} // namespace banksia
