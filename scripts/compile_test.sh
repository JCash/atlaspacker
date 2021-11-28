#!/usr/bin/env bash

set -e

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

PREFIX=testpacker

export CFLAGS="${CFLAGS} -DJC_TEST_USE_COLORS"

NAME=utils
compile_c_file test/render.c test${NAME}
compile_c_file test/utils.c test${NAME}
compile_lib test${NAME} test${NAME}

# Output the files with a prefix: e.g. lib_atlaspacker.cpp.o
# Gathers all object files matching the prefix into a library

NAME=atlaspacker
compile_cpp_file test/test_${NAME}.cpp test${NAME}
compile_lib test${NAME} test${NAME}
link_exe test_${NAME} test${NAME} testutils stb atlaspacker

NAME=convexhull
compile_cpp_file test/test_${NAME}.cpp test${NAME}
compile_lib test${NAME} test${NAME}
link_exe test_${NAME} test${NAME} testutils stb atlaspacker

NAME=binpacker
compile_cpp_file test/test_${NAME}.cpp test${NAME}
compile_lib test${NAME} test${NAME}
link_exe test_${NAME} test${NAME} testutils stb atlaspacker

NAME=tilepacker
compile_cpp_file test/test_${NAME}.cpp test${NAME}
compile_lib test${NAME} test${NAME}
link_exe test_${NAME} test${NAME} testutils stb atlaspacker
