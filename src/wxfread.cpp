/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifdef _XIANGQI_

#include <cstring>

#include "wxfread.h"

namespace {
    /// WXF tags, in the order of WxfTag, and the PGN tags they become (Banksia GUI's
    /// banksia/game/game.cpp and banksia/base/types.h)
    enum class WxfTag { format, game, time, result, white, black, date, place, event, fen, author, start,
                        white_elo, black_elo, none };

    const char* WXFTags[] = { "FORMAT", "GAME",  "TIME", "RESULT", "RED",    "BLACK", "DATE",
                              "PLACE",  "EVENT", "FEN",  "AUTHOR", "START{", nullptr, nullptr };

    const char* WXF2PGNTags[] = {
        nullptr, nullptr, nullptr, "Result", "White", "Black", "Date", "Place", "Event", "FEN", nullptr,
        nullptr, // start
        "WhiteElo", "BlackElo",
        nullptr, nullptr
    };

    /// how CXQ games ended, kept as the tag Termination
    const char* cxqReasons[] = {
        "max moves reached",
        "movetime up",
        "timeup",
        "perpetual chase",
        "request",
        nullptr, nullptr
    };

    const std::string jieqiPieceNames = "AERCHPaerchp";
} // namespace

using namespace ocgdb;

int WXFRead::fixJieqiWXFStringOfCXQ(char* wxfString)
{
    assert(wxfString);
    auto cnt = 0;

    for (auto&& ch : jieqiPieceNames) {
        for (auto p = wxfString + 1; *p;) {
            auto q = strchr(p, ch);
            if (!q) break;

            if (*(q - 1) == ' ' && *(q + 1) <= ' ') {
                *(q - 1) = '=';
                cnt++;
            }

            p = q + 1;
        }
    }

    return cnt;
}

void WXFRead::processDataBlock(char* buffer, long sz, bool connectBlock)
{
    if (isPgn) {
        PGNRead::processDataBlock(buffer, sz, connectBlock);
        return;
    }

    assert(buffer && sz > 0);

    std::unordered_map<const char*, char*> tagMap;

    auto tag = WxfTag::none;
    char* tagBegin = nullptr;

    bool scanEOL = true;
    for (char *p = buffer, *end = buffer + sz; p < end && working; p++) {
        char ch = *p;

        if ('A' <= ch && ch <= 'Z') {
            for (auto i = 0; WXFTags[i]; i++) {
                auto len = strlen(WXFTags[i]);
                if (memcmp(p, WXFTags[i], len) == 0 && !isalnum(*(p + len))) {
                    if (!tagBegin) {
                        tagBegin = p;
                    }
                    tag = static_cast<WxfTag>(i);
                    p += len + 1;

                    if (tag == WxfTag::start) {
                        auto q = strstr(p, "}END");
                        if (q) {
                            *q = 0;

                            if (variant == banksia::ChessVariant::jieqi) {
                                fixJieqiWXFStringOfCXQ(p);
                            }
                            processPGNGame(tagMap, p);
                            tagBegin = nullptr;
                            p = q + 5;
                        }
                        break;
                    }

                    if (!WXF2PGNTags[i]) {
                        break;
                    }


                    /// whitespaces
                    while (p < end && *p && *p != '\n' && *p != '\r' && !isalnum(*p)) {
                        p++;
                    }
                    /// EOL
                    auto q = p;
                    while (p < end && *p && *p != '\n' && *p != '\r') {
                        p++;
                    }

                    scanEOL = false;


                    if (q < p) {
                        *p = 0;

                        tagMap[WXF2PGNTags[i]] = q;

                        if (tag == WxfTag::event) {
                            auto s = "CXQ Game"; // trim for "CXQ Game ; 20m/240s/24s"
                            auto x = strstr(q, s);
                            if (x) {
                                auto l = strlen(s);
                                x[l] = 0;
                            }
                        } else if (tag == WxfTag::date) {
                            auto x = strstr(q, " ");
                            if (x) {
                                *x = 0;
                            }
                        } else if (tag == WxfTag::result) {
                            auto l = 3;
                            auto x = strstr(q, "1-0");
                            if (!x) x = strstr(q, "0-1");
                            if (!x) {
                                x = strstr(q, "0.5-0.5");
                                l = 7;
                            }
                            if (x) {
                                x[l] = 0;
                                tagMap[WXF2PGNTags[i]] = x;

                                x += l + 1;
                                for (auto j = 0;; j++) {
                                    auto s = cxqReasons[j];
                                    if (!s) break;
                                    auto y = strstr(x, s);
                                    if (y) {
                                        l = strlen(s);
                                        y[l] = 0;
                                        tagMap["Termination"] = y;
                                        break;
                                    }
                                }
                            }

                        } else {
                            auto x = strchr(q, ';');
                            if (x) {
                                *x = 0;

                                for (x++; *x == ' '; x++)
                                    ;

                                if (tag == WxfTag::white || tag == WxfTag::black) {
                                    auto y = strchr(x, ';');
                                    if (y) {
                                        *y = 0;
                                        if (strlen(x) > 3) {
                                            auto tagElo = tag == WxfTag::white
                                                                  ? WxfTag::white_elo
                                                                  : WxfTag::black_elo;
                                            tagMap[WXF2PGNTags[static_cast<int>(tagElo)]] = x;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        if (scanEOL) {
            while (p < end && *p != '\n') {
                p++;
            }
        }
    }

    if (connectBlock) {
        processHalfBegin(tagBegin, (long)sz - (tagBegin - buffer));
    }
}

#endif // _XIANGQI_
