# The chess and xiangqi core

This page is for developers: how the board code under `src/base/`, `src/chess/` and `src/xq/` is
organised, and what it does for a game database. Users do not need it; the user guide is
[user-guide.md](user-guide.md).

The core is the board code of the Banksia programs (the same lineage as the Beta versions' `src/board/`,
namespace `bslib`), released here under the MIT licence. The database code (`src/*.cpp`) uses it
through `namespace banksia`.

## 1. Two programs from one source

The core fixes the board size and the piece set at compile time (`src/base/types.h`: with
`_CHESS_` a board has 64 squares, otherwise 90), so one translation unit serves one family of
games. OCGDB is therefore built twice:

| Program | Games | Compiled with |
|---|---|---|
| `ocgdb` | standard chess, chess960 | `-D_CHESS_ -D_BANKSIA_` |
| `ocgdb-xq` | xiangqi, jieqi | `-D_XIANGQI_ -D_JIEQI_` |

Every change must build and work in both; `make` builds both ([building.md](building.md)).

## 2. The folders

| Folder | Files | What |
|---|---|---|
| `base/` | `base`, `comm`, `enginescore`, `fileutil`, `jsonable`, `pgntag`, `rng`, `strutil`, `tickable`, `types`, `bitboardbase` | the board interface (`BoardCore`), moves and move history, notation helpers, PGN tags, files and strings, bitboards |
| `chess/` | `chess`, `chess_hash`, `chess_eco`, `chess960`, `chessbitboard`, `chess_db` | the chess and chess960 boards, Polyglot hash keys, the ECO tables |
| `xq/` | `xq`, `xq_hash`, `xq_eco`, `jieqiboard`, `xqbitboard`, `xq_db` | the xiangqi and jieqi boards, hash keys, the xiangqi ECO tables |
| `3rdparty/json/` | jsoncpp (MIT) | needed by the core's settings code |

What the core does: move generation and legality, FEN, SAN and coordinate notation, the xiangqi
notations (WXF, traditional), hashing (Polyglot keys for chess), the ECO tables, chess960 castling
and jieqi reveals. `comm.cpp` takes `APP_NAME` and `APP_VERSION` from the Makefile.

## 3. What a database needs from the core

A game database stores variations, comments and binary move codes, and PQL needs bitboards. The
core has these parts for that; the database-only ones are kept in files of their own
(`chess/chess_db.cpp`, `xq/xq_db.cpp`) or in blocks marked `OCGDB`, so that the general board code
stays easy to read.

| Part | Where | What for |
|---|---|---|
| `VariationRec` in `Hist`, `variationPieces` | `enginescore.h` | which variation a move belongs to |
| `ParseVariation` (parse, asComment, discard) | `types.h` | what the PGN parser does with `( ... )` |
| `DupRec`, `MoveStringLevel`, `MoveWithLevel` (with `code`), `PositionCallback` | `base.h` | duplicates, the parsers' move lists, the per-position callback of PQL |
| `_mainLineHists()`, `getMainLineSize()` | `base.h` | the game's own moves (PlyCount, duplicates, ECO) |
| `checkMake(Move, VariationRec)`, `_adjustBoardToMoveLevel()`, `_adjustBoardToMoveBackLevel()`, `_removeAllVariations()`, variation-aware `_make()`, `_takeBack()`, `_setupPlyCounters()` | `base.cpp` | moving between the variations of a game |
| `fromMoveList(record, notation, ParseVariation, ..., PositionCallback)` | `base.cpp` | PGN move text with variations; a callback at every position (PQL) |
| `fromMoveList(record, moveVec, flag, PositionCallback)`, `_fromMoveList(MoveWithLevel)` | `base.cpp` | the binary move columns, variations included |
| `toMoveListString()` with variations, in brackets | `base.cpp` | PGN export |
| `encode2Bytes`/`decode2Bytes`/`encode1Byte`/`decode1Byte`, `getHashKeyForCheckingDuplicates`, `createCheckingDuplicateRec`, `posToBitboards` | `base.h`, `chess/chess_db.cpp`, `xq/xq_db.cpp`, `xq/jieqiboard.cpp` | the Moves2 and Moves1 columns, duplicates, PQL |
| `ChessBoard::staticInit()`, `_posToBitboard`, `pattern_shift*` | `chess/chess_db.cpp` | PQL bitboards and `#` patterns |
| `completeDecodedMove()`, `isBoardDependent2Bytes()` | `base.h`, `jieqiboard.*` | jieqi reveals in Moves2 |
| bitboard classes | `base/bitboardbase.*`, `chess/chessbitboard.*`, `xq/xqbitboard.*` | PQL |

The move codes themselves are described in the user guide, §8.4.

## 4. How the decoders work

- **Moves2** codes are decoded move by move on the board, so a jieqi reveal (whose meaning depends
  on which covered piece stands on the from-square) is completed when its move is made.
- **Moves1** names a piece by the index it has at that moment of the game, so it too is decoded
  move by move (`_fromMoveList1Byte`). The table of move offsets used by the Moves1 encoder is
  built once, inside its static initializer, so several threads can use it safely.
- **Variations.** A variation that cannot be replayed (a move that is not legal there) is
  dropped and the game is read again without it, so a broken side line never costs the main line.
- **Damaged data.** None of the decoders asserts on a blob it cannot read: the game stops where
  the data breaks. The PGN parsers treat damaged SAN (a move naming its destination as its origin,
  a xiangqi move with `=`, a string too short to be a move) as an illegal move.
- **Xiangqi FENs.** A plain xiangqi FEN lists no captured pieces; the board counts what is missing
  from a side's full set as captured. A jieqi FEN keeps its own `[...]` field of pieces taken.
- **ECO.** `getLastEco()` walks the main line only and also looks up the position after the last
  move, so a game that ends exactly on an opening position gets that opening.

## 5. Not in the core

OCGDB is a database tool, so the parts of the Banksia core that play games are not included:

| Left out | Why |
|---|---|
| games against engines, matches, books, ratings, the command-line front end | a tournament manager's work |
| Syzygy and Felicity endgame tablebases | not needed by a database; `probeEgtb()` answers "no tablebase" |
| the Asian-rule judge of perpetual checks and chases | a database stores the results games were given; `XqBoard::ruleRepetition()` makes no judgement |
| engine-analysis comment parsing, move evaluation symbols (`!?`), the Vietnamese and Chinese notations | comments are stored as text; evaluation symbols and NAGs are read and dropped |

Each place where a call into a left-out part was removed is marked `OCGDB:` in the source.
