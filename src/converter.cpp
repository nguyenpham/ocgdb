//
//  converter.cpp
//  ocgdb
//
//  Created by Nguyen Pham on 23/4/2022.
//

#include "converter.h"

using namespace ocgdb;

static char fenName[50], eventName[50], event[50], siteName[50], site[50], white[50], black[50], moves[1024 * 2], empty[1];

void Converter::runTask()
{
    if (paraRecord.pgnPaths.empty()) {
        return;
    }

    startTime = getNow();

    strcpy(fenName, "FEN");
    strcpy(eventName, "Event");
    strcpy(event, "Lichess Puzzles");
    strcpy(siteName, "Site");
    empty[0] = 0;
    site[0] = 0;

    strcpy(white, "White");
    strcpy(black, "Black");



    assert(!paraRecord.dbPaths.empty());
    auto dbPath = paraRecord.dbPaths.front();
    // remove old db file if existed
    std::remove(dbPath.c_str());

    // init
    initDb();

    if (!mDb) {
        return;
    }
    
    processCSVFile(paraRecord.pgnPaths.front());

    updateInfoTable();
}

void Converter::processCSVFile(const std::string& csvPath)
{
    assert(!csvPath.empty());
    
    auto t = getThreadRecord(); assert(t);

    std::ifstream inFile(csvPath);
    const int MAX_LENGTH = 1024 * 16;
    char* line = new char[MAX_LENGTH];

    gameCnt = 0;
    printStats();
    while (inFile.getline(line, MAX_LENGTH) && strlen(line) > 0) {
        processCSVLine(t, line);
        
        if ((gameCnt & 0x7fff) == 0) {
            printStats();
        }
    }

    delete[] line;
    inFile.close();
    printStats();
}


// PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,GameUrl

void Converter::processCSVLine(ThreadRecord* t, char* line)
{
    assert(line);
    
    std::vector<char*> list;
    
    auto p = line;
    for(auto i = 0; p && *p && i < 8; i++) {
        list.push_back(p);

        p = strchr(p, ',');
        if (!p) {
            break;
        }
        *p = 0;
        ++p;
    }
    
    sprintf(moves, "{Rating: %s\n%s}\n%s", list[3], list[7], list[2]);
    
    std::unordered_map<char*, char*> itemMap;
    
    itemMap[eventName] = event;
////    itemMap[siteName] = empty;
//    itemMap[white] = white;
//    itemMap[black] = black;

    itemMap[fenName] = list[1];
    processPGNGameWithAThread(t, itemMap, moves);
}

