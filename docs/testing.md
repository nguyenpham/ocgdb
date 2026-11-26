# Testing and verification

OCGDB 2.0.0. Machine for every number: Apple M1 Pro (8 cores), 16 GB, macOS. The machine was
also running other jobs during the measurements, so times are somewhat slower than on an idle
machine.

## 1. The regression tests

```bash
cd src && make -j8 && cd ..
tests/run-tests.sh
```

54 checks, about 10 s; the exit code is the number of failures. The programs are taken from
`src/` (or `$OCGDB`, `$OCGDB_XQ`); everything is written into a temporary folder. Run them also
against a debug build, which has the assertions on:

```bash
cd src && make -j8 debug && cd ..
OCGDB=src/ocgdb-debug OCGDB_XQ=src/ocgdb-xq-debug tests/run-tests.sh
```

Both must pass, 54 of 54.

The data are in `tests/data/` — small and made for the tests, no third-party games:

| File | Games | Made how | Covers |
|---|---|---|---|
| `chess.pgn` | 8 | by hand, checked with python-chess 1.11.2 | variations (nested, a variation starting with a Black move), comments with brackets, a comment before the first move, NAGs and `!?`, a tag value with `\"` and `\\`, a date with `??`, a game from a FEN with Black to move, en passant, castling both ways, promotions to all four pieces from a FEN, two mates, an exact copy of game 1 (other comments), the start of game 1 |
| `chess960.pgn` | 8 | random legal games with castling, python-chess, seed 7 | chess960 castling with the king or the rook already on its square |
| `xq.pgn` | 6 | random legal games, a generator linked with the core | xiangqi SAN round trip |
| `jieqi.pgn` | 6 | the same generator, jieqi, reveals drawn by `JieqiBoard` (seeds 1000–1005) | covered men, reveals, ambiguous covered moves |

What is checked: create (counts, Info, escaped tags, dates, PlyCount of the main line, the first
comment, IDs in file order), PQL (counts compared with python-chess: `mate` 2, `check` 3,
`move[P=N, p=n]` 1, `move[O-O-O]` 1, `line[1. e4 e5 2. Nf3 Nc6 3. Bb5]` 3; `then`; a parse error;
`-where`; a PGN file), export and import again (every Moves2 and every comment count the same),
64-byte blocks (a block boundary in every game: the same database), Moves1 and text moves giving
the same answers as Moves2, `-g` from Moves1, `-dup` (with and without `embededgames`),
`-merge` of a PGN file and of a database, `-dup -o remove` (keeps the first of each group),
`-create -o nodup`, encryption (refused without or with a wrong password, read with it),
chess960 (castling in every game, round trip), xiangqi and jieqi (export identical to the input,
Moves1 = Moves2, jieqi Moves1 refused), and each program refusing the other family's databases.

## 2. The ready-made databases

The databases of [user-guide.md](user-guide.md) §3, downloaded from their releases (SHA-256 as
the release notes give them) and unpacked into a scratch folder.

### 2.1 MoonRiver Masters (xiangqi, 135,030 games)

`moonrivermasters.ocgdb.db3` from [MoonRiverChineseChessHub](https://github.com/nguyenpham/MoonRiverChineseChessHub),
release `moonrivermasters-db`.

```bash
ocgdb-xq -export -db moonrivermasters.ocgdb.db3 -pgn mrm.pgn        # 5.4 s, 89.7 MB, 24,780 games/s
ocgdb-xq -create -pgn mrm.pgn -db rt.ocgdb.db3                      # 4.5 s, 30,248 games/s
ocgdb-xq -db rt.ocgdb.db3 -sql "ATTACH '<path>/moonrivermasters.ocgdb.db3' AS o" \
    -sql "SELECT COUNT(*), SUM(a.Moves2 IS b.Moves2), SUM(a.Result IS b.Result), SUM(a.PlyCount IS b.PlyCount),
                 SUM(IFNULL(a.FEN,'') = IFNULL(b.FEN,'')) FROM main.Games a JOIN o.Games b ON a.ID = b.ID"
```

The database made from the export equals the original game by game: 135,030 of 135,030 Moves2,
Result, PlyCount and FEN (the original writes `''` for the start position, OCGDB NULL).
`-dup` finds no duplicate (0.9 s, 153,000 games/s), as the release says.

`-bench`, and two more queries:

| Query | Found | Time | Speed |
|---|---:|---:|---:|
| `R = 2 and C = 2` | 134,992 | 0.3 s | 509,500 games/s |
| `c[e] = 2` | 5,454 | 1.9 s | 72,400 games/s |
| `H[c-g] >= 2 and h[c-g] >= 2` | 73,300 | 1.3 s | 101,500 games/s |
| `P[4] = 5` | 34 | 1.9 s | 70,400 games/s |
| `mate and move[H, h]` | 668 | 2.3 s | 58,300 games/s |
| `line[h2e2]` | 65,445 | 1.2 s | 116,400 games/s |

(`R = 2 and C = 2` is fast because nearly every game matches at its first position.) `line[h2e2]` was also counted with SQL on the raw
bytes (h2-e2 is the code `0x21C6`): `SELECT COUNT(*) FROM Games WHERE IFNULL(FEN,'') = '' AND
substr(Moves2,1,2) = x'C621'` gives 65,445, as PQL does.

### 2.2 Banksia Masters (chess, 8,008,264 games)

`banksiamasters.ocgdb.db3` (1.87 GB) from [BanksiaChessHub](https://github.com/nguyenpham/BanksiaChessHub),
release `banksiamasters-db`.

`-bench` (`/usr/bin/time -l`: 380 s in all, peak memory 367 MB):

| Query | Found | Time | Speed |
|---|---:|---:|---:|
| `Q = 3` | 396 | 64.6 s | 123,911 games/s |
| `r[e4, e5, d4, d5] = 2` | 18,880 | 64.5 s | 124,074 games/s |
| `P[d4, e5, f4, g4] = 4 and kb7` | 1,096 | 63.4 s | 126,235 games/s |
| `B[c-f] + b[c-f] == 2` | 7,799,407 | 25.0 s | 320,164 games/s |
| `white6 = 5` | 184 | 63.7 s | 125,734 games/s |
| `mate and move[N, n]` | 24,122 | 98.2 s | 81,548 games/s |

(`B[c-f] + b[c-f] == 2` is fast because nearly every game matches at its first position.)

Other tasks on it:

| Command | Result | Time |
|---|---|---:|
| `-sql "SELECT Result, COUNT(*) FROM Games GROUP BY Result"` | 3,213,469 / 2,621,694 / 2,172,554 / 547 (1-0, 0-1, draw, none) | < 1 s |
| `-sql` Fischer's score (`WHERE 13196 IN (g.WhiteID, g.BlackID)`, player ID from `Players`) | 395 games, 224.5 points | 0.9 s |
| `-export ... -where "w.Name = 'Fischer, Robert' OR b.Name = 'Fischer, Robert'"` | 395 games (395 `[Event` tags in the file) | 9.5 s |
| `-g 8001800 -o printfen` | Spassky–Fischer, Reykjavik 1972, game 3 | < 1 s |

The export by name takes 9.5 s because `-where` on a name scans all 8 million games; an index
on `Games(WhiteID)` and `Games(BlackID)` (user guide §9.6) makes such queries fast.

### 2.3 Lichess Puzzles (chess, 2,440,210 puzzles)

`lichesspuzzles2-4M.ocgdb.db3` from BanksiaChessHub, release `lichess-puzzles`, made by an early
OCGDB (no Info row `Type`) and read by version 2 as it is. Every game starts from its FEN; its
first comment holds the rating and the themes (`{Rating: 1852 crushing hangingPiece long
middlegame}`).

| Query | Found | Time | Speed |
|---|---:|---:|---:|
| `mate and move[N, n]` | 25,639 | 7.6 s | 320,000 games/s |
| `move[P=N, p=n] and mate` | 0 | 5.4 s | 455,000 games/s |
| SQL: `SELECT COUNT(*) FROM Comments WHERE Comment LIKE '% smotheredMate%'` | 5,327 | | |

## 3. Other databases

### 3.1 PQL counts on 19,739 annotated games

A database of 19,739 heavily annotated chess games (416,575 comments, variations, Moves2). The
expected counts were found independently with CQL 6.2 and python-chess, and Banksia GUI's PQL
finds the same.

| Query | Expected | ocgdb 2.0.0 | time |
|---|---:|---:|---:|
| `q = 3` | 3 | 3 | 0.15 s |
| `black[a-c] > 8` | 14 | 14 | 0.19 s |
| `check` | 17,965 | 17,965 | 0.12 s |
| `mate and move[N, n]` | 363 | 363 | 0.25 s |
| `move[P=N, p=n]` | 45 | 45 | 0.12 s |
| `N[e5] then q = 0` | 2,943 | 2,943 | 0.14 s |
| `line[1. e4 e5 2. Nf3 Nc6 3. Bb5]` | 1,228 | 1,228 | 0.14 s |
| `move[O-O-O] and ply < 20` | 1,118 | 1,118 | 0.13 s |
| `ply <= 40 and Q = 0 and q = 1` | 2,309 | 2,309 | 0.12 s |
| `{R[d2], R[d3], #}` | 1,676 | 1,676 | |
| `{R[d2], R[d3], <}` | 46 | 46 | |
| `{K[g1], P[f2, g2, h2], <}` | 8,290 | 8,290 | |
| `P = 8` | 19,593 | 19,593 | |

All equal. Speed: 80,000–160,000 games/s on this database.

The same queries on the database made again from the export (§3.2) as Moves2, as Moves1, as text
moves, and on the exported PGN file itself give the same counts.

### 3.2 Round trip of the annotated database

```bash
ocgdb -export -db annotated.ocgdb.db3 -pgn annotated.pgn          # 1.7 s, 53.8 MB
ocgdb -create -pgn annotated.pgn -db rt.ocgdb.db3                 # 3.4 s
```

Compared with SQL (`ATTACH`), game by game:

| | equal | differs | why |
|---|---:|---:|---|
| Moves2 | 19,693 | 46 | the 46 games with a variation that cannot be replayed: the core drops such a variation and reads the game again; the main lines of all 46, decoded from the bytes of both databases, are equal |
| comments | 416,455 | 120 | exactly the comments of those dropped variations |
| Result, ECO | 19,739 | 0 | |
| FEN | 19,737 | 2 | the original has the invalid full-move number `0` (`... w - - 0 0`); the core writes `1` |
| White and Event names | 19,739 | 0 | including the event `092. Wch18 Nld\urss`, whose `\` is written as `\\` and read back |
| Date | 19,514 | 225 | the original (made by Banksia GUI) writes `2010-1-8`, OCGDB `2010-01-08` (ISO 8601, sortable) |
| PlyCount | 12,711 | 7,028 | the original counts the variations' moves too; OCGDB counts the main line |

python-chess 1.11.2 reads all 19,739 exported games with 0 errors; the length of every main
line equals the database's PlyCount; 50,197 variations. Bracket balance: 0 games unbalanced.

The same database built with `OCGDB_PGN_BLOCK=4096` (a block boundary every few games) is
identical: 19,739 of 19,739 Moves2, 416,455 comments.

### 3.3 MillionBase, 3.46 million games

`mb-3.45.ocgdb.db3` (Ed Schröder's MillionBase 3.45).

| Step | Games | Time | Speed | Peak memory |
|---|---:|---:|---:|---:|
| `-export` to PGN (2,125,242,949 bytes) | 3,456,399 | 225 s | 15,371 games/s | 359 MB |
| `-create` from that PGN (837 MB database) | 3,456,399 | 195 s | 17,682 games/s | 398 MB |

The created database against the original: all 3,456,399 Moves2 equal (5,613 games have no
moves; both store NULL), every Result and FEN equal; PlyCount equal except those 5,613 empty
games (original NULL, OCGDB 0).

`-bench` (5 queries and `mate and move[N, n]`):

| Query | Found | Time | Speed |
|---|---:|---:|---:|
| `Q = 3` | 57 | 34.4 s | 100,354 games/s |
| `r[e4, e5, d4, d5] = 2` | 8,212 | 34.9 s | 98,929 games/s |
| `P[d4, e5, f4, g4] = 4 and kb7` | 483 | 34.3 s | 100,837 games/s |
| `B[c-f] + b[c-f] == 2` | 3,328,418 | 15.3 s | 225,451 games/s |
| `white6 = 5` | 57 | 34.3 s | 100,913 games/s |
| `mate and move[N, n]` | 7,705 | 50.2 s | 68,842 games/s |

### 3.4 Xiangqi, 153,600 games

A PGN collection of 153,600 xiangqi games:

| Step | Time | Speed |
|---|---:|---:|
| `-create` (Moves2) | 5.2 s | 29,400 games/s |
| `-export` | 9.5 s | 16,245 games/s |

The database made from the export is identical: 153,600 of 153,600 Moves2, Result, Date, FEN
and ECO. Chinese event names come back intact.

PQL on Moves2, Moves1 and the PGN file — the same counts everywhere:

| Query | Found |
|---|---:|
| `R = 2 and C = 2` | 153,600 |
| `line[h2e2]` | 73,971 |
| `mate and move[H, h]` | 926 |
| `check` | 125,462 |
| `P[4] = 5` | 37 |
| `H[e5] then r = 0` | 8,890 |

`-dup` finds 4,395 pairs with the same moves. They are all under 80 plies and between different
players: the collection keeps such games on purpose, so this is expected.

### 3.5 Chess960

300 random legal chess960 games (python-chess, seed 7, 296 castlings): created with
`-variant chess960`, exported, read by python-chess — 300 of 300 games identical (moves and
start positions), 0 errors. `move[O-O, O-O-O]` finds 211 games, python-chess counts 211 games
with a castling.

### 3.6 Duplicates

On the annotated database merged with its own export (39,478 games): `-dup` finds 20,241
duplicates in every run (19,739 copies + 502 duplicates inside the original), whatever the number
of threads; `-o remove` leaves 19,237 games and a second `-dup` finds none.

`-create -o nodup` from the annotated export twice: 19,237 added, 20,241 skipped, the same as
`-dup`.

### 3.7 Encryption

A database created with `-password 'p@ss w0rd'`: without a password and with a wrong one the
programs refuse it; Python's `sqlite3` says "file is not a database"; with the password `-info`
and `-q` read it. It uses the same library, version and options (SQLCipher 4 scheme) as Banksia
GUI and MoonRiver.

## 4. Debug builds

The annotated database through `ocgdb-debug` (assertions on): create, export, `N[e5] then q = 0`
(2,943), `-dup -o embededgames`; 20,000 xiangqi games through `ocgdb-xq-debug`: no assertion
fails.

`ocgdb-xq -create -o moves1` of `tests/data/xq.pgn`, 60 times with each build: all clean (the
Moves1 encoder is safe with several threads).
