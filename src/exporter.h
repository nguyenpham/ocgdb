/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_EXPORTER_H
#define OCGDB_EXPORTER_H

#include "dbread.h"

namespace ocgdb {

    /// -export: the games of a database (all, or those of -where) to a PGN file, in the order of
    /// their IDs, with variations and comments
    class Exporter : virtual public DbRead
    {
    protected:
        virtual bool runTask() override;
        virtual bool needsPool() const override { return false; }
    };

    /// -g: games by their IDs, printed as PGN
    class Extract : virtual public DbRead
    {
    protected:
        virtual bool runTask() override;
        virtual bool needsPool() const override { return false; }
    };

} // namespace ocgdb

#endif /* OCGDB_EXPORTER_H */
