#!/usr/bin/env bash

# This file is part of Open Chess Game Database Standard.
#
# Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
#
# Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
# or copy at http://opensource.org/licenses/MIT)

#
# Regression tests of ocgdb and ocgdb-xq.
#
#   tests/run-tests.sh
#
# The programs are taken from src/ (build them first: cd src && make), or from $OCGDB and
# $OCGDB_XQ. Everything is written into a temporary folder, removed at the end. The data are
# the small files of tests/data: handmade chess games, and chess960, xiangqi and jieqi games
# generated at random (legal moves) -- no third-party games.
#
# Exit code: the number of failed checks.

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
DATA="$HERE/data"
OCGDB=${OCGDB:-$ROOT/src/ocgdb}
OCGDB_XQ=${OCGDB_XQ:-$ROOT/src/ocgdb-xq}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
# Windows (Git Bash / MSYS2): the shell rewrites a /tmp/... argument for a Windows program, but not
# a path inside an SQL string (ATTACH '...'), so use the mixed form C:/Users/.../Temp/... throughout.
if command -v cygpath > /dev/null 2>&1; then
    TMP=$(cygpath -m "$TMP")
fi

for p in "$OCGDB" "$OCGDB_XQ"; do
    if [ ! -x "$p" ]; then
        echo "Missing $p: build it first (cd src && make)"
        exit 1
    fi
done

pass=0
fail=0

check() { # name expected actual
    if [ "$2" == "$3" ]; then
        pass=$((pass + 1))
        echo "PASS  $1"
    else
        fail=$((fail + 1))
        echo "FAIL  $1: expected '$2', got '$3'"
    fi
}

# the count of a search: found <program> <args...>
found() {
    "$@" 2>&1 | sed -n 's/^; Found: //p' | tail -1
}

# the first value of an SQL query: sqlv <program> <db> <sql> [more args]
sqlv() {
    local p=$1 db=$2 q=$3
    shift 3
    "$p" -db "$db" -sql "$q" "$@" 2>/dev/null | grep -A2 '^SQL:' | sed -n 3p
}

# the PGN of a file without the tags the database computes (PlyCount, ECO)
plain() {
    grep -v '^\[PlyCount \|^\[ECO ' "$1"
}

echo "== chess: create, tags, counts"
"$OCGDB" -create -pgn "$DATA/chess.pgn" -db "$TMP/c.ocgdb.db3" > "$TMP/log" 2>&1
check "create: 8 games" "8" "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
check "create: no error" "" "$(grep -i 'error' "$TMP/log")"
check "Info: GameCount" "8" "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT Value FROM Info WHERE Name = 'GameCount'")"
check "Info: Variant" "standard" "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT Value FROM Info WHERE Name = 'Variant'")"
check "escaped quotes and backslash in a tag" 'Test "quoted" event \ backslash' \
    "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT e.Name FROM Games g JOIN Events e ON e.ID = g.EventID WHERE g.ID = 1")"
check "date with ??" "2026-11-11" "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT Date FROM Games WHERE ID = 3")"
check "PlyCount is the main line's" "41" "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT PlyCount FROM Games WHERE ID = 1")"
check "the comment before the first move" "A first comment before the moves." \
    "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT Comment FROM Comments WHERE GameID = 1 AND Ply = -1")"
check "game IDs follow the file" "Scholar's mate" \
    "$(sqlv "$OCGDB" "$TMP/c.ocgdb.db3" "SELECT e.Name FROM Games g JOIN Events e ON e.ID = g.EventID WHERE g.ID = 7")"

echo "== chess: PQL (expected counts from python-chess 1.11.2)"
check "mate" "2" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "mate")"
check "check" "3" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "check")"
check "move[P=N, p=n]" "1" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "move[P=N, p=n]")"
check "move[O-O-O]" "1" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "move[O-O-O]")"
check "line[...]" "3" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "line[1. e4 e5 2. Nf3 Nc6 3. Bb5]")"
check "then" "1" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "move[O-O-O] then K[b1]")"
check "parse error is reported" "Error: unexpected near \"R\"" \
    "$("$OCGDB" -db "$TMP/c.ocgdb.db3" -q "Q = 1 R = 7" 2>&1 | grep '^Error')"
check "-where" "1" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "mate" -where "w.Name = 'Quick'")"
check "search a PGN file" "2" "$(found "$OCGDB" -pgn "$DATA/chess.pgn" -q "mate")"

echo "== chess: export and import again"
"$OCGDB" -export -db "$TMP/c.ocgdb.db3" -pgn "$TMP/c.pgn" > /dev/null 2>&1
"$OCGDB" -create -pgn "$TMP/c.pgn" -db "$TMP/c2.ocgdb.db3" > /dev/null 2>&1
check "round trip: moves, variations, comments" "8|8|8" \
    "$("$OCGDB" -db "$TMP/c2.ocgdb.db3" -sql "ATTACH '$TMP/c.ocgdb.db3' AS o" -sql "SELECT COUNT(*), SUM(a.Moves2 IS b.Moves2), SUM((SELECT COUNT(*) FROM main.Comments c WHERE c.GameID = a.ID) = (SELECT COUNT(*) FROM o.Comments c WHERE c.GameID = a.ID)) FROM main.Games a JOIN o.Games b ON a.ID = b.ID" 2>/dev/null | grep -A1 '^COUNT' | tail -1)"
check "the exported moves of game 1" "1.e4 e5 2.Nf3 Nc6" "$(grep -m1 -o '1\.e4 e5 2\.Nf3 Nc6' "$TMP/c.pgn")"
check "a variation starting with Black is numbered" "(9...Na5" "$(grep -m1 -o '( \?9\.\.\.Na5' "$TMP/c.pgn" | tr -d ' ')"

echo "== chess: blocks of 64 bytes (a block boundary in every game)"
OCGDB_PGN_BLOCK=64 "$OCGDB" -create -pgn "$DATA/chess.pgn" -db "$TMP/c64.ocgdb.db3" > /dev/null 2>&1
check "the same database" "8|8" \
    "$("$OCGDB" -db "$TMP/c64.ocgdb.db3" -sql "ATTACH '$TMP/c.ocgdb.db3' AS o" -sql "SELECT COUNT(*), SUM(a.Moves2 IS b.Moves2) FROM main.Games a JOIN o.Games b ON a.ID = b.ID" 2>/dev/null | grep -A1 '^COUNT' | tail -1)"

echo "== chess: Moves1 and text moves give the same answers"
"$OCGDB" -create -pgn "$DATA/chess.pgn" -db "$TMP/c1.ocgdb.db3" -o moves1 > /dev/null 2>&1
"$OCGDB" -create -pgn "$DATA/chess.pgn" -db "$TMP/ct.ocgdb.db3" -o moves > /dev/null 2>&1
for q in "mate" "check" "move[P=N, p=n]" "line[1. e4 e5 2. Nf3 Nc6 3. Bb5]"; do
    check "Moves1: $q" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "$q")" "$(found "$OCGDB" -db "$TMP/c1.ocgdb.db3" -q "$q")"
    check "Moves:  $q" "$(found "$OCGDB" -db "$TMP/c.ocgdb.db3" -q "$q")" "$(found "$OCGDB" -db "$TMP/ct.ocgdb.db3" -q "$q")"
done
check "-g from Moves1" "4.Qxf7# 1-0" "$("$OCGDB" -db "$TMP/c1.ocgdb.db3" -g 7 2>&1 | grep -o '4\.Qxf7# 1-0')"

echo "== chess: duplicates, merge"
check "-dup" "duplicates: 1, removed: 0" "$("$OCGDB" -dup -db "$TMP/c.ocgdb.db3" 2>&1 | grep -o 'duplicates: [0-9]*, removed: [0-9]*')"
check "-dup embededgames" "duplicates: 2, removed: 0" \
    "$("$OCGDB" -dup -db "$TMP/c.ocgdb.db3" -o embededgames 2>&1 | grep -o 'duplicates: [0-9]*, removed: [0-9]*')"
cp "$TMP/c.ocgdb.db3" "$TMP/m.ocgdb.db3"
"$OCGDB" -merge -db "$TMP/m.ocgdb.db3" -pgn "$DATA/chess.pgn" > /dev/null 2>&1
check "-merge a PGN file" "16" "$(sqlv "$OCGDB" "$TMP/m.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
"$OCGDB" -merge -db "$TMP/m.ocgdb.db3" -db "$TMP/c.ocgdb.db3" > /dev/null 2>&1
check "-merge a database" "24" "$(sqlv "$OCGDB" "$TMP/m.ocgdb.db3" "SELECT Value FROM Info WHERE Name = 'GameCount'")"
"$OCGDB" -dup -db "$TMP/m.ocgdb.db3" -o remove > /dev/null 2>&1
check "-dup remove" "7" "$(sqlv "$OCGDB" "$TMP/m.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
check "-dup remove keeps the first" "1,2,3,4,6,7,8" "$(sqlv "$OCGDB" "$TMP/m.ocgdb.db3" "SELECT group_concat(ID) FROM (SELECT ID FROM Games ORDER BY ID)")"
"$OCGDB" -create -pgn "$DATA/chess.pgn" -pgn "$DATA/chess.pgn" -db "$TMP/nd.ocgdb.db3" -o nodup > /dev/null 2>&1
check "-create nodup" "7" "$(sqlv "$OCGDB" "$TMP/nd.ocgdb.db3" "SELECT COUNT(*) FROM Games")"

echo "== chess: encryption"
"$OCGDB" -create -pgn "$DATA/chess.pgn" -db "$TMP/e.ocgdb.db3" -password "s3cret 'pw'" > /dev/null 2>&1
"$OCGDB" -info -db "$TMP/e.ocgdb.db3" > /dev/null 2>&1
check "without the password: refused" "1" "$?"
"$OCGDB" -info -db "$TMP/e.ocgdb.db3" -password wrong > /dev/null 2>&1
check "wrong password: refused" "1" "$?"
check "with the password" "8" "$(sqlv "$OCGDB" "$TMP/e.ocgdb.db3" "SELECT COUNT(*) FROM Games" -password "s3cret 'pw'")"

echo "== chess960"
"$OCGDB" -create -variant chess960 -pgn "$DATA/chess960.pgn" -db "$TMP/f.ocgdb.db3" > /dev/null 2>&1
check "create" "8" "$(sqlv "$OCGDB" "$TMP/f.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
check "castling in every game" "8" "$(found "$OCGDB" -db "$TMP/f.ocgdb.db3" -q "move[O-O, O-O-O]")"
"$OCGDB" -export -db "$TMP/f.ocgdb.db3" -pgn "$TMP/f.pgn" > /dev/null 2>&1
"$OCGDB" -create -variant chess960 -pgn "$TMP/f.pgn" -db "$TMP/f2.ocgdb.db3" > /dev/null 2>&1
check "round trip" "8|8" \
    "$("$OCGDB" -db "$TMP/f2.ocgdb.db3" -sql "ATTACH '$TMP/f.ocgdb.db3' AS o" -sql "SELECT COUNT(*), SUM(a.Moves2 IS b.Moves2 AND a.FEN IS b.FEN) FROM main.Games a JOIN o.Games b ON a.ID = b.ID" 2>/dev/null | grep -A1 '^COUNT' | tail -1)"

echo "== xiangqi and jieqi"
"$OCGDB_XQ" -create -pgn "$DATA/xq.pgn" -db "$TMP/x.ocgdb.db3" > /dev/null 2>&1
check "xiangqi: create" "6" "$(sqlv "$OCGDB_XQ" "$TMP/x.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
"$OCGDB_XQ" -export -db "$TMP/x.ocgdb.db3" -pgn "$TMP/x.pgn" > /dev/null 2>&1
check "xiangqi: export = input" "" "$(diff <(plain "$DATA/xq.pgn") <(plain "$TMP/x.pgn"))"
"$OCGDB_XQ" -create -pgn "$DATA/xq.pgn" -db "$TMP/x1.ocgdb.db3" -o moves1 > /dev/null 2>&1
check "xiangqi: Moves1 = Moves2" "$(found "$OCGDB_XQ" -db "$TMP/x.ocgdb.db3" -q "check")" \
    "$(found "$OCGDB_XQ" -db "$TMP/x1.ocgdb.db3" -q "check")"
check "xiangqi: start position" "6" "$(found "$OCGDB_XQ" -db "$TMP/x.ocgdb.db3" -q "R = 2 and C = 2 and ply = 0")"
"$OCGDB_XQ" -create -variant jieqi -pgn "$DATA/jieqi.pgn" -db "$TMP/j.ocgdb.db3" > "$TMP/log" 2>&1
check "jieqi: create, no error" "6" "$(sqlv "$OCGDB_XQ" "$TMP/j.ocgdb.db3" "SELECT COUNT(*) FROM Games")"
check "jieqi: no games with errors" "" "$(grep 'with errors' "$TMP/log")"
"$OCGDB_XQ" -export -db "$TMP/j.ocgdb.db3" -pgn "$TMP/j.pgn" > /dev/null 2>&1
check "jieqi: export = input" "" "$(diff <(plain "$DATA/jieqi.pgn") <(plain "$TMP/j.pgn"))"
"$OCGDB_XQ" -create -variant jieqi -pgn "$DATA/jieqi.pgn" -db "$TMP/j1.ocgdb.db3" -o moves1 > /dev/null 2>&1
check "jieqi: Moves1 refused" "1" "$?"

echo "== the right program for the variant"
"$OCGDB" -info -db "$TMP/x.ocgdb.db3" > /dev/null 2>&1
"$OCGDB" -q "check" -db "$TMP/x.ocgdb.db3" > /dev/null 2>&1
check "ocgdb refuses a xiangqi database" "1" "$?"
"$OCGDB_XQ" -q "check" -db "$TMP/c.ocgdb.db3" > /dev/null 2>&1
check "ocgdb-xq refuses a chess database" "1" "$?"

echo
echo "passed: $pass, failed: $fail"
exit $fail
