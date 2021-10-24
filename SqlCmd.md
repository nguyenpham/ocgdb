# Basic SQL command


## Create database and tables

    DROP TABLE IF EXISTS info;
    CREATE TABLE info (name TEXT UNIQUE NOT NULL, value TEXT)

    DROP TABLE IF EXISTS event
    CREATE TABLE event (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)

    DROP TABLE IF EXISTS player
    CREATE TABLE player (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, elo INTEGER DEFAULT 0)

    DROP TABLE IF EXISTS game
    CREATE TABLE game(id INTEGER PRIMARY KEY AUTOINCREMENT, event_id INTEGER, white_id INTEGER, white_elo INTEGER, black_id INTEGER, black_elo INTEGER, timer TEXT, date TEXT, eco TEXT, result INTEGER, length INTEGER, fen TEXT, moves TEXT, FOREIGN KEY(event_id) REFERENCES event, FOREIGN KEY(white_id) REFERENCES player, FOREIGN KEY(black_id) REFERENCES player)


## Example of insert commands

    INSERT INTO info(name, value) VALUES ('version', '0.1')
    INSERT INTO info(name, value) VALUES ('variant', 'standard')
    INSERT INTO info(name, value) VALUES ('license', 'free')

    INSERT INTO event(name) VALUES (\"\") // default empty

    SELECT g.id, w.name white, white_elo, b.name black, black_elo, timer, date, result, eco, length, fen, moves
    FROM game g
    INNER JOIN player w ON white_id = w.id
    INNER JOIN player b ON black_id = b.id
    WHERE result = '1-0' AND length  > 100
