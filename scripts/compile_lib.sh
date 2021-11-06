#!/usr/bin/env bash

set -e

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

PREFIX=lib_

# Output the files with a prefix: e.g. lib_atlaspacker.cpp.o
compile_c_file src/atlaspacker.c ${PREFIX}
compile_c_file src/binpacker.c ${PREFIX}
compile_c_file src/tilepacker.c ${PREFIX}

# Gathers all object files matching the prefix
compile_lib atlaspacker ${PREFIX}


# stb
PREFIX=lib_stb_
CFLAGS="${CFLAGS} -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/stb_wrappers.c ${PREFIX}
compile_lib stb ${PREFIX}