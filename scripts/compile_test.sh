#!/usr/bin/env bash

set -e

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

PREFIX=testpacker

export CFLAGS="${CFLAGS} -DJC_TEST_USE_COLORS"

# Output the files with a prefix: e.g. lib_atlaspacker.cpp.o
compile_cpp_file test/test_packer.cpp ${PREFIX}

# Gathers all object files matching the prefix into a library
compile_lib testpacker ${PREFIX}

# Gathers creates an executable by linking the libraries
link_exe test_packer testpacker stb atlaspacker

