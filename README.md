# Open Chess Game Database Standard (OCGDB)

Version Alpha

## Features/high lights
- It has an open data structure: very easy to understand, change, convert to/from other formats
- It could be used already by 3rd-party programs (SQL browsers/tools)
- It could work well with over 90 million games. So far it is only the chess game database program (except some database servers) could work with that number of games. We estimate it could work well too with several times larger games
- It could convert data from PGN files with very high speed, as fast as the fastest programs
- It could create databases with very small sizes (depending on configurations), as small as the smallest ones created by binary formats
- It could perform approximate-position-searching (to answer questions such as find positions/games with 3 white Queens; white Pawns in d4, e4, f5, g6, and black King in b7) with very high speed, as fast as any fastest programs
- It has its own Position Query Language and parser to query for approximate-position-searching
- It uses SQL - the strongest query language for databases. That could help to query any information game headers, from simple to very complicated queries- It has its own query language for approximate-position-searching. It is so simple but extremely strong to query
- It has all the basic functions of a database program (for converting, searching) and more functions will be added constantly

We believe it is one of the fastest, smallest (in terms of database sizes), and strongest (in terms of game numbers and position-searching) chess game database programs. It could compete for all parameters, results with the best chess game database format and its programs/tools.


## Overview

Almost all popular/published chess game databases are in binary formats. Those formats have some advantages such as fast and compact. However, they also have some drawbacks:
 
- Hard to understand: each format requires a long list of descriptions and explanations why this but not that
- Hard to change strutures: adding, removing, changing data types... usually hard and headach tasks, require change seriously both data and code
- Hard to support by other apps: other programs may get many problems to support since they can use different programming languages, data structures…
- Hard to support by web apps: processing large binary files is not strong points of scripting languages/web apps
- License compatibility: code for those binary formats are mostly from GPL to more restricted, thus some programs may get hard to adapt or even can't support 

Typically a binary format is sticked to a specific chess software and the exchange between formats is very limited.

The advantages of being binary formats were very important in the past when computers were weak and Internet speed was slow. However, they become less important nowadays since computers become much stronger and Internet is much faster. That has been making other formats become more attractive.

On the other hand, we have the PGN format for chess games, which could be used for chess databases too. It has some advantages:
- It is a kind of open standard already. Almost all chess GUIs and tools understand and can work with
- Readable for both human and software (because of being text format). We don’t need any special tool to read and edit, just any text editor
- So simple to understand and support


However, there are many drawbacks for using PGN as chess databases:
- It doesn’t have structures at all, there is no relationship between games, players, events. They are independent to each other. In other words, they are just collections of games, but not databases
- The data may be heavily redundant and inconsistent: names of events, players may be repeated multi times. Same events, players may be different in some games
- Processing speed is so low (compared with binary databases), especially when the number of games large. Even for some simple queries such as count numbers of games, players,… it may take very long and unacceptable periods in modern computers
- Text size is typically large


Thus we think we need a new format which could be used to exchange databases between different apps and/or for web apps. Requirements:
- Easier to understand data structures. It is the best if it could describe itself
- Easy to support by different languages, apps
- Fast enough for some apps to use directly
- Easy to support and fast enough to use by web apps
- Almost free for all (MIT/or less restriction license)


## SQL/SQLite
We have picked up SQL/SQLite as the main tools/direction to develop the new database. Using SQL for chess game database is not new, someone have tried already (and gave up). We have known it has some reathom drawbacks such as slow and larege on size, but we overcome all that problems.

However it has some advantages:
- Data could be displayed in text forms, readable for both human and machines
- Easy to alternate structures. Adding, removing, changing fields are so simple
- Easy to understand structures. They all are almost described themselves 
- Easy to write tools, converters
- Users can query directly
- Supported by a lot of tools
- SQL: is a very strong and flexible way to make queries
- Come with many strong, matual SQL engines and libraries


## Converting speed
We tested on an iMac 3.6 GHz Quad-Core i7, 16 GB RAM (year 2017), converting a PGN file with 3.45 million games, size 2.42 GB.

Convert into a db3 file (.db3, stored on the hard disk), required 29 seconds:

```
    #games: 3457050, elapsed: 29977 ms, 00:29, speed: 115323 games/s
```
Convert into a memory database (output path is :memory:) on the RAM, required 22 seconds:

```
    #games: 3457050, elapsed: 22067 ms, 00:22, speed: 156661 games/s
```

## File name extension
It should keep normal extension of a database. For example, the one for SQLite should have the extension .db3. It should have upper/extra extension .ocgdb to distinguide from other database.
For examples of file names:

```
  mb345.ocgdb.db3
  carlsen.ocgdb.db3
```


## Names
Names of tables, columnes... should be Camel style, less space and close to PGN tag names as much as posible. 
For examples: ```White, BlackElo, PlyCount, GameCount, FEN```

### Some important Table names
- Events: for event names
- Sites: for site names
- Players: for player names
- Games: for game info, FEN and Moves
- Comments: for comments of moves

### Field names
PGN standard requires some name tags (compusory), the database should have those fields too. If one is an index field, uses surfix ID.
An important field is Moves to keep all moves in text form.

For examples of field names:
```EventID, WhiteID, BlackElo, Round, Result, Date, FEN, Moves```

### Field values
Except for fields of identicals such EventID, SiteID, WhiteID, BlackID, values of other fields could be NULL.


## FEN field
A FEN string of each game could be stored them (FEN strings) in that field. If the game starts from start-position, doesn't require FEN, it could be saved as a NULL string.


## Moves fields
All moves of a game could be stored in some forms in some specific fields as the below:
- Moves: all moves including all other information such as comments are stored as a single string
- Moves1: each move (except Queen move) is encoded as a 1 byte when a Queen move is encoded as 2 bytes. All moves (of a game) are stored as an binary array and then saved into the database as a blob
- Moves2: similar to Moves1 but each move is encoded as 2 bytes


## Position query language (PQL)
The EBNF (Extended Backus Naur Form) of the language as the below:

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

Some other examples:

```
// find all positions having 3 White Queens
Q = 3

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
Because the language is very simple and input strings are typically so short, we implement its parser in a simple, straightforward way, using recursive method. From an input string (query), the parser will create an evaluation tree. That tree will be evaluated at every chess position with the parameters as a set of bitboards of that position. The position and its game will be picked up as the result if the evaluation of the tree is true (not zero).

### Matching

```
SELECT g.ID, g.Round, Date, w.Name White, WhiteElo, b.Name Black, BlackElo, Result, Timer, ECO, PlyCount, FEN, Moves FROM Games g INNER JOIN Players w ON WhiteID = w.ID INNER JOIN Players b ON BlackID = b.ID WHERE g.Moves LIKE '1.d4 Nf6 2.Nf3 d5 3.e3 Bf5 4.c4 c6 5.Nc3 e6%'
```


## Data and code

## Sample databases
There are two sample databases in the folder samples:

- carlsen.ocgdc.db3 with over 2000 games, moves keeps as text and stored in Moves
- mb-3.45.ocgdb.db3.zip: the MillionBase database (by Ed Schröder) of 3.45 million games, encoded moves as two bytes (Moves2)

You may open it with any SQLite browsers/tools and make some query to understand its structures, speed, advantages and disadvantages.


### Code

#### SQL commands
The file SqlCmd.md contain some SQL commands to create databases and tables, examples to insert and query tables.

#### Cpp code
All samples, tools are C++ 17.

## Compile
Run make file in the subfolder ```src```:

```
cd src
make
```
In macOS, you can run and compile with xCode with the project file in the folder ```projects```.


## History
* 21/01/2022: Version Alpha
* 20/11/2021: Improve/clean code, improve speed for benchmark
* 16/11/2021: Improve speed for converter, convert 3.45 million games under a minute
* 8/11/2021: Improve speed for converter, from 6 to over 247 times faster
* 24/10/2021: first release of source-code
* 20/10/2021: project created with a sample database


## License
MIT: Almost totally free (requires just fair use). All documents, codes, data samples in this project are ready and free to integrate into other apps without worrying about license compatibility.


