/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#include <sstream>
#include <iomanip>
#include <cstring>
#include <assert.h>

#ifdef _WIN32

//#include <windows.h>
//#include <direct.h>
//#include <stdlib.h> // for full path
//#include <filesystem>

#else

//#include <glob.h>
//#include <dirent.h>
#include <sys/stat.h>
//#include <unistd.h>
#endif

#include "funcs.h"
#include "chess.h"

using namespace bslib;

const char* pieceTypeName = ".kqrbnp";

const char* reasonStrings[] = {
    "*", "mate", "stalemate", "repetition", "resign", "fifty moves", "insufficient material",
    "illegal move", "timeout",
    "adjudication by lengths", "adjudication by egtb", "adjudication by engines' scores", "adjudication by human",
    "perpetual chase",
    "both perpetual chases", "extra comment", "crash", "abort",
    nullptr
};

// noresult, win, draw, loss
const char* resultStrings[] = {
    "*", "1-0", "1/2-1/2", "0-1", nullptr
};
const char* resultStrings_short[] = {
    "*", "1-0", "0.5", "0-1", nullptr
};

const char* sideStrings[] = {
    "black", "white", "none", nullptr
};
const char* shortSideStrings[] = {
    "b", "w", "n", nullptr
};

const char* variantStrings[] = {
    "standard", "chess960", nullptr
};

static std::string originFens[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq",
};


bool Funcs::isChessFamily(ChessVariant variant)
{
    return variant == ChessVariant::standard || variant == ChessVariant::chess960;
}

std::string Funcs::getOriginFen(ChessVariant variant)
{
    return originFens[static_cast<int>(variant)];
}

std::string Funcs::resultType2String(ResultType type, bool shortFrom) {
    auto t = static_cast<int>(type);
    if (t < 0 || t > 3) t = 0;
    return shortFrom ? resultStrings_short[t] : resultStrings[t];
}

ResultType Funcs::string2ResultType(const std::string& s)
{
    for(int i = 0; resultStrings[i]; i++) {
        if (resultStrings[i] == s) {
            return static_cast<ResultType>(i);
        }
    }
    return ResultType::noresult;
}

std::string Funcs::reasonType2String(ReasonType type)
{
    auto t = static_cast<int>(type);
    if (t < 0) t = 0;
    return reasonStrings[t];
}

ReasonType Funcs::string2ReasonType(const std::string& s)
{
    for(int i = 0; reasonStrings[i]; i++) {
        if (reasonStrings[i] == s) {
            return static_cast<ReasonType>(i);
        }
    }
    return ReasonType::noreason;
}

std::string Funcs::side2String(Side side, bool shortFrom)
{
    auto sd = static_cast<int>(side);
    if (sd < 0 || sd > 1) sd = 2;
    return shortFrom ? shortSideStrings[sd] : sideStrings[sd];
}

Side Funcs::string2Side(std::string s)
{
    toLower(s);
    for(int i = 0; sideStrings[i]; i++) {
        if (sideStrings[i] == s || shortSideStrings[i] == s) {
            return static_cast<Side>(i);
        }
    }
    return Side::none;

}

std::string Funcs::chessVariant2String(ChessVariant variant)
{
    auto t = static_cast<int>(variant);
    if (t < 0 || t >= static_cast<int>(ChessVariant::none)) t = 0;
    return variantStrings[t];
}


char Funcs::chessPieceType2Char(int pieceType)
{
    return pieceTypeName[pieceType];
}

int Funcs::chessCoordinateStringToPos(const std::string& str)
{
    auto colChr = str[0], rowChr = str[1];
    if (colChr >= 'a' && colChr <= 'h' && rowChr >= '1' && rowChr <= '8') {
        int col = colChr - 'a';
        int row = rowChr - '1';

        return (7 - row) * 8 + col;
    }
    return -1;
}

std::string Funcs::chessPosToCoordinateString(int pos)
{
    int row = pos / 8, col = pos % 8;
    std::ostringstream stringStream;
    stringStream << char('a' + col) << 8 - row;
    return stringStream.str();
}



int Funcs::chessCharactorToPieceType(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        ch += 'a' - 'A';
    }
    const char* p = strchr(pieceTypeName, ch);
    if (p != nullptr) {
        int k = (int)(p - pieceTypeName);
        return k;
    }

    return EMPTY;
}

ChessVariant Funcs::string2ChessVariant(std::string s)
{
    if (s.empty()) return ChessVariant::standard;

    toLower(s);
    for(int i = 0; variantStrings[i]; i++) {
        if (variantStrings[i] == s) {
            return static_cast<ChessVariant>(i);
        }
    }

    if (s.find("960") != std::string::npos || s == "fischerandom" || s == "fische random") {
        return ChessVariant::chess960;
    }
    return s == "orthodox" ? ChessVariant::standard : ChessVariant::none;
}




void Funcs::toLower(std::string& str)
{
    for (size_t i = 0; i < str.size(); ++i) {
        str[i] = tolower(str[i]);
    }
}

void Funcs::toLower(char* str)
{
    for (size_t i = 0; str[i]; ++i) {
        str[i] = tolower(str[i]);
    }
}

static const char* trimChars = " \t\n\r\f\v";

// trim from left
std::string& ltrim(std::string& s)
{
    s.erase(0, s.find_first_not_of(trimChars));
    return s;
}

// trim from right
std::string& Funcs::rtrim(std::string& s)
{
    s.erase(s.find_last_not_of(trimChars) + 1);
    return s;
}

// trim from left & right
std::string& Funcs::trim(std::string& s)
{
    return ltrim(rtrim(s));
}

std::vector<std::string> Funcs::splitString(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        auto s = trim(item);
        if (!s.empty())
            elems.push_back(s);
    }
    return elems;
}

std::vector<std::string> Funcs::splitString(const std::string& s, const std::string& del)
{
    std::vector<std::string> vec;
    int start = 0;
    int end = s.find(del);
    while (end != -1) {
        vec.push_back(s.substr(start, end - start));
        start = end + del.size();
        end = s.find(del, start);
    }
    vec.push_back(s.substr(start, end - start));
    return vec;
}

std::string Funcs::replaceString(std::string subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
        if (pos >= subject.size()) break;
    }
    return subject;
}

std::string Funcs::score2String(int centiscore, bool pawnUnit)
{
    if (pawnUnit) {
        auto score = static_cast<double>(centiscore) / 100;
        return score2String(score, true);
    }
    return std::to_string(centiscore);
}

std::string Funcs::score2String(double score, bool pawnUnit)
{
    if (pawnUnit) {
        std::ostringstream stringStream;
        stringStream << std::fixed << std::setprecision(2) << std::showpos << score;
        return stringStream.str();
    }

    auto centiscore = static_cast<int>(score * 100);
    return score2String(centiscore, false);
}

std::string Funcs::secondToClockString(int second, const std::string& spString)
{
    auto as = std::abs(second);

    auto h = as / (60 * 60), r = as % (60 * 60), m = r / 60, s = r % 60;

    std::string str, mstring, sstring;
    if (m == 0) mstring = "00";
    else {
        if (m < 10) mstring = "0";
        mstring += std::to_string(m);
    }
    if (s == 0) sstring = "00";
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

char* Funcs::trim(char* s)
{
    if (s) {
        // trim left
        while(*s <= ' ' && *s > 0) s++;

        // trim right
        for(auto len = strlen(s); len > 0; --len) {
            auto ch = s[len - 1];
            if (ch <= ' ' && ch > 0) s[len - 1] = 0;
            else break;
        }
    }

    return s;
}

size_t Funcs::getFileSize(FILE * file)
{
    assert(file);
    
#ifdef _MSC_VER
    _fseeki64(file, 0, SEEK_END);
    size_t size = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);
#else
    //probably should be fseeko/ftello with #define _FILE_OFFSET_BITS 64
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
#endif
    return size;
}


#ifdef _WIN32
size_t Funcs::getFileSize(const std::string& path)
{
    // Good to compile with Visual C++
    struct __stat64 fileStat;
    int err = _stat64(path.c_str(), &fileStat);
    if (0 != err) return 0;
    return fileStat.st_size;
}

#else
size_t Funcs::getFileSize(const std::string& fileName)
{
    struct stat st;
    if (stat(fileName.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_size;
}

#endif


BoardCore* Funcs::createBoard(ChessVariant variant)
{
    return variant == ChessVariant::standard ? new ChessBoard() : nullptr;
}


std::ofstream Funcs::openOfstream2write(const std::string& path)
{
#ifdef _WIN32
    return std::ofstream(std::filesystem::u8path(path.c_str()), std::ios_base::out | std::ios_base::app);
#else
    return std::ofstream(path, std::ios_base::out | std::ios_base::app);
#endif

}
