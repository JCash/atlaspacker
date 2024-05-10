#!/usr/bin/env bash

set -e

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

PREFIX=lib_

LUA_DIR=$(realpath ${SCRIPTDIR}/../external/repos)
echo LUA_DIR=${LUA_DIR}
CFLAGS="${CFLAGS} -Iinclude -Iexternal -I${LUA_DIR}"

# Output the files with a prefix: e.g. lib_atlaspacker.cpp.o
compile_c_file src/atlaspacker.c ${PREFIX}
compile_c_file src/binpacker.c ${PREFIX}
compile_c_file src/tilepacker.c ${PREFIX}
compile_c_file src/convexhull.c ${PREFIX}
compile_c_file src/project.c ${PREFIX}
compile_c_file src/exporter.c ${PREFIX}
compile_c_file src/util.c ${PREFIX}

# Gathers all object files matching the prefix
compile_lib atlaspacker ${PREFIX}

