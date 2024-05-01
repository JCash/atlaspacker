#!/usr/bin/env bash

set -e

export CFLAGS="${CFLAGS} -Iinclude -Isrc -I. -Iexternal"

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

LIB_SUFFIX=.a

PREFIX=lib_

BUILD_DIR=$(realpath ${SCRIPTDIR}/../build)

echo BUILD_DIR=${BUILD_DIR}


echo "************************************************"
echo "  STB"
echo "************************************************"

PREFIX=lib_stb_
CFLAGS="${CFLAGS} -Iexternal -Iexternal/repos/stb -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/stb_wrappers.c ${PREFIX}
compile_lib stb ${PREFIX}

echo "************************************************"
echo "  CJSON"
echo "************************************************"

PREFIX=lib_cjson_
CFLAGS="${CFLAGS} -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/cJSON.c ${PREFIX}
compile_lib cjson ${PREFIX}

echo "************************************************"
echo "  NFD"
echo "************************************************"

PREFIX=lib_nfd_
CFLAGS="${CFLAGS} -Iexternal/nativefiledialog/src/include"
compile_c_file external/nativefiledialog/src/nfd_common.c ${PREFIX}
compile_objc_file external/nativefiledialog/src/nfd_cocoa.m ${PREFIX}
compile_lib nfd ${PREFIX}

echo "************************************************"
echo "  CIMGUI"
echo "************************************************"

CIMGUI_BUILD_DIR=./external/repos/cimgui/build
mkdir -p ${CIMGUI_BUILD_DIR}
pushd ${CIMGUI_BUILD_DIR}
cmake -DIMGUI_STATIC=1 ..
make -j8
cp -v cimgui${LIB_SUFFIX} ${BUILD_DIR}/libcimgui${LIB_SUFFIX}
popd

echo "************************************************"
echo "  LUA 5.1"
echo "************************************************"

PREFIX=lib_lua_
CFLAGS="${CFLAGS} -DMAKE_LIB -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file external/repos/lua/onelua.c ${PREFIX}
compile_lib lua51 ${PREFIX}
