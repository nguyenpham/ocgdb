/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2021-2022 Nguyen Pham (github@nguyenpham)
 * Copyright (c) 2021-2022 Developers
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_EXTRACT_H
#define OCGDB_EXTRACT_H

#include "dbread.h"

namespace ocgdb {


class Extract : public DbRead
{
public:
    void getGame(const ParaRecord&);
    
private:
    virtual void runTask() override;

};

} // namespace ocdb

#endif /* OCGDB_EXTRACT_H */
