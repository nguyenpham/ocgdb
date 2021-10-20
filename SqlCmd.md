# Basic SQL command


## Create database and tables

    DROP TABLE IF EXISTS info;
    CREATE TABLE info (name TEXT UNIQUE NOT NULL, value TEXT)

    DROP TABLE IF EXISTS event
    CREATE TABLE event (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE)

    DROP TABLE IF EXISTS player
    CREATE TABLE player (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE, elo INTEGER DEFAULT 0, countrycode TEXT DEFAULT '')

    DROP TABLE IF EXISTS move
    CREATE TABLE move(id INTEGER PRIMARY KEY AUTOINCREMENT, fen TEXT, moves TEXT)

    DROP TABLE IF EXISTS game
    CREATE TABLE game(id INTEGER PRIMARY KEY AUTOINCREMENT, event_id INTEGER, white_id INTEGER, white_elo INTEGER, black_id INTEGER, black_elo INTEGER, timer TEXT, date TEXT, result INTEGER, length INTEGER, move_id INTEGER, FOREIGN KEY(event_id) REFERENCES event, FOREIGN KEY(white_id) REFERENCES player, FOREIGN KEY(black_id) REFERENCES player, FOREIGN KEY(move_id) REFERENCES move)


## Example of insert commands

    INSERT INTO info(name, value) VALUES ('variant', 'standard')
    INSERT INTO info(name, value) VALUES ('license', 'free')

    INSERT INTO event(name) VALUES (\"\") // default empty

