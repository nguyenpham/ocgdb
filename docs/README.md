# OCGDB documentation

| Page | For | What |
|---|---|---|
| [user-guide.md](user-guide.md) | users | everything about the programs `ocgdb` and `ocgdb-xq`: ready-made databases to try, tasks, parameters, options, PQL, SQL, the database format, Banksia GUI and MoonRiver, speed, troubleshooting, what is new in version 2 |
| [building.md](building.md) | builders | requirements, `make`, the two programs, what is compiled, SQLite with encryption, Windows |
| [testing.md](testing.md) | developers | the regression tests, and the checks on large databases (round trips, PQL counts, python-chess, speed), with numbers |
| [core.md](core.md) | developers | the chess and xiangqi core: folders, what a database needs from it, how the move decoders work |

In the repository root: [README.md](../README.md) (the standard: why SQL, the tables and fields,
the move encodings, PQL in brief).

Free databases in this format, used as examples throughout:
[BanksiaChessHub](https://github.com/nguyenpham/BanksiaChessHub) (chess: Banksia Masters,
8 million games; Lichess Puzzles) and
[MoonRiverChineseChessHub](https://github.com/nguyenpham/MoonRiverChineseChessHub) (xiangqi:
MoonRiver Masters, 135,030 games) — see [user-guide.md](user-guide.md) §3.
