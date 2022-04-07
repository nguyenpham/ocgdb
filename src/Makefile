# This file is part of Open Chess Game Database Standard.
#
# Copyright (c) 2022 Nguyen Pham (github@nguyenpham)
# Copyright (c) 2022 Developers
#
# Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
# or copy at http://opensource.org/licenses/MIT)

all:
	g++ -std=c++17 -DNDEBUG -O3 -c *.cpp
	g++ -std=c++17 -DNDEBUG -O3 -c board/*.cpp
	g++ -std=c++17 -DNDEBUG -O3 -c 3rdparty/SQLiteCpp/*.cpp
	gcc -DNDEBUG -O3 -c 3rdparty/sqlite3/*.c
	g++ -o ocgdb *.o -lpthread -ldl

clean:
	rm -f ocgdb *.o ./board/*.o ./3rdparty/SQLiteCpp/*.o ./3rdparty/sqlite3/*.o
