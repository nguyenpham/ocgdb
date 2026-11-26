# OCGDB user guide

Open Chess Game Database Standard — the programs `ocgdb` and `ocgdb-xq`, version 2.0.0.

This guide is for people who use the programs. How they are built is in
[building.md](building.md), how they are tested in [testing.md](testing.md), and how their chess
and xiangqi code is organised in [core.md](core.md).

**Contents**

1. [What OCGDB is](#1-what-ocgdb-is)
2. [Getting the programs](#2-getting-the-programs)
3. [Ready-made databases](#3-ready-made-databases) — Banksia Masters, MoonRiver Masters, Lichess Puzzles
4. [Quick start](#4-quick-start)
5. [Concepts](#5-concepts)
6. [The command line](#6-the-command-line)
7. [The tasks](#7-the-tasks) — create, merge, export, query, sql, get games, duplicates, info, bench
8. [PQL, the Position Query Language](#8-pql-the-position-query-language)
9. [SQL and the database format](#9-sql-and-the-database-format)
10. [Working with Banksia GUI and MoonRiver](#10-working-with-banksia-gui-and-moonriver)
11. [Speed and size](#11-speed-and-size)
12. [Troubleshooting](#12-troubleshooting)
13. [What is new in version 2](#13-what-is-new-in-version-2)

---

## 1. What OCGDB is

OCGDB is a way to keep chess and xiangqi games in an **SQLite** database, and two console
programs that work with such databases:

| Program | Games |
|---|---|
| `ocgdb` | standard chess, chess960 |
| `ocgdb-xq` | xiangqi (Chinese chess), jieqi (xiangqi with covered pieces) |

Both programs have the same commands. With them you can:

- **create** a database from PGN files (xiangqi: also WXF files);
- **merge** more PGN files, or other databases, into a database;
- **export** a database, or part of it, to PGN;
- **search positions** with PQL, a small language for positions and moves ("three queens", "a
  knight on e5 and later no Black queen", "mate by a knight", "this structure, shifted anywhere");
- **query** with SQL — any SQL that SQLite understands;
- print **games** by their IDs;
- find and remove **duplicate** games;
- **encrypt** databases with a password;
- see what a database holds (**info**) and measure the speed (**bench**).

The databases are ordinary SQLite files: any SQLite tool (the `sqlite3` shell, DB Browser for
SQLite, Python, a web server) can read them, and the format is described in §9. Banksia GUI and
MoonRiver read and write the same databases (§10).

The programs are command-line only: no window, no Qt. They use all cores of the computer.

## 2. Getting the programs

Build them from the source (details in [building.md](building.md)):

```bash
git clone https://github.com/nguyenpham/ocgdb.git
cd ocgdb/src
make -j8
```

This makes `src/ocgdb` and `src/ocgdb-xq`. Copy them anywhere on your `PATH`, or run them from
`src/`. Running a program without parameters, or with `-help`, prints the list of parameters and
some examples. `-version` prints the version.

Check the build with the tests (about 10 seconds), from the folder `ocgdb`:

```bash
tests/run-tests.sh
```

## 3. Ready-made databases

You do not need to build a database to try OCGDB. Large, free databases in this format are
published as GitHub releases of two data repositories; the examples of this guide use them.

| Database | Where | Games | Download | Unpacked |
|---|---|---:|---:|---:|
| **Banksia Masters** — chess games between human players, 1801 onwards: both players on the FIDE rating list or from before it, at least 20 plies, names in the FIDE style `Last, First`, duplicates removed | [BanksiaChessHub](https://github.com/nguyenpham/BanksiaChessHub), release `banksiamasters-db`, file `banksiamasters.ocgdb.db3.7z` | 8,008,264 | 855 MB | 1.87 GB |
| **MoonRiver Masters** — xiangqi games between human players, 1905 onwards (championships, leagues, opens; online games when both players are rated 2500 or more), duplicates removed | [MoonRiverChineseChessHub](https://github.com/nguyenpham/MoonRiverChineseChessHub), release `moonrivermasters-db`, file `moonrivermasters.ocgdb.db3.7z` | 135,030 | 14.8 MB | 31.3 MB |
| **Lichess Puzzles** — the Lichess puzzle database (CC0) as games: each puzzle starts from its FEN, its rating and themes are the first comment | [BanksiaChessHub](https://github.com/nguyenpham/BanksiaChessHub), release `lichess-puzzles`, file `lichesspuzzles2-4M.ocgdb.db3.zip` | 2,440,210 | 148 MB | 373 MB |

Download a file from the release page (or with the GitHub command-line tool), then unpack it with
7-Zip (`7z x`, or `7zz x` on macOS), the Windows 11 Explorer, or `unzip` for the `.zip`:

```bash
gh release download banksiamasters-db -R nguyenpham/BanksiaChessHub
gh release download moonrivermasters-db -R nguyenpham/MoonRiverChineseChessHub
7z x banksiamasters.ocgdb.db3.7z          # banksiamasters.ocgdb.db3, 1.87 GB
7z x moonrivermasters.ocgdb.db3.7z        # moonrivermasters.ocgdb.db3, 31.3 MB
```

The releases list the SHA-256 of each file. The same data repositories also publish opening books
built from these games (Polyglot, OOBS, MoonRiver `.mrbk`).

What `-info` shows for MoonRiver Masters:

```
$ ocgdb-xq -info -db moonrivermasters.ocgdb.db3
Database: moonrivermasters.ocgdb.db3
File size: 31264768 bytes

Info table:
  Data Structure Version: 0.6
  Type: OCGDB
  Version: 0.1
  Variant: xiangqi
  License: free
  Description: MoonRiver Masters: xiangqi games between human players; rated online games with both players rated 2500 or more
  GameCount: 135030
  PlayerCount: 16272
  EventCount: 3402
  SiteCount: 90
  CommentCount: 1051

Tables (rows):
  Comments: 1051
  Events: 3402
  Games: 135030
  Info: 11
  Players: 16272
  Sites: 90

Columns of Games:
  ID INTEGER
  EventID INTEGER
  ...
  FEN TEXT
  Moves2 BLOB

Moves are read from: Moves2

Indexes:
  idx_comments_gameid (Comments)
```

Banksia Masters has one more table, `FidePlayers`: the FIDE rating-list entry of each player
(any extra table is kept and ignored by the programs).

## 4. Quick start

With the two Masters databases of §3:

```bash
# what is in it
ocgdb -info -db banksiamasters.ocgdb.db3

# games where three White queens stood on the board at some moment
ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 3"

# the same, only the first 10 games, and print them
ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 3" -resultcount 10 -o printpgn

# SQL: how the games ended
ocgdb -db banksiamasters.ocgdb.db3 -sql "SELECT Result, COUNT(*) FROM Games GROUP BY Result ORDER BY 2 DESC"

# all games of Bobby Fischer, to PGN
ocgdb -export -db banksiamasters.ocgdb.db3 -pgn fischer.pgn -where "w.Name = 'Fischer, Robert' OR b.Name = 'Fischer, Robert'"

# print game 12
ocgdb -db banksiamasters.ocgdb.db3 -g 12
```

Xiangqi is the same with `ocgdb-xq`:

```bash
ocgdb-xq -db moonrivermasters.ocgdb.db3 -q "line[h2e2]"            # games opening with the central cannon
ocgdb-xq -db moonrivermasters.ocgdb.db3 -q "mate and move[H, h]"   # mates by a horse move
ocgdb-xq -export -db moonrivermasters.ocgdb.db3 -pgn moonriver.pgn # all 135,030 games as PGN, 5 s
```

Your own PGN files make a database the same way:

```bash
ocgdb -create -pgn mygames.pgn -db mygames.ocgdb.db3
ocgdb-xq -create -pgn myxq.pgn -db myxq.ocgdb.db3
```

What these commands print, and how long they take, is shown in §7.
## 5. Concepts

### 5.1 A database

A database is one SQLite file. Its name should end with `.ocgdb.db3` (`banksiamasters.ocgdb.db3`,
`moonrivermasters.ocgdb.db3`); the programs warn when creating one that does not, and work anyway.
`:memory:` makes a database in memory, gone when the program ends (useful for timing).

A database holds the games of **one variant**, written in its Info table (`Variant`): `standard`,
`chess960`, `xiangqi` or `jieqi`. `ocgdb` opens only chess and chess960 databases, `ocgdb-xq`
only xiangqi and jieqi ones; each tells you to use the other when it is given the wrong kind.

### 5.2 Games, IDs and names

Every game has an ID (1, 2, 3...). When a database is created from PGN files, game *N* of the
first file gets the ID *N*, and the games of the next file follow on; a game that is skipped (a
filter, an error, a duplicate) leaves its ID unused. The same file always gives the same IDs,
however many threads ran.

Players, events and sites are kept once each in their own tables (`Players`, `Events`, `Sites`)
and the games refer to them by ID. Names are matched without regard to case and outer spaces, so
`"Carlsen, Magnus"` and `"carlsen, magnus "` are one player (the first spelling seen is kept).
An empty name, `?` or `*` is the empty name, ID 1.

### 5.3 Moves: the columns Moves, Moves1, Moves2

The moves of a game are stored in one or more of three columns, chosen when the database is
created (`-o moves`, `moves1`, `moves2`):

| Column | Type | What | Size | Variations | Comments |
|---|---|---|---|---|---|
| `Moves2` | BLOB | 2 bytes a move (§9.4) | the default | yes | in the table `Comments` |
| `Moves1` | BLOB | 1 byte for most moves, 2 for a few (§9.4) | the smallest | no (main line only) | main line, in `Comments` |
| `Moves` | TEXT | the move text of the PGN, as it was | the largest | yes, inside the text | inside the text |

`Moves2` is the default and the best choice: compact, fast to search, and it keeps variations.
`Moves1` makes the smallest database but drops variations and cannot store jieqi games. `Moves`
keeps the text exactly as written in the PGN (annotation symbols, everything); it can be combined
with `Moves2` (`-o moves,moves2`). When a database has more than one, the programs read `Moves2`,
then `Moves1`, then `Moves`.

### 5.4 Variations and comments

When a game is created with `Moves2`, its **variations** (the moves in brackets in a PGN) are
kept, nested ones too, and its **comments** go into the table `Comments`, each with the index of
the move it follows (the comment before the first move has the index -1). Exporting writes them
back in their places. `-o discardvariations` and `-o discardcomments` leave them out.

Annotation symbols (`!`, `?`, `!?`, `??`...) are read and dropped, as are NAGs (`$1`). The
column `Moves` keeps them, being the original text.

A variation that cannot be replayed (an annotator's typo: a move that is not legal there) is
dropped and the game read again without it, so the main line is never lost because of a side
line.

**PlyCount** is the number of half-moves of the **main line** — not the variations' moves.

### 5.5 Searching: main line only

PQL searches the **main line** of each game. Moves of variations are commentary, not part of the
game, and "ply", "the last move" and "then" would be wrong inside them (CQL does the same by
default).

### 5.6 Threads

The programs use one thread per core unless told otherwise (`-cpu n`). Creating, searching and
checking duplicates use all threads; exporting, printing games, SQL and info use one.

### 5.7 Encryption

A database can be encrypted with a password (`-password`). It is then unreadable without it, by
OCGDB and by any other program. The encryption is SQLCipher 4's (with SQLite3 Multiple Ciphers),
the same as Banksia GUI's, so the two programs open each other's encrypted databases; other
SQLCipher tools (DB Browser for SQLite with SQLCipher, the `sqlcipher` shell) open them too.
Reading also tries the older SQLCipher formats and the other schemes SQLite3 Multiple Ciphers
knows. **There is no way to recover a forgotten password.**

## 6. The command line

```
ocgdb <task> [<parameters>]
ocgdb-xq <task> [<parameters>]
```

The order of the parameters does not matter. A parameter that can be repeated (`-pgn`, `-db`,
`-q`, `-sql`, `-g`) adds one more each time. Only one task can be given.

### 6.1 Tasks

| Task | Does | Needs |
|---|---|---|
| `-create` | a new database from PGN files | one `-db`, one or more `-pgn` |
| `-merge` | adds games to an existing database | the database (first `-db`), and `-pgn` files and/or more `-db` |
| `-export` | a database to a PGN file | one `-db`, one `-pgn` |
| `-q <query>` | searches positions with PQL | `-db` and/or `-pgn` |
| `-sql <statement>` | runs SQL | one `-db` |
| `-g <id>` | prints games | one `-db` |
| `-dup` | finds (and removes) duplicate games | one or more `-db` |
| `-info` | describes a database | one or more `-db` |
| `-bench` | times a set of searches | `-db` |

### 6.2 Parameters

| Parameter | Meaning | Used by |
|---|---|---|
| `-pgn <file>` | a PGN file (`ocgdb-xq`: also WXF, any name not ending with `.pgn`) | create, merge, export (the output), q |
| `-db <file>` | a database | all |
| `-password <text>` | the password of encrypted databases; a new database is encrypted with it | all |
| `-variant <name>` | the variant of a new database or of searched PGN files: `standard` (default) or `chess960` for `ocgdb`; `xiangqi` (default) or `jieqi` for `ocgdb-xq` | create, q |
| `-o <opt>[,<opt>...]` | options, §6.3 | create, merge, q, g, dup |
| `-where <condition>` | an SQL condition choosing the games (§7.3, §7.4) | export, q, dup |
| `-elo <n>` | only games where both players have an Elo of at least *n* | create, merge, q (databases) |
| `-plycount <n>` | only games of at least *n* half-moves (main line) | create, merge, q (databases), dup |
| `-limit <n>` | read at most *n* games | create, merge, q, export |
| `-resultcount <n>` | stop a search after *n* games found; print at most *n* rows of SQL | q, sql |
| `-r <file>` | write the results to this file too | q, sql, g, dup, info |
| `-desc "<text>"` | a description, written in the Info table of a new database | create |
| `-cpu <n>` | the number of threads (default: all cores) | all |
| `-debug` | print all parameters, the SQL the tasks run, and the parsed query tree | all |
| `-help`, `-h` | the list of parameters | |
| `-version` | the version | |

### 6.3 Options (`-o`)

| Option | Meaning | Used by |
|---|---|---|
| `moves` | store the move text, column `Moves` | create |
| `moves1` | store the moves in 1 byte, column `Moves1` (not jieqi) | create |
| `moves2` | store the moves in 2 bytes, column `Moves2` (the default) | create |
| `acceptnewtags` | a new column in `Games` for each PGN tag the database has no column for | create, merge |
| `discardcomments` | do not store comments | create, merge |
| `discardvariations` | do not store variations | create, merge |
| `discardsites` | do not store the tag Site | create, merge |
| `discardnoelo` | skip games without the Elo of both players | create, merge |
| `discardfen` | skip games that start from a FEN | create, merge |
| `discarderr` | skip games with an error in their moves (by default the moves up to the error are kept) | create, merge |
| `reseteco` | compute the ECO code of every game, even those whose PGN has one | create, merge |
| `nodup` | skip a game whose moves are the same as those of a game already added | create, merge |
| `nobot` | Lichess: skip games played by bots (`WhiteTitle`/`BlackTitle` "BOT") | create, merge |
| `bot` | Lichess: only games played by bots | create, merge |
| `keepsite` | Lichess: the tag Site of a Lichess game is the game's URL; keep it in a column `Source` | create, merge |
| `printall` | list the duplicates | dup |
| `printfen` | print the FEN of the position found | q, g |
| `printpgn` | print the games found | q, dup |
| `embededgames` | also find games that are the start of a longer game | dup |
| `remove` | delete the duplicates | dup |

### 6.4 Exit code

0 when the task succeeded, 1 when it failed (a wrong parameter, a file that cannot be opened, a
wrong password, an SQL error, a query that does not parse). The messages go to the console
(errors to standard error).

## 7. The tasks

### 7.1 Create a database: `-create`

```bash
ocgdb -create -pgn big.pgn -db big.ocgdb.db3
ocgdb -create -pgn 2020.pgn -pgn 2021.pgn -db twoyears.ocgdb.db3 -elo 2200 -o moves,moves2,discardsites
ocgdb -create -variant chess960 -pgn frc.pgn -db frc.ocgdb.db3
ocgdb-xq -create -pgn xq-games.pgn -db xq.ocgdb.db3
ocgdb-xq -create -pgn cxq.wxf -db cxq.ocgdb.db3              # WXF text
ocgdb-xq -create -variant jieqi -pgn jieqi.pgn -db jieqi.ocgdb.db3
ocgdb -create -pgn lichess.pgn -db lichess.ocgdb.db3 -o nobot,keepsite
ocgdb -create -pgn private.pgn -db private.ocgdb.db3 -password "my secret"
```

An existing file of that name is **replaced**. The PGN files are read in blocks of 8 MB, and the
games of a block are added by all threads.

**Output:**

```
$ ocgdb-xq -create -pgn moonriver.pgn -db moonriver.ocgdb.db3
Open Chess Game Database Standard (OCGDB) ocgdb-xq 2.0.0 (xiangqi, jieqi)
Threads: 8
Creating moonriver.ocgdb.db3 (xiangqi)
Reading: moonriver.pgn
#games read: 135030, added: 135030, elapsed: 4464 ms 0:04, speed: 30248 games/s
Completed!
```

A long run prints a progress line every 5 seconds, with the part of the file read. At the end:
games read, added, skipped as duplicates (`nodup`), games with errors in their moves.

**What is stored from each game:**

| PGN | Database |
|---|---|
| `Event`, `Site`, `White`, `Black` | names in `Events`, `Sites`, `Players`; their IDs in `EventID`, `SiteID`, `WhiteID`, `BlackID` |
| `WhiteElo`, `BlackElo` | integers, only between 301 and 4999 (others are not Elos) |
| `Date` (or `UTCDate` when there is no Date) | `YYYY-MM-DD`; each `?` becomes `1` (`1950.??.??` → `1950-11-11`); two-digit years 31–99 are 1931–1999, 0–30 are 2000–2030; anything else is not a date and is not stored |
| `Result` | `1-0`, `0-1`, `1/2-1/2` (`0.5-0.5` too); anything else: no result (NULL) |
| `Round`, `TimeControl` | text |
| `ECO` | as given; computed from the moves when the game has none, or with `reseteco` |
| `PlyCount` | **counted from the moves** (the main line), not taken from the tag |
| `FEN` | the start position as the board writes it, NULL for the standard start (chess960: always written) |
| `SetUp`, `Variant` | not stored (the database has its variant) |
| other tags | dropped, unless `acceptnewtags` (a TEXT column per new tag, named after it) |
| the moves | `Moves2` (default), `Moves1`, `Moves` |
| comments | table `Comments` (with `Moves2`/`Moves1`) |

A game whose `[Variant]` names another variant than the database's is skipped (counted as an
error). Tags are read with the escapes of the PGN standard (`\"` and `\\` inside a value).

**Errors in moves.** A game with a move that is not legal keeps its moves up to that move and is
counted "with errors"; `discarderr` skips it instead. A game with an invalid FEN is skipped.

**Speed** (Apple M1 Pro, 8 threads; [testing.md](testing.md)): 3,456,399 chess games
(MillionBase, 2.1 GB of PGN) in 195 s; the 135,030 games of MoonRiver Masters (90 MB of PGN) in
4.5 s; heavily annotated games (20 comments a game) about 5,800 games/s.

### 7.2 Add games: `-merge`

```bash
ocgdb -merge -db main.ocgdb.db3 -pgn new.pgn
ocgdb -merge -db main.ocgdb.db3 -pgn new.pgn -o nodup
ocgdb -merge -db main.ocgdb.db3 -db other.ocgdb.db3 -db third.ocgdb.db3
```

The first `-db` is the database that receives the games; the PGN files and the other databases
are added to it. The games are stored as the receiving database stores games (its move columns
and its tag columns); the new games' IDs follow its largest ID. The names of players, events and
sites are matched with the ones already there. The other databases must be of the same variant;
they are read, game by game, with their variations and comments.

`nodup` skips the games that are copies of games added **in this run**; to clean the whole
database, run `-dup -o remove` afterwards (§7.7).

The Info table's counts are brought up to date at the end.

### 7.3 Export: `-export`

```bash
ocgdb -export -db banksiamasters.ocgdb.db3 -pgn masters.pgn
ocgdb -export -db banksiamasters.ocgdb.db3 -pgn fischer.pgn -where "w.Name = 'Fischer, Robert' OR b.Name = 'Fischer, Robert'"
ocgdb -export -db banksiamasters.ocgdb.db3 -pgn top.pgn -where "g.WhiteElo >= 2700 AND g.BlackElo >= 2700"
ocgdb-xq -export -db moonrivermasters.ocgdb.db3 -pgn moonriver.pgn
```

Writes the games in the order of their IDs, with their variations and comments. An existing file
is replaced. The PGN has:

- the Seven Tag Roster first, in its order (Event, Site, Date, Round, White, Black, Result; `?`
  for a missing one, `*` for no result), then the other columns of the game, by name (PlyCount,
  ECO, WhiteElo...), then `[Variant]` for chess960 and jieqi and `[FEN]`/`[SetUp "1"]` when the
  game does not start from the standard position;
- dates as `YYYY.MM.DD`; tag values with `"` and `\` escaped;
- the moves in SAN, 8 full moves a line, variations in brackets, comments in braces (a `{` or
  `}` inside a comment is written as `(` or `)`), the result at the end.

`-where` is an SQL condition on the games (§9.5); in it `g` is the table `Games`, `w` and `b`
the players (White, Black), `e` the event and `s` the site:

```
g.Date >= '2020-01-01'
g.ECO LIKE 'B9%'
g.Result = '1/2-1/2' AND g.PlyCount > 100
e.Name LIKE '%Olympiad%'
g.ID BETWEEN 1000 AND 1999
```

python-chess 1.11.2 reads the exported files without errors (tested on 19,739 annotated games,
[testing.md](testing.md) §3.2). The 135,030 games of MoonRiver Masters, exported and made into a
database again, come back with the same moves, results and start positions, all of them
([testing.md](testing.md) §2.1).

Speed: 15,000–17,000 games/s (one thread).

### 7.4 Search positions: `-q`

```bash
ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 3"
ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 3" -q "mate and move[N, n]"    # two searches
ocgdb -db a.ocgdb.db3 -db b.ocgdb.db3 -q "N[e5] then q = 0"                # two databases
ocgdb -pgn fischer.pgn -q "line[1. e4 c5 2. Nf3 d6 3. d4]"                 # a PGN file, no database
ocgdb -db banksiamasters.ocgdb.db3 -q "P[d4, e5] = 2 and p[d5, e6] = 2" -where "g.WhiteElo >= 2600" -o printpgn -r found.pgn
ocgdb -db banksiamasters.ocgdb.db3 -q "check" -resultcount 100
```

The query language is described in §8. Each query is searched in each PGN file and each database;
a game is found when one of the positions of its main line matches (with `then`: when its
positions match the parts in order).

**Output.** A line per game found, sorted by ID — the game's ID (in a PGN file: its place in the
file, `game #N`) and the ply of the first position that matched:

```
$ ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 3" -resultcount 3 -o printfen
Open Chess Game Database Standard (OCGDB) ocgdb 2.0.0 (chess, chess960)
Threads: 8
Query: Q = 3
Searching banksiamasters.ocgdb.db3
Stopped: 3 results reached (-resultcount)
; Query: Q = 3
; Source: banksiamasters.ocgdb.db3
; Found: 3
1. ID 10342, ply 83, FEN: 2QQ1rk1/5pp1/8/4QP2/8/q6P/pp4P1/4R2K b - - 0 42
2. ID 350138, ply 71, FEN: Q4rk1/5p1p/4p1p1/4q1r1/8/5QnP/5QP1/RR4K1 b - - 0 36
3. ID 425938, ply 39, FEN: r1b1kbQQ/2qp1p2/2n1p3/8/3N1P2/8/2P1B1P1/qqBQ1R1K b q - 0 20
Completed!
```

Searched to the end (no `-resultcount`), the 8,008,264 games of Banksia Masters give 396 games
in 65 s; a progress line is printed every 5 seconds.

- `-o printfen` adds the FEN of the matching position (as above);
- `-o printpgn` adds the whole game, as PGN (from a database: with variations and comments;
  from a PGN file: the game as it is in the file);
- `-r <file>` writes the same into a file — with `printpgn`, a PGN file of the games found (the
  lines starting with `;` are PGN comments).

With millions of results the list is long: use `-resultcount n` to stop after *n* games, or
`-r` to write it into a file.

**Choosing the games first.** In databases, `-where` (as for export, §7.3), `-elo` and
`-plycount` limit the search to the games that pass them, which is faster than searching
everything (they are not applied to PGN files): "this structure in
games of 2500+ players since 2015" is
`-q "..." -where "g.WhiteElo >= 2500 AND g.BlackElo >= 2500 AND g.Date >= '2015'"`.

**PGN files** are searched without making a database: `-pgn file.pgn -q "..."`. Use `-variant`
for chess960 or jieqi files. Games whose `[Variant]` names another variant are skipped.
(`ocgdb-xq` searches PGN files only, not WXF files: make a database of those first.)

Speed: about 125,000 games/s with eight threads (Banksia Masters, 8 million games, in 64 s),
slower for queries on moves (`mate and move[N, n]`: 82,000 games/s). [testing.md](testing.md) §2.2.

### 7.5 SQL: `-sql`

```bash
ocgdb -db banksiamasters.ocgdb.db3 -sql "SELECT COUNT(*) FROM Games"
ocgdb -db banksiamasters.ocgdb.db3 -sql "SELECT Result, COUNT(*) FROM Games GROUP BY Result"
ocgdb -db big.ocgdb.db3 -sql "UPDATE Players SET Name = 'Carlsen, Magnus' WHERE Name = 'Carlsen, M.'"
ocgdb -db big.ocgdb.db3 -sql "CREATE INDEX IF NOT EXISTS idx_games_white ON Games(WhiteID)" -sql "ANALYZE"
```

Runs the statements in order and prints what each returns: a header line with the column names,
then one line per row, the values separated by `|`, then the number of rows. A BLOB (a move
column) is printed as its size; NULL as `NULL`.

```
$ ocgdb -db banksiamasters.ocgdb.db3 -sql "SELECT g.ID, w.Name White, b.Name Black, g.Result FROM Games g JOIN Players w ON w.ID = g.WhiteID JOIN Players b ON b.ID = g.BlackID WHERE g.ID BETWEEN 8001800 AND 8001802"
Open Chess Game Database Standard (OCGDB) ocgdb 2.0.0 (chess, chess960)
SQL: SELECT g.ID, w.Name White, b.Name Black, g.Result FROM Games g JOIN Players w ON w.ID = g.WhiteID JOIN Players b ON b.ID = g.BlackID WHERE g.ID BETWEEN 8001800 AND 8001802
ID|White|Black|Result
8001800|Spassky, Boris V.|Fischer, Robert|0-1
8001801|Praveen Mahadeo Thipsay|Prasad, Devaki V|0-1
8001802|Sarno, Spartaco|Braga, Fernando|0-1
; 3 rows
Completed!
```

A statement that returns no rows (`UPDATE`, `DELETE`, `CREATE`...) prints the number of rows
changed. When the statements changed the database, the counts of the Info table are recounted.
The database is opened for writing when it can be; there is no undo — make a copy first if
unsure. Several statements in one `-sql`, separated by `;`, are not supported: give one `-sql`
per statement.

The tables and columns are in §9; many examples in §9.6.

### 7.6 Print games: `-g`

```bash
ocgdb -db banksiamasters.ocgdb.db3 -g 12
ocgdb -db banksiamasters.ocgdb.db3 -g 12 -g 13 -g 2000 -r three.pgn
ocgdb -db banksiamasters.ocgdb.db3 -g 8001800 -o printfen
ocgdb-xq -db moonrivermasters.ocgdb.db3 -g 67
```

Prints each game as PGN (as export writes it), with a comment line `; ID: n` before it.
`printfen` adds the FEN of the last position. An ID that is not in the database gives an error
message and the others are printed.

```
$ ocgdb -db banksiamasters.ocgdb.db3 -g 8001800 -o printfen
Open Chess Game Database Standard (OCGDB) ocgdb 2.0.0 (chess, chess960)
; ID: 8001800
[Event "World Championship"]
[Site "Reykjavik"]
[Date "1972.07.16"]
[Round "3"]
[White "Spassky, Boris V."]
[Black "Fischer, Robert"]
[Result "0-1"]
[ECO "A61-0"]
[PlyCount "92"]

1.d4 Nf6 2.c4 e6 3.Nf3 c5 4.d5 exd5
5.cxd5 d6 6.Nc3 g6 7.Nd2 Nbd7 8.e4 Bg7
...
45.Kd2 Qc2+ 46.Ke1 Qe2# 0-1
; FEN of the last position: 6k1/5p2/3p4/1p1P3p/2pQ2p1/3b2P1/4qP1P/2B1K3 w - - 8 46
```

Xiangqi games are written in SAN on the a0–i9 board (game 67 of MoonRiver Masters; in SAN the
horse is `N` and the elephant `B`, while PQL calls them `H` and `E`):

```
1.Che2 Ng7 2.Ng2 Rh9 3.Rh0 g5 4.c4 Ch3
5.Nc2 Bce7 6.Ni1 Ch2 7.Rxh2 Rxh2 8.Cxe6+ Nxe6
```

### 7.7 Duplicate games: `-dup`

```bash
ocgdb -db big.ocgdb.db3 -dup                                # count them
ocgdb -db big.ocgdb.db3 -dup -o printall -r dups.txt        # list them
ocgdb -db big.ocgdb.db3 -dup -o printall,printpgn           # list them with the games
ocgdb -db big.ocgdb.db3 -dup -o remove                      # delete them
ocgdb -db big.ocgdb.db3 -dup -o embededgames,printall       # also games that are the start of others
```

Two games are **duplicates** when they start from the same position and have the same moves in
their main lines — whatever their tags, comments and variations. Of each group of copies, the
game with the **smallest ID** (the first in the database) is kept and the others are
duplicates. With `embededgames`, a game that is the start of a longer game (the same first moves,
fewer of them) is a duplicate too, and the longer game is kept.

```
Finding duplicate games: games.ocgdb.db3
>>>>> Duplicate: 5 of 1
>>>>> Duplicate: 6 of 1
#games checked: 8, elapsed: 2 ms, speed: 4000 games/s, duplicates: 2, removed: 0
```

`remove` deletes the duplicates and their comments in one transaction, recounts the Info table,
and drops Banksia GUI's opening tree (the table `Tree`), whose counts included them — Banksia
GUI offers to make it again. `-where` and `-plycount` limit the games checked.

Games with the same moves but different players are duplicates for OCGDB; a collection may
want to keep such games (in xiangqi, for example, short games with the same moves are often
played by different players). Look at the list (`printall`) before removing.

Speed: 40,000–200,000 games/s (more copies, more comparisons).

### 7.8 Describe a database: `-info`

```bash
ocgdb -info -db banksiamasters.ocgdb.db3
```

Prints the file size, the Info table, every table with its number of rows, the columns of
`Games`, the move column read, and the indexes. Use it to see the variant of a database, whether
it has comments or an index, or what Banksia GUI added to it.

### 7.9 Benchmark: `-bench`

```bash
ocgdb -bench -db banksiamasters.ocgdb.db3
```

Runs six searches over the whole database and prints the count and speed of each (no list of
games). The queries (chess): `Q = 3`, `r[e4, e5, d4, d5] = 2`, `P[d4, e5, f4, g4] = 4 and kb7`,
`B[c-f] + b[c-f] == 2`, `white6 = 5`, `mate and move[N, n]`. Xiangqi: `R = 2 and C = 2`,
`c[e] = 2`, `H[c-g] >= 2 and h[c-g] >= 2`, `P[4] = 5`, `mate and move[H, h]`.

## 8. PQL, the Position Query Language

### 8.1 The idea

A query is a condition on **one position**; a game matches when **any position of its main line**
matches. The position is the board, plus the move that led to it and the game so far.

Piece letters count pieces: `Q` is the number of White queens, `p` the number of Black pawns.
Everything is a number, and comparisons and logic turn numbers into true/false:

```
Q = 3                           three White queens
R + r >= 3 and Q + q = 0        at least three rooks and no queens
P[d4, e5] = 2 and p[d5, e6] = 2 a French structure
mate and move[N, n]             checkmate by a knight move
N[e5] then q = 0                a White knight on e5, and later Black has no queen
```

Upper case is White (xiangqi: Red), lower case Black. Spaces do not matter. Everything after `//`
to the end of the line is a comment.

### 8.2 Pieces and squares

| Chess | Piece |   | Xiangqi | Piece |
|---|---|---|---|---|
| `K` `k` | king | | `K` `k` | general (king) |
| `Q` `q` | queen | | `A` `a` | advisor |
| `R` `r` | rook | | `E` `e` | elephant |
| `B` `b` | bishop | | `R` `r` | chariot (rook) |
| `N` `n` | knight | | `C` `c` | cannon |
| `P` `p` | pawn | | `H` `h` | horse |
| `white` `black` | all pieces of a side | | `P` `p` | soldier (pawn) |
| | | | `white` `black` | all pieces of a side (Red, Black) |

A piece alone counts the pieces on the whole board. Following it with squares counts only those
on the squares:

| Written | Counts | Example | Meaning |
|---|---|---|---|
| `X` + square | one square | `qb3` | Black queens on b3 (0 or 1) |
| `X` + file | a file | `bb` | Black bishops on the b file |
| `X` + rank | a rank | `B3` | White bishops on the 3rd rank |
| `X[...]` | a list of squares, files, ranks and ranges | `P[a4, c5, d5]` | White pawns on a4, c5, d5 |
| | file range | `n[b-e]` | Black knights on files b to e |
| | rank range | `q[5-7]` | Black queens on ranks 5 to 7 |
| | square range | `p[a2-d2]` | Black pawns on a2, b2, c2, d2 |
| | a mix | `p[b, 5, a2-d2]` | Black pawns on the b file, the 5th rank, or a2–d2 |
| `white`/`black` + squares | all pieces of a side there | `white6`, `black[a-c]` | |

Chess squares are a1–h8 (White's first rank is 1). Xiangqi squares are **a0–i9**: files a–i from
Red's left, ranks 0–9 from Red's back rank (Red's general starts on e0, Black's on e9).

A letter followed by a digit is a square (`b3`), so `qb3` reads as "queens on b3". In chess,
`bb` is "Black bishops on the b file"; `b[b]` says the same more clearly.

### 8.3 Numbers and conditions

| Operators | Meaning |
|---|---|
| `*` `/` | multiply, divide (integers) — strongest |
| `+` `-` | add, subtract |
| `=` `==` `!=` `<>` `<` `<=` `>` `>=` | comparisons: 1 if true, 0 if false |
| `not` `!` | not |
| `and` `&&` | and |
| `or` `\|\|` | or — weakest |
| `then` | sequence (§8.6) — weakest of all, between whole conditions |
| `(` `)` | brackets, around numbers or conditions |

Operators of one level work left to right: `8 - 2 - 1` is 5, `A and B or C` is
`(A and B) or C`. A number alone is a condition: true when it is not 0, so `R` means "there is a
White rook" (`R != 0`) and `R == 3` "exactly three". `and` and `or` stop as soon as the answer is
known.

```
3 = Q                         the same as Q = 3
B[c-f] + b[c-f] = 2           two bishops on the files c to f, of either side
black - white >= 6            Black has six or more pieces more than White
not (Q + q > 0) and R + r >= 3
(Q = 2 or R = 2) and P = 8
```

### 8.4 Exact positions

```
fen[r2qkb1r/1p2nppp/p3p3/4Pb2/2BB4/P7/1P3PPP/RN1Q1RK1 b kq - 0 12]
fen[<fen>, <fen>, ...]                  any of these positions
rnbqkbnr/pp2pppp/2p5/3pP3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3      a FEN alone
```

A position is matched exactly: pieces, side to move and castling rights (it is compared by its
hash key; the move counters do not matter). A FEN alone ends after its six fields, so it can be
combined: `<fen> and q = 1`.

### 8.5 Patterns

```
{K[g1], P[f2, g2, h2], <}           White king on g1 with pawns f2, g2, h2, anything else anywhere
{P[e2, f3, g4], K[f2], #, 0}        that formation anywhere on the board (shifted)
{<fen>, =, 2}                       the FEN's position, with at most 2 squares different
{R[d2], R[d3], #}                   two White rooks one above the other, anywhere
```

A pattern is a board made of piece lists (`Piece[squares]`) and FENs — the later ones win when
they disagree — then an operator and a tolerance:

| Operator | The position matches when |
|---|---|
| `=` (`==`) | it is the pattern |
| `<` | the pattern is part of it (the position may have more) |
| `>` | it is part of the pattern (the pattern may have more) |
| `#` | the pattern, moved by any number of files and ranks, is part of it (chess only) |

The tolerance (default 0) is the number of squares allowed to differ. Without an operator, `=`.

### 8.6 Moves and sequences

The position was reached by a move; these look at it and at the game so far, and can be used
anywhere a number can:

| Word | Value |
|---|---|
| `ply` | the number of half-moves played to reach the position (0 at the start; for a game from a FEN, counted from the FEN's move number) |
| `wtm`, `btm` | 1 when White (Black) is to move |
| `check` | 1 when the side to move is in check |
| `mate` | 1 when the side to move is checkmated (in check, no legal move) |
| `capture` | 1 when the last move captured (en passant too) |
| `promotion` | 1 when the last move promoted |
| `move[p1, p2, ...]` | 1 when the last move matches one of the move patterns |
| `line[moves]` | 1 on the position reached by exactly these moves from the standard start |
| `A then B` | the game has a position matching A and a **later** one matching B (any number of `then`) |

A **move pattern** is written like a SAN move and every part may be left out: piece letter
(its case is the side), from-square or from-file, `x` for a capture, to-square, promotion
(`=Q` or `Q`); or `O-O`, `O-O-O`. With no piece letter, any piece of either side.

```
move[N]            any White knight move
move[Nf3]          a White knight goes to f3
move[Ng1f3]        the knight from g1 to f3
move[Nbd2]         a White knight from the b file to d2
move[x]            any capture
move[Pxd5, pxd5]   a pawn of either side takes on d5
move[e4]           any move to e4
move[P=Q]          a White pawn promotes to a queen (also Pe8Q)
move[O-O, O-O-O]   either side castles (chess960 too)
```

`line[...]` takes SAN or coordinate moves, with or without move numbers:
`line[1. e4 e5 2. Nf3 Nc6 3. Bb5]`, `line[e2e4 e7e5]`; xiangqi: `line[h2e2 h9g7]`. A move that
cannot be played is an error when the query is read.

```
mate and move[N, n]                checkmate by a knight move
move[P=N, p=n]                     under-promotion to a knight
move[O-O-O] and ply < 20           queenside castling before move 10
check and capture                  a capture that gives check
N[e5] then q = 0                   a knight on e5, later no Black queen
line[1. d4 Nf6 2. c4 e6 3. Nc3 Bb4] then move[Bxc3]   the Nimzo-Indian, and Black later takes on c3
```

### 8.7 Xiangqi examples

```
R = 2 and C = 2 and ply = 0          every game (the start position)
line[h2e2]                           games opening with the central cannon (h2-e2)
c[e] = 2                             both Black cannons on the e file
H[c-g] >= 2 and h[c-g] >= 2          two horses of each side in the middle files
P[4] = 5                             all five Red soldiers on rank 4
mate and move[H, h]                  mate by a horse move
H[e5] then r = 0                     a Red horse on e5, later Black has no chariot
```

Jieqi: a covered piece has no letter of its own in PQL — it counts in `white` and `black` only
(at the start position `white = 16`, and `R` is 0 until a chariot is revealed); once revealed, it
counts as its type.

### 8.8 The grammar

```
query       = clause { "then" clause }
clause      = and_clause { ("or" | "||") and_clause }
and_clause  = not_clause { ("and" | "&&") not_clause }
not_clause  = ("not" | "!") not_clause | condition
condition   = expression { ( "=" | "==" | "!=" | "<>" | "<" | "<=" | ">" | ">=" ) expression }
expression  = term { ( "+" | "-" ) term }
term        = factor { ( "*" | "/" ) factor }
factor      = number | piece | fenclause | fenstring | pattern | keyword | move | line | "(" clause ")"
keyword     = "ply" | "check" | "mate" | "capture" | "promotion" | "wtm" | "btm"
move        = "move" "[" movespec { "," movespec } "]"
movespec    = "O-O" | "O-O-O" | [piecename] [square | column] ["x"] [square] ["=" piecename | piecename]
line        = "line" "[" moves in SAN or coordinates, move numbers allowed "]"
piece       = piecename [ square | column | row | squareset ]
piecename   = "K" | "Q" | "R" | "B" | "N" | "P" | "k" | "q" | "r" | "b" | "n" | "p" | "white" | "black"
              (xiangqi: "K" | "A" | "E" | "R" | "C" | "H" | "P", lower case for Black, "white", "black")
squareset   = "[" item { "," item } "]"
item        = square | square "-" square | column | column "-" column | row | row "-" row
square      = column row
column      = "a" ... "h"           (xiangqi: "a" ... "i")
row         = "1" ... "8"           (xiangqi: "0" ... "9")
fenclause   = "fen" "[" fenstring { "," fenstring } "]"
pattern     = "{" pattern_term { "," pattern_term } [ "," operand [ "," tolerance ] ] "}"
pattern_term = fenstring | piecename "[" square { "," square } "]"
operand     = "=" | "==" | "<" | ">" | "#"
tolerance   = number
```

### 8.9 Errors

A query that cannot be read is not searched; the message names the word where the problem is:

```
ocgdb -db banksiamasters.ocgdb.db3 -q "Q = 1 R = 7"     Error: unexpected near "R"
ocgdb -db banksiamasters.ocgdb.db3 -q "(Q"              Error: missing close bracket near "the end"
ocgdb -db banksiamasters.ocgdb.db3 -q "move[o-o]"       Error: wrong move near "o-o"
```

`-debug` prints the tree the query was read into.

### 8.10 PQL and CQL

CQL (the Chess Query Language, <https://www.gadycosteff.com/cql/>) describes far more —
attacks, pins, symmetry, regular expressions over lines — and reads PGN files only, chess only.
PQL is quick to learn, reads OCGDB databases directly, and handles xiangqi. For hard tactical
queries, export the games (`-export`, perhaps with `-where`) and run CQL on the PGN (give CQL
the games without comments if it stops on one).

## 9. SQL and the database format

### 9.1 Tables

```sql
CREATE TABLE Info (Name TEXT UNIQUE NOT NULL, Value TEXT);
CREATE TABLE Events (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE);
CREATE TABLE Sites (ID INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT UNIQUE);
CREATE TABLE Players (ID INTEGER PRIMARY KEY, Name TEXT UNIQUE, Elo INTEGER);
CREATE TABLE Games (ID INTEGER PRIMARY KEY AUTOINCREMENT,
    EventID INTEGER, SiteID INTEGER, Date TEXT, Round TEXT,
    WhiteID INTEGER, WhiteElo INTEGER, BlackID INTEGER, BlackElo INTEGER,
    Result TEXT, TimeControl TEXT, ECO TEXT, PlyCount INTEGER, FEN TEXT,
    Moves2 BLOB DEFAULT NULL,                      -- and/or Moves1 BLOB, Moves TEXT
    FOREIGN KEY(EventID) REFERENCES Events, FOREIGN KEY(SiteID) REFERENCES Sites,
    FOREIGN KEY(WhiteID) REFERENCES Players, FOREIGN KEY(BlackID) REFERENCES Players);
CREATE TABLE Comments (ID INTEGER PRIMARY KEY AUTOINCREMENT, GameID INTEGER, Ply INTEGER, Comment TEXT);
CREATE INDEX idx_comments_gameid ON Comments(GameID);
```

That is what `-create` makes with the default options. With `acceptnewtags` `Games` gets a TEXT
column per new tag; with `moves`, `moves1` the move columns differ. Databases made by other
programs may have other columns: the programs read whatever `Games` has, and need an ID and at
least one move column.

| Table | Row 1 |
|---|---|
| `Events`, `Sites`, `Players` | the empty name (`''`): games without that tag point to it |

`Players.Elo` is the Elo given with the player's first game.

### 9.2 The Info table

| Name | Value |
|---|---|
| `Data Structure Version` | `0.6` (the structure; the same as Banksia GUI's) |
| `Type` | `OCGDB` |
| `Version` | `0.1` (the version of the user's data) |
| `Variant` | `standard`, `chess960`, `xiangqi` or `jieqi` (a database without it is the program's own: standard or xiangqi) |
| `License` | `free` |
| `Description` | `-desc` |
| `Generator` | `ocgdb 2.0.0` — the program that made it |
| `GameCount`, `PlayerCount`, `EventCount`, `SiteCount`, `CommentCount` | the numbers of rows, kept up to date by the programs (`SELECT COUNT(*)` on a huge table is slow) |
| `TreeNodeCount` | Banksia GUI's opening tree (§10) |

### 9.3 Dates, results, ECO

- `Date`: `YYYY-MM-DD` (ISO 8601), so dates sort and compare as text:
  `g.Date >= '2020'`, `substr(g.Date, 1, 4) = '1972'`.
- `Result`: `1-0`, `0-1`, `1/2-1/2`, or NULL.
- `ECO`: as in the PGN, or computed. Databases made by Banksia GUI may have codes with a
  sub-index (`C88-1`): the opening classification of Banksia GUI's ECO tables.
- `FEN`: NULL for the standard start position.

### 9.4 The move codes

**Moves2 — chess.** A little-endian 16-bit number per move:
`from | dest << 6 | promotion << 12`, where the squares are numbered as Stockfish numbers them
(a1 = 0, b1 = 1, ..., h8 = 63) and the promotion is 0 for none, 2 knight, 3 bishop, 4 rook,
5 queen. Castling is the king's move (e1–g1); in chess960 the king's move onto its rook.

**Moves2 — xiangqi.** `from | dest << 7`, squares numbered from a9 (0) to i0 (89) rank by rank
(a9 = 0, b9 = 1, ..., i9 = 8, a8 = 9, ..., i0 = 89). **Jieqi**: a move that reveals a covered piece
is `0x8000 | index | type << 4 | dest << 7`, where *index* is the number of the covered piece
(0–15, its place among its side's pieces in board order at the start) and *type* what it turned
out to be (2 advisor ... 7 soldier); other moves as in xiangqi. The reveal fits in 2 bytes (15
bits: a captured covered piece is never revealed, so a move carries at most one type); it is
decoded on the board of its move, which knows the covered piece's square.

**Variations in Moves2.** A variation is written between two marks that are not moves, the codes
of a move from a square to the same square: from = dest = 1 opens a variation one level deeper,
from = dest = 2 goes back one level. In chess (Stockfish numbering of the board's squares 1 and
2, b8 and c8) those are `0x0E79` and `0x0EBA`. The moves after an opening mark are the
alternatives to the move before it; two variations at the same level, one after the other, are
written as "back, deeper".

**Moves1.** Most moves take one byte: the low 4 bits are the moving piece's index (0–15: the
pieces of the side in board order at the start position, kept through the game), the high 4 bits
the move among that piece's possible ones (chess: the king's and knights' 8–10 steps, a pawn's 4,
a rook's file or rank and target, a bishop's diagonal and target). A queen's move (xiangqi: a
chariot's or cannon's) takes two bytes: index | dest << 4. Moves1 has no room for variation marks
and is decoded move by move on the board, so it holds the main line only.

**Comments.** `Comments.Ply` is the index of the move the comment follows, counting every move
of the Moves2 data from 0 — variation moves included, marks not — and -1 for the comment before
the first move. With Moves1 the index counts the main line.

**Moves.** The PGN move text as it was read, comments, variations and all.

Reading the codes takes a board (the programs, or a library): `-export` and `-g` turn them into
PGN.

### 9.5 The query the programs use

`-export`, `-g`, `-where` and `printpgn` read a game with the names of its players, event and
site joined in:

```sql
SELECT g.ID GID, e.Name Event, s.Name Site, w.Name White, b.Name Black, g.*
FROM Games g
LEFT JOIN Events e ON g.EventID = e.ID
LEFT JOIN Sites s ON g.SiteID = s.ID
LEFT JOIN Players w ON g.WhiteID = w.ID
LEFT JOIN Players b ON g.BlackID = b.ID
```

`-where <condition>` is added to it as `WHERE (<condition>)`, so the condition can use `g`, `e`,
`s`, `w`, `b` and every column of `Games`. With `-debug` the programs print the SQL they run.

### 9.6 Examples

```sql
-- games, players, events
SELECT (SELECT COUNT(*) FROM Games), (SELECT COUNT(*) FROM Players), (SELECT COUNT(*) FROM Events);

-- results
SELECT Result, COUNT(*), ROUND(100.0 * COUNT(*) / (SELECT COUNT(*) FROM Games), 1) AS pct
FROM Games GROUP BY Result ORDER BY 2 DESC;

-- the players with the most games
SELECT p.Name, COUNT(*) n FROM Games g JOIN Players p ON p.ID IN (g.WhiteID, g.BlackID)
GROUP BY p.ID ORDER BY n DESC LIMIT 20;

-- a player's score
SELECT SUM(CASE WHEN (w.Name LIKE 'Carlsen%' AND g.Result = '1-0') OR (b.Name LIKE 'Carlsen%' AND g.Result = '0-1') THEN 1.0
                WHEN g.Result = '1/2-1/2' THEN 0.5 ELSE 0 END) AS points, COUNT(*) AS games
FROM Games g JOIN Players w ON w.ID = g.WhiteID JOIN Players b ON b.ID = g.BlackID
WHERE w.Name LIKE 'Carlsen%' OR b.Name LIKE 'Carlsen%';

-- the openings played most
SELECT ECO, COUNT(*) FROM Games GROUP BY ECO ORDER BY 2 DESC LIMIT 20;

-- White's score by opening, at least 100 games
SELECT ECO, COUNT(*) n, ROUND(AVG(CASE Result WHEN '1-0' THEN 1.0 WHEN '0-1' THEN 0.0 ELSE 0.5 END), 3) score
FROM Games WHERE Result IS NOT NULL GROUP BY ECO HAVING n >= 100 ORDER BY score DESC;

-- games per year
SELECT substr(Date, 1, 4) AS year, COUNT(*) FROM Games GROUP BY year ORDER BY year;

-- the longest games
SELECT ID, PlyCount FROM Games ORDER BY PlyCount DESC LIMIT 10;

-- the events with the strongest average
SELECT e.Name, COUNT(*) n, AVG((g.WhiteElo + g.BlackElo) / 2) elo FROM Games g JOIN Events e ON e.ID = g.EventID
WHERE g.WhiteElo > 0 AND g.BlackElo > 0 GROUP BY e.ID HAVING n >= 20 ORDER BY elo DESC LIMIT 10;

-- the comments of game 12
SELECT Ply, Comment FROM Comments WHERE GameID = 12 ORDER BY Ply;

-- games starting from a position (some programs write '' instead of NULL for the start position)
SELECT COUNT(*) FROM Games WHERE IFNULL(FEN, '') <> '';

-- chess, Moves2: games opening 1. e4 (e2 = 12, e4 = 28: 12 | 28 << 6 = 0x070C)
SELECT COUNT(*) FROM Games WHERE IFNULL(FEN, '') = '' AND substr(Moves2, 1, 2) = x'0C07';

-- xiangqi, Moves2: games opening with the central cannon h2-e2 (h2 = 70, e2 = 67: 70 | 67 << 7 = 0x21C6);
-- MoonRiver Masters: 65,445, the same as PQL's line[h2e2]
SELECT COUNT(*) FROM Games WHERE IFNULL(FEN, '') = '' AND substr(Moves2, 1, 2) = x'C621';

-- size of the move data
SELECT SUM(length(Moves2)) FROM Games;
```

Indexes make the SQL of large databases fast; the programs create only the one on
`Comments(GameID)`. For queries by player, event or date:

```bash
ocgdb -db banksiamasters.ocgdb.db3 -sql "CREATE INDEX IF NOT EXISTS idx_games_white ON Games(WhiteID)" \
      -sql "CREATE INDEX IF NOT EXISTS idx_games_black ON Games(BlackID)" \
      -sql "CREATE INDEX IF NOT EXISTS idx_games_date ON Games(Date)" -sql "ANALYZE"
```

### 9.7 Changing data

Any SQL can change a database (`-sql "UPDATE ..."`, `DELETE`, `ALTER TABLE`). Keep in mind:

- deleting games: delete their comments too
  (`DELETE FROM Comments WHERE GameID NOT IN (SELECT ID FROM Games)`);
- the move columns are binary: do not edit them by hand;
- after changes made by other tools, `ocgdb -sql "SELECT 1"` does not recount — run any
  changing statement through `ocgdb -sql`, or `-merge`/`-dup`, to bring the Info counts up to
  date, or `UPDATE Info SET Value = (SELECT COUNT(*) FROM Games) WHERE Name = 'GameCount'`.

## 10. Working with Banksia GUI and MoonRiver

Banksia GUI (chess) and MoonRiver (xiangqi) read and write the same databases (their Database
Viewer, their database builder), and download the ready-made databases of §3 themselves
(*Tools › Download*).

- **Moving databases between them** needs nothing: open the file.
- **Encryption**: the same scheme; a password set in one opens in the other.
- **Their opening tree** is the table `Tree` (and the Info row `TreeNodeCount`). OCGDB does not
  read or make it; `-dup -o remove` drops it because its counts include the removed games.
- **Differences** in what they write: Banksia GUI writes dates without leading zeros
  (`2010-1-8`), ECO codes with its sub-index (`C88-1`) and counts variation moves in PlyCount;
  it may also write `''` instead of NULL for a game from the start position. OCGDB writes
  `2010-01-08`, the PGN's ECO, the main line's length and NULL. Both programs read both. Banksia
  Masters, made by Banksia GUI, has such dates and ECO codes (the first of Fischer's games in it:
  `[ECO "B97-1"]`); MoonRiver Masters writes `''` for the start position.
- Banksia GUI's PQL tab and `ocgdb -q` give the same results (tested, [testing.md](testing.md)
  §3.1).

## 11. Speed and size

Measured on an Apple M1 Pro (8 cores, 16 GB) with eight threads, the machine also busy with other
work. [testing.md](testing.md) has the details.

| Task | Data | Time | Speed |
|---|---|---:|---:|
| create | 3,456,399 chess games (MillionBase, 2.1 GB PGN) | 195 s | 17,700 games/s |
| create | 135,030 xiangqi games (MoonRiver Masters, 90 MB PGN) | 4.5 s | 30,200 games/s |
| create | 19,739 annotated games (416,455 comments) | 3.4 s | 5,800 games/s |
| export | 3,456,399 chess games | 225 s | 15,400 games/s |
| export | 135,030 xiangqi games | 5.4 s | 24,800 games/s |
| PQL search | 8,008,264 games (Banksia Masters), `Q = 3` | 65 s | 124,000 games/s |
| PQL search | the same, `mate and move[N, n]` | 98 s | 82,000 games/s |
| PQL search | 2,440,210 puzzles (Lichess Puzzles), `mate and move[N, n]` | 7.6 s | 320,000 games/s |
| PQL search | 135,030 xiangqi games, `mate and move[H, h]` | 2.3 s | 58,000 games/s |
| duplicates | 135,030 xiangqi games | 0.9 s | 153,000 games/s |
| duplicates | 39,478 games | 0.9 s | 43,000 games/s |

Size, with Moves2, names and tags included: the 3,456,399 MillionBase games make an 837 MB
database (about 240 bytes a game); Banksia Masters is 1.87 GB for 8,008,264 games (233 bytes a
game) and packs to 855 MB with 7-Zip; MoonRiver Masters is 31.3 MB for 135,030 games. Moves1 is
smaller still, `Moves` text larger.

Memory: about 400 MB for those runs (370 MB for the search of Banksia Masters); a search holds
only the games being searched by the threads.

## 12. Troubleshooting

| Message | Meaning | What to do |
|---|---|---|
| `... is a database of xiangqi; this program reads chess and chess960 databases (use ocgdb-xq ...)` | the wrong program for the variant | use the other program |
| `can't read ... -- not a database, or an encrypted one (give its password with -password)` | an encrypted database, or not a database | give `-password` |
| `wrong password, or not a database` | the password does not open it | check the password |
| `... is not a game database (it has no table Games)` | an EPD or opening book database of Banksia GUI, or another SQLite file | only game databases are supported |
| `has no column for moves` | `Games` has none of Moves, Moves1, Moves2 | |
| `Error: unexpected near "..."` and the like | the query cannot be read | §8.9 |
| `Moves1 (option moves1) cannot store jieqi games` | | use moves2 |
| create: `with errors: n` | *n* games had an illegal move (kept up to it) or an invalid FEN, or another variant | look at the games; `discarderr` skips them |
| create: `Warning: the database file's extension should be .ocgdb.db3` | | rename if you like |
| `Warning: a game longer than 4194304 bytes across a block boundary was dropped` | a single game of over 4 MB of text | split the file |

Games missing after create: check the filters (`-elo`, `-plycount`, `discardnoelo`,
`discardfen`, `nodup`, `nobot`/`bot`) and games of another variant.

A search finds nothing: check the case of the letters (White upper case), the squares (xiangqi
a0–i9), and that the database is of the variant you think (`-info`).

## 13. What is new in version 2

Version 2 has a new chess and xiangqi core, a new SQL layer and a new PQL. What a user notices,
compared with the Beta versions of 2022 (up to Beta 8):

- **Xiangqi and jieqi**: the program `ocgdb-xq`. Chess960 with `-variant chess960`.
- **PQL**: moves (`move[...]`, `capture`, `promotion`, `check`, `mate`, `ply`, `wtm`/`btm`),
  `line[...]`, `then`, `not`, brackets around conditions, correct precedence (`a - b - c`,
  `A and B or C`), patterns with `#` working, errors naming the word. The results list each game
  found with the ply where it matched.
- **Variations and comments** stored and exported (Moves2).
- **New tasks**: `-sql`, `-info`; `-merge` adds games of other databases; **new parameters**:
  `-password`, `-where`, `-variant`, `-limit`, `-debug`; options `discardvariations`,
  `discarderr`, `nodup`, `keepsite`.
- **SQLite 3.53.4** with encryption (SQLite3 Multiple Ciphers), instead of 3.36.0.
- **Game IDs** follow the order of the PGN files (game *N* of a file is ID *N*, however many
  threads ran).
- **PGN tags** are read and written with the escapes of the PGN standard (`\"`, `\\`).
- **Duplicates**: the copy kept is always the first; groups of copies are handled whole;
  `nodup` compares whole main lines (a 64-bit key of the start position and every move), so
  transpositions are not taken for copies.
- **PlyCount** is the length of the main line, counted from the moves; **dates** are written with
  two-digit month and day; **ECO** is computed only for a game that has none (or with
  `reseteco`).
- **Export** reads all comments in one pass when `Comments(GameID)` has no index; new databases
  get the index.
- **Faster create**: no SAN strings are made while creating; progress is printed every 5 seconds.
- **Removed**: the EPD database code (it was not reachable from the command line), the Xcode and
  Visual Studio projects.

Databases made by the Beta versions are read by version 2 (a database without the Info row `Variant` is
taken as standard chess). The Lichess Puzzles database of §3 is one: made with an early version,
read by version 2 as it is.
