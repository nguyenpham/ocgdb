# Open Chess Game Database Standard (OCGDB)

Version Beta


## Brief of main ideas/techniques
- Use SQL/SQLite as the backbone/framework for storing data and querying general information
- Approximate position searching: a) parse games on the fly b) use Position Query Language (PQL) for querying widely and dynamically
- Names, tables follow to PGN tags


## Why OCGDB? Features/Highlights
- Open databases: users could easily understand data structures, modify, convert to, or from other database formats
- It is the format supporting the highest numbers of games (tested with 94 million games, estimated it could work with billions of games)
- It is based on SQL - the strongest query language for querying general information. Users can query without using chess specific programs
- It has its own query language (PQL) for approximate-position-searching thus it can cover very widely
- It could use for all database purposes, from mobile, desktop, console to web applications 
- It is one of the formats/programs that could create the smallest chess game databases
- It is one of the fastest chess game database formats/programs when generating databases and searching
- 75% of code is SQLite and some other popular open-source libraries that are tested so widely and carefully. The rest 25% (about chess) is tested carefully too. Using this code is quite safe and can save a lot of headache and effort. Furthermore, those databases are in SQL, developers may easily, quickly develop themselves all code to work with that databases
- MIT license: you may use it for any applications/purposes unlimitedly without worrying about license conditions


We believe it is one of the fastest (in terms of speeds of creating and querying/searching), smallest (in terms of database sizes), strongest (in terms of game numbers), and smartest (in terms of querying/position-searching) chess game database programs. It could compete for all parameters, results with the best chess game database formats and their programs/tools.


## Overview

Almost all popular/published chess game databases are in binary formats. Those formats have some advantages such as being fast and compact. However, they also have some drawbacks:
 
- Hard to understand: each format requires a long list of descriptions and explanations why this but not that
- Hard to change structures: adding, removing, changing data types... usually hard and headache tasks, require change seriously both data and code
- Hard to support by other apps: other programs may get many problems to support since they may use different programming languages, data structures…
- Hard to support by web apps: processing large binary files is not a strong point of scripting languages/web apps
- License compatibility: code for those binary formats are mostly from GPL to more restricted, thus some programs may get hard to adapt or even can't support 

Typically a binary format is attached to a specific chess software and the exchange between formats is very limited.

The advantages of being binary formats were very important in the past when computers were weak and Internet speed was slow. However, they become less important nowadays since computers become much stronger and Internet is much faster. That has been making other formats become more attractive.

On the other hand, we have already the PGN format for chess games, which could be used for chess databases too. It has some advantages:
- It is a kind of open standard already. Almost all chess GUIs and tools understand and can work with
- Readable for both humans and software (because of being text format). We don’t need any special tool to read and edit, just any text editor
- So simple to understand and support


However, there are many drawbacks to using PGN as chess databases:
- It doesn’t have structures at all, there is no relationship between games, players, events. They are independent of each other. In other words, they are just collections of games, but not databases
- The data may be heavily redundant and inconsistent: names of events, players may be repeated multi times. Same events, players may be different in some games
- Processing speed is so low (compared with binary databases), especially when the number of games is large. Even for some simple queries such as count numbers of games, players,… it may take very long and unacceptable periods in modern computers
- Text size is typically large


Thus we think we need a new format that could be used to exchange databases between different apps and/or for web apps. Starting requirements:
- Easier to understand data structures. It is best if it could describe itself
- Easy to support by different languages, apps
- Fast enough for some apps to use directly
- Easy to support and fast enough to use by web apps
- Almost free for all (MIT/or less restriction license)


## SQL/SQLite
We have picked up SQL/SQLite as the main tools/direction to develop the new database. Using SQL for the chess game database is not new, someone has tried already (and gave up). We have known it has some reason drawbacks such as slow and large on size.

However, it has some advantages:
- Data could be displayed in text forms, readable for both humans and machines
- Easy to alternate structures. Adding, removing, changing fields are so simple
- Easy to understand structures. They all are almost described themselves 
- Easy to write tools, converters
- Users can query directly
- Supported by a lot of tools
- SQL: is a very strong and flexible way to make queries
- Come with many strong, matrual SQL engines and libraries


### Overcome drawbacks
We confirm we have got all necessary information and overcomed all drawbacks of using SQL/SQLite. The chess game databases work very well and they are one of the smallest and fastest ones in chess game database world.


## File name extension
It should keep the normal extension of a database. For example, the one for SQLite should have the extension .db3. It should have upper/extra extension .ocgdb to distinguish it from other databases.
For examples of file names:

```
  mb345.ocgdb.db3
  carlsen.ocgdb.db3
```


## Names
Names of tables, columns... should be Camel style, less space and close to PGN tag names as much as possible. 
For examples: ```White, BlackElo, PlyCount, GameCount, FEN```

### Some important Table names
- Events: for event names
- Sites: for site names
- Players: for player names
- Games: for game info, FEN, and Moves
- Comments: for comments of moves
- Info: for brief information such as GameCount, playerCount, EventCount...

### Field names
PGN standard requires some name tags (compulsory), the database should have those fields too. If one is an index field, uses suffix ID.
An important field is Moves to keep all moves in text form.

For examples of field names:
```EventID, WhiteID, BlackElo, Round, Result, Date, FEN, Moves```

### Field values
Except for fields of identicals such EventID, SiteID, WhiteID, BlackID, values of other fields could be NULL.

### Popular fields in table Games
We suggest beside ID, FEN and move fields, Games should have following fields according to popular PGN tags:

```EventID, SiteID, Date, Round, WhiteID, WhiteElo, BlackID, BlackElo, Result, TimeControl, ECO, PlyCount```
        
### Other tag fields in table Games
When a game has some tags that are not in the above list, users can choose to create new fields to store information of those tags or not.

### Field types
Fields PlyCount, WhiteElo, BlackElo and ones for IDs (such as ID, EventID) should be INTEGER.
Fields Moves1 and Moves2 should be BLOB.
Other fields should be TEXT.

## FEN field
A FEN string of each game could be stored (FEN strings) in that field. If the game starts from start-position, it could be saved as a NULL string.


## Moves fields
All moves of a game could be stored in some forms in some specific fields as the below:

- Moves: type TEXT; all moves including all other information such as comments are stored as a single string. For example:
  ```1.c4 e5 2.Nc3 Bb4 {A21: English, Kramnik-Shirov counterattack} 3.Nd5 Be7 4.Nxe7 Nxe7 5.d4 exd4 6.Qxd4 O-O 1/2-1/2```
- Moves2: type BLOB; each move is encoded as 2 bytes. The encode for chess: ```from | dest << 6 | promotion << 12```
- Moves1: type BLOB; each move (except Queen move) is encoded as a 1 byte when a Queen move is encoded as 2 bytes

All moves (of a game) in Moves1/Moves2 are stored as a binary array and then saved into the database as a blob.

The algorithms of Moves and Moves2 are very simple, developers can easily encode and decode using their own code.

In contrast, the algorithm of Moves1 is quite complicated, deeply integrated into our code/library. It is not easy for developers to write their own encode/decode. Even Moves1 can create the smallest databases, users should consider using Moves and/or Moves2 instead, just for being easy to use by other libraries.


## Converting speed
We tested on an iMac 3.6 GHz Quad-Core i7, 16 GB RAM (the year 2017), converting a PGN file with 3.45 million games, size 2.42 GB.

Convert into a db3 file (.db3, stored on the hard disk), required 29 seconds:

```
    #games: 3457050, elapsed: 29977 ms, 00:29, speed: 115323 games/s
```
Convert into a memory database (output path is :memory:) on the RAM, required 22 seconds:

```
    #games: 3457050, elapsed: 22067 ms, 00:22, speed: 156661 games/s
```

Convert a PGN file of 94 million games from Lichess:

```
#games: 93679650, elapsed: 5209214ms 1:26:49, speed: 17983 games/s, #blocks: 25777, processed size: 206208 MB
```

## Retrieve data
Query database and extract some important data fields:

```
for (auto cnt = 0; statement.executeStep(); cnt++) {
    auto gameID = statement.getColumn("ID").getInt64(); assert(gameID > 0);
    auto fenText = statement.getColumn("FEN").getText();
    auto moveText = statement.getColumn("Moves").getText();
}
```

Query database, extract some data fields and parse into chessboard, using multi-threads:
```
for auto cnt = 0; statement.executeStep(); cnt++) {
    auto gameID = statement.getColumn("ID").getInt64();
    auto fenText = statement.getColumn("FEN").getText();
    auto moveText = statement.getColumn("Moves").getText();
    threadParsePGNGame(gameID, fenText, moveText);
}
```

## Position Query Language (PQL)
The EBNF (Extended Backus Naur Form) of the language is as the below:

```
clause = condition { ("and" | "or" | "&&" | "||") condition }
condition = expression { ( "=" | "<" | "<="| " >" | ">=" | "==“ | "!=" | "<>" ) expression }
expression = term  {( "+" | "-" ) term }
term = factor {( "*" | "/" ]) factor} 
factor = number | piece | "(" expression ")"
piece = piecename (<empty> | square | squareset)
piecename = "K" | "Q" | "R" | "B" | "N" | "P" | "k" | "q" | "r" | "b" | "n" | "p" | "white" | "black"
squareset = column | row | "[" (square | squarerange | columnrange | rowrange) { "," (square | squarerange | columnrange | rowrange) } "]"
squarerange = square "-" square
columnrange = column "-" column
rowrange = row "-" row
square = column row 
column = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h"
row = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8"
```

Some symbols are mixed between SQL style and C/C++ style (such as users can use both <> and != for not-equal comparison) just for convenience.

A condition/expression may have some chess piece names such as Q (for white Queens), p (for black Pawns). When evaluating, they are counted as cardinalities/total numbers of those chess pieces on the chessboard.

For examples:

```
R                the total number of White Rooks
qb3              the total number of Black Queens on square b3
B3               the total number of White Bishops on row 3
bb               the total number of Black Bishops on column b
n[b-e]           the total number of Black Knights from column b to e
P[a4, c5, d5]    the total number of White Pawns on squares a4, c5, and d5
```

Users can do all calculations between those pieces, numbers and all will turn to conditions (true/false) in the end.

The condition may be implicit or explicit:

```
R         the implicit form of the comparison R != 0
R == 3        the total of White Rooks must be 3
q[5-7] >= 2     the total of Black Queens from row 5 to row 7 must be equal or larger than 2
```

### Comments
Any text after // will be trimmed out.


Some other examples:

```
// find all positions having 3 White Queens
Q = 3

// 3 White Queens, written in inverted way
3 = Q

// Find all positions having two Black Rooks in the middle squares
r[e4, e5, d4, d5] = 2

// White Pawns in d4, e5, f4, g4, Black King in b7
P[d4, e5, f4, g4] = 4 and kb7

// black Pawns in column b, row 5, from square a2 to d2 must be smaller than 4
p[b, 5, a2 - d2] < 4

// Black Pawns in column c more than 1
pc > 1

// White Pawns in row 3 from 2
P3 >= 2

// Two Bishops in column c, d, e, f
B[c-f] + b[c-f] = 2

// There are 5 white pieces in row 6
white6 = 5
```


### The Parser
Because the language is very simple and input strings are typically so short, we implement its parser in a simple, straightforward way, using the recursive method. From an input string (query), the parser will create an evaluation tree. That tree will be evaluated at every chess position with the parameters as a set of bitboards of that position. The position and its game will be picked up as the result of the evaluation of the tree is true (not zero).



## Sample databases
There are two sample databases in the samples project at:
https://github.com/nguyenpham/ocgdb-samples

- carlsen.ocgdc.db3 with over 2000 games, moves keeps as text and stored in Moves
- mb-3.45.ocgdb.db3 the MillionBase database (by Ed Schröder) of 3.45 million games, encoded moves as two bytes (Moves2)

You may open it with any SQLite browsers/tools and make some queries to understand its structures, speed, advantages, and disadvantages.


## SQL commands
The file SqlCmd.md contains some SQL commands to create databases and tables, examples to insert, and query tables.


## Code
All samples, tools are C++ 17.

## Compile
Run make file in the subfolder ```src```:

```
cd src
make
```

In macOS, Windows, you can run and compile with Xcode/Visual Studio with the project files in the folder ```projects```.


## Usage

Users may run the program:
- for creating SQLite databases from PGN files:
```
ocgdb -pgn c:\games\big.png -db c:\db\big.ocgdb.db3 -cpu 4 -o moves
```

- for exporting a SQLite database into a PGN file:
```
ocgdb -pgn c:\games\big.png -db c:\db\big.ocgdb.db3 -cpu 4 -export
```

- for checking duplicate games of a SQLite database:
```
ocgdb -db c:\db\big.ocgdb.db3 -cpu 4 -dup o printall;remove
```

- for querying positions:
```
ocgdb -db c:\db\big.ocgdb.db3 -cpu 4 -q "Q=3" -q"P[d4, e5, f4, g4] = 4 and kb7"
```

- for extracting a game with ID number:
```
ocgdb -db c:\db\big.ocgdb.db3 -g 1321"
```

## History
* 25/01/2022: Version Beta
* 23/01/2022: Version Alpha
* 20/11/2021: Improve/clean code, improve speed for benchmark
* 16/11/2021: Improve speed for converter, convert 3.45 million games under a minute
* 8/11/2021: Improve speed for converter, from 6 to over 247 times faster
* 24/10/2021: first release of source-code
* 20/10/2021: project created with a sample database


## License
MIT: Almost totally free (requires just fair use). All documents, codes, data samples in this project are ready and free to integrate into other apps without worrying about license compatibility.



