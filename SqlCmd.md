# Basic SQL command


## Create database and tables

    DROP TABLE IF EXISTS Info
    CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT);
    INSERT INTO Info (Name, Value) VALUES ('Version', '0.1');
    INSERT INTO Info (Name, Value) VALUES ('Variant', 'standard');
    INSERT INTO Info (Name, Value) VALUES ('License', 'free');

    DROP TABLE IF EXISTS Events;
    CREATE TABLE Events (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE);
    INSERT INTO Events (Name) VALUES ("

    DROP TABLE IF EXISTS Sites;
    CREATE TABLE Sites (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE);
    INSERT INTO Sites (Name) VALUES ("

    DROP TABLE IF EXISTS Players;
    CREATE TABLE Players (ID INTEGER PRIMARY KEY, Name TEXT UNIQUE, Elo INTEGER);
    INSERT INTO Players (ID, Name) VALUES (1, "

    DROP TABLE IF EXISTS Games;
    CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT, EventID INTEGER, SiteID INTEGER, Date TEXT, Round INTEGER, WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER, Result INTEGER, Timer TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT, Moves TEXT, FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites, FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players);


## Example of insert commands
    INSERT INTO Info (Name, Value) VALUES ('Version', '0.1');
    INSERT INTO Info (Name, Value) VALUES ('Variant', 'standard');
    INSERT INTO Info (Name, Value) VALUES ('License', 'free');

    INSERT INTO Games (EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        
## Example of querying a game
    SELECT g.ID, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves
    FROM Games g
    INNER JOIN Players w ON WhiteID = w.ID
    INNER JOIN Players b ON BlackID = b.ID
    WHERE g.ID = ?
