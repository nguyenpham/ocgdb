//
//  converter.hpp
//  ocgdb
//
//  Created by Nguyen Pham on 23/4/2022.
//

#ifndef converter_hpp
#define converter_hpp

#include "dbread.h"
#include "addgame.h"

namespace ocgdb {

class Converter : public Builder
{
public:
    
private:
    virtual void runTask() override;

private:
    void processCSVFile(const std::string& csvPath);
    void processCSVLine(ThreadRecord* t, char* line);

};

} // namespace ocdb

#endif /* converter_hpp */
