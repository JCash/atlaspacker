#!/usr/bin/env bash

set -e

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

PREFIX=lib_

# stb
PREFIX=lib_stb_
CFLAGS="${CFLAGS} -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/stb_wrappers.c ${PREFIX}
compile_lib stb ${PREFIX}


# cJSON
PREFIX=lib_cjson_
CFLAGS="${CFLAGS} -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/cJSON.c ${PREFIX}
compile_lib cjson ${PREFIX}

# cJSON
PREFIX=lib_nfd_
CFLAGS="${CFLAGS} -Iexternal/nativefiledialog/src/include"
compile_c_file external/nativefiledialog/src/nfd_common.c ${PREFIX}
compile_objc_file external/nativefiledialog/src/nfd_cocoa.m ${PREFIX}
compile_lib nfd ${PREFIX}
