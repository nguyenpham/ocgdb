/**
 * This file is part of Open Chess Game Database Standard.
 *
 * Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
 * or copy at http://opensource.org/licenses/MIT)
 */

#ifndef OCGDB_SQLCMD_H
#define OCGDB_SQLCMD_H

#include "dbcore.h"

namespace ocgdb {

    /// -sql: runs SQL statements on a database and prints what they return
    class SqlCmd : virtual public DbCore
    {
    protected:
        virtual bool runTask() override;
        virtual bool needsPool() const override { return false; }

    private:
        bool runStatement(const std::string& sql);
    };

    /// -info: what a database holds -- its Info table, its tables, the columns of Games
    class DbInfo : virtual public DbCore
    {
    protected:
        virtual bool runTask() override;
        virtual bool needsPool() const override { return false; }
    };

} // namespace ocgdb

#endif /* OCGDB_SQLCMD_H */
