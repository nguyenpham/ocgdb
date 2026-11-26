# This file is part of Open Chess Game Database Standard.
#
# Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)
#
# Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
# or copy at http://opensource.org/licenses/MIT)
#
# Builds two programs from the same sources (docs/building.md):
#
#   ocgdb       chess and chess960                  (-D_CHESS_)
#   ocgdb-xq    xiangqi and jieqi                   (-D_XIANGQI_ -D_JIEQI_)
#
# Two programs, not one, because the core (base/types.h, taken from Banksia) fixes
# the board size and the piece set with the preprocessor: a translation unit serves
# one family only. SQLite (SQLite3 Multiple Ciphers) does not depend on the family
# and is compiled once.
#
#   make                 both programs
#   make chess / xq      one of them
#   make debug           both, with assertions, -O0 -g: ocgdb-debug, ocgdb-xq-debug
#                        (objects in build-debug/)
#   make clean
#   make -j8             in parallel

CXX      ?= c++
CC       ?= cc

BUILD    ?= build
OPT      ?= -O3 -DNDEBUG

CXXFLAGS += -std=c++20 $(OPT) -Wall -Wextra -I. -pthread
CFLAGS   += $(OPT) -w

# The same options as Banksia GUI's copy (banksiagui tools/sqlite3mc/sqlite3mc-sources.cmake),
# so the databases and the SQL of both programs are the same.
SQLITE_DEFS = -DCODEC_TYPE=CODEC_TYPE_SQLCIPHER -DSQLITE_ENABLE_COLUMN_METADATA \
	-DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS -DSQLITE_ENABLE_FTS4 \
	-DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_GEOPOLY -DSQLITE_ENABLE_RTREE \
	-DSQLITE_ENABLE_MATH_FUNCTIONS -DSQLITE_THREADSAFE=1

LDLIBS   = -pthread
ifeq ($(shell uname -s),Linux)
LDLIBS  += -ldl -lm
endif

SHARED_SRC = $(wildcard *.cpp) $(wildcard base/*.cpp) \
	$(wildcard 3rdparty/SQLiteCpp/*.cpp) 3rdparty/json/jsoncpp.cpp
CHESS_SRC  = $(SHARED_SRC) $(wildcard chess/*.cpp)
XQ_SRC     = $(SHARED_SRC) $(wildcard xq/*.cpp)

CHESS_OBJ  = $(patsubst %.cpp,$(BUILD)/chess/%.o,$(CHESS_SRC))
XQ_OBJ     = $(patsubst %.cpp,$(BUILD)/xq/%.o,$(XQ_SRC))
SQLITE_OBJ = $(BUILD)/sqlite3mc.o

# the program's version (ocgdb -version, the Info table's "Generator" row)
VERSION    = 2.0.0

CHESS_DEFS = -D_CHESS_ -D_BANKSIA_ -DAPP_NAME='"ocgdb"' -DAPP_VERSION='"$(VERSION)"'
XQ_DEFS    = -D_XIANGQI_ -D_JIEQI_ -DAPP_NAME='"ocgdb-xq"' -DAPP_VERSION='"$(VERSION)"'

CHESS_BIN ?= ocgdb
XQ_BIN    ?= ocgdb-xq

.PHONY: all chess xq debug clean

all: chess xq

chess: $(CHESS_BIN)
xq: $(XQ_BIN)

$(CHESS_BIN): $(CHESS_OBJ) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(XQ_BIN): $(XQ_OBJ) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/chess/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CHESS_DEFS) -MMD -MP -c $< -o $@

$(BUILD)/xq/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(XQ_DEFS) -MMD -MP -c $< -o $@

# third-party code: its warnings are not ours to fix
$(BUILD)/chess/3rdparty/%.o $(BUILD)/xq/3rdparty/%.o: CXXFLAGS += -w

$(SQLITE_OBJ): 3rdparty/sqlite3/sqlite3mc_amalgamation.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SQLITE_DEFS) -c $< -o $@

debug:
	$(MAKE) BUILD=build-debug OPT="-O0 -g" CHESS_BIN=ocgdb-debug XQ_BIN=ocgdb-xq-debug all

clean:
	rm -rf build build-debug ocgdb ocgdb-xq ocgdb-debug ocgdb-xq-debug

-include $(CHESS_OBJ:.o=.d) $(XQ_OBJ:.o=.d)
