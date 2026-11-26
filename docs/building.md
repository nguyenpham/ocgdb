# Building OCGDB

## 1. Requirements

- a C++20 compiler: clang 14+ or gcc 11+
- a C compiler for SQLite
- `make`

Nothing else: no Qt, no CMake, no libraries to install. SQLite (with encryption) is compiled into
the programs.

Tested with Apple clang (Xcode command line tools) on macOS (Apple M1 Pro), and with MSYS2
MinGW64 GCC on Windows (§5).

## 2. Build

```bash
git clone https://github.com/nguyenpham/ocgdb.git
cd ocgdb/src
make -j8          # both programs
make chess        # only ocgdb
make xq           # only ocgdb-xq
make debug        # ocgdb-debug and ocgdb-xq-debug: -O0 -g, assertions on (objects in build-debug/)
make clean
```

The programs are written into `src/`. A clean `make -j8` takes under a minute on an Apple M1 Pro,
most of it SQLite (the 13 MB amalgamation, compiled once for both programs).

| Program | Plays | Compiled with |
|---|---|---|
| `ocgdb` | standard chess, chess960 | `-D_CHESS_ -D_BANKSIA_` |
| `ocgdb-xq` | xiangqi, jieqi | `-D_XIANGQI_ -D_JIEQI_` |

Why two programs: the core (`src/base/types.h`) fixes the board size and the piece set with the
preprocessor, so a translation unit serves one family ([core.md](core.md)). A database records its
variant (Info table, row `Variant`), and each program refuses the other family's databases with a
message naming the right program.

The version is `VERSION` in `src/Makefile` (2.0.0); it reaches the programs as `APP_VERSION`
and is written into new databases (Info row `Generator`).

Check the build with the tests (about 10 seconds, [testing.md](testing.md)):

```bash
cd .. && tests/run-tests.sh
```

## 3. What is compiled

| Folder | What | Notes |
|---|---|---|
| `src/*.cpp` | the database program | both programs; `wxfread.cpp` only does something in `ocgdb-xq` |
| `src/base/` | the core, shared | [core.md](core.md) |
| `src/chess/` | chess and chess960 boards | `ocgdb` only |
| `src/xq/` | xiangqi and jieqi boards | `ocgdb-xq` only |
| `src/3rdparty/sqlite3/` | SQLite3 Multiple Ciphers 2.5.1 (SQLite 3.53.4), the amalgamation | compiled once (`build/sqlite3mc.o`) |
| `src/3rdparty/SQLiteCpp/` | the C++ wrapper of SQLite (SQLiteCpp, MIT) | |
| `src/3rdparty/threadpool/` | Barak Shoshany's thread pool (MIT) | workers wait on a condition variable |
| `src/3rdparty/json/` | jsoncpp (MIT) | the core's settings code needs it |

Warnings: `-Wall -Wextra`; the code of `3rdparty/` is compiled with `-w`. A clean build gives no
warning.

## 4. SQLite

`src/3rdparty/sqlite3/` is SQLite3 Multiple Ciphers 2.5.1, unmodified (its file
`VERSION-sqlite3mc-2.5.1` has the source URL and SHA-256). It is used instead of plain SQLite so
that:

- the SQL is a recent SQLite's (3.53.4: window functions, `RETURNING`, JSON, `->>`, math
  functions, FTS5...);
- databases can be encrypted (`-password`) in SQLCipher 4's format, the one Banksia GUI and
  MoonRiver use (compile option `CODEC_TYPE=CODEC_TYPE_SQLCIPHER`, see the Makefile's
  `SQLITE_DEFS`).

## 5. Other platforms

**Linux**: the Makefile adds `-ldl -lm`.

**Windows** (MSYS2 MinGW64, GCC 16.2). From an MSYS2 MINGW64 shell:

```bash
cd src && make -j8 LDLIBS="-pthread -static" && cd .. && tests/run-tests.sh
cd src && make -j8 debug LDLIBS="-pthread -static" && cd .. && OCGDB=src/ocgdb-debug OCGDB_XQ=src/ocgdb-xq-debug tests/run-tests.sh
```

- `-static` links the GCC runtime into `ocgdb.exe` / `ocgdb-xq.exe`. Without it they need
  `libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll` from `C:\msys64\mingw64\bin`;
  run from Git Bash they then exit silently with code 127, because Git for Windows has its own,
  incompatible `libstdc++-6.dll` earlier on its PATH.
- `tests/run-tests.sh` converts its temporary folder with `cygpath -m` on Windows: the shell
  rewrites a `/tmp/...` argument for a Windows program, but not a path inside an SQL string
  (`ATTACH '...'`).
- The `.exe` outputs are in `.gitignore`.

There are no Xcode or Visual Studio projects: `make` is the build.
