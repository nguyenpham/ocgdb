/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021 developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef Funcs_h
#define Funcs_h

#include <stdio.h>
#include <string>
#include <vector>

#include "types.h"

class Funcs
{
public:
    static bool isChessFamily(ocgdb::ChessVariant variant);
    static std::string getOriginFen(ocgdb::ChessVariant variant);

    static std::string resultType2String(ocgdb::ResultType type, bool shortFrom);
    static ocgdb::ResultType string2ResultType(const std::string& s);
    static std::string reasonType2String(ocgdb::ReasonType type);
    static ocgdb::ReasonType string2ReasonType(const std::string& s);
    static std::string side2String(ocgdb::Side side, bool shortFrom);
    static ocgdb::Side string2Side(std::string s);

    static std::string chessVariant2String(ocgdb::ChessVariant variant);
    static char chessPieceType2Char(int pieceType);
    static int chessCoordinateStringToPos(const std::string& str);
    static std::string chessPosToCoordinateString(int pos);
    static int chessCharactorToPieceType(char ch);

    static ocgdb::ChessVariant string2ChessVariant(std::string s);

    static void toLower(std::string& str);
    static void toLower(char* str);
    static std::string& trim(std::string& s);

    static std::vector<std::string> splitString(const std::string &s, char delim);
    static std::vector<std::string> splitString(const std::string& s, const std::string& del);

    static std::string replaceString(std::string subject, const std::string& search, const std::string& replace);

    static std::string score2String(int centiscore, bool pawnUnit);
    static std::string score2String(double score, bool pawnUnit);

    static std::string secondToClockString(int second, const std::string& spString);
    
    static char* trim(char*);
};

#endif /* Funcs_h */
