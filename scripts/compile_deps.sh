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
echo "  DEAR IMGUI"
echo "************************************************"

PREFIX=lib_imgui
IMGUI_BUILD_DIR=./external/repos/imgui/build

CXXFLAGS="${CXXFLAGS} -Iexternal -Iexternal/repos/stb -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_cpp_file external/repos/imgui/imgui.cpp ${PREFIX}
compile_cpp_file external/repos/imgui/imgui_draw.cpp ${PREFIX}
compile_cpp_file external/repos/imgui/imgui_tables.cpp ${PREFIX}
compile_cpp_file external/repos/imgui/imgui_widgets.cpp ${PREFIX}
compile_cpp_file external/repos/imgui/imgui_demo.cpp ${PREFIX}
compile_lib imgui ${PREFIX}

echo "************************************************"
echo "  LUA 5.1"
echo "************************************************"

PREFIX=lib_lua_
LUA_DIR=external/repos/lua
CFLAGS="${CFLAGS} -DMAKE_LIB -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
compile_c_file ${LUA_DIR}/onelua.c ${PREFIX}
compile_lib lua51 ${PREFIX}


echo "************************************************"
echo "  xxHash"
echo "************************************************"

PREFIX=lib_xxh_
CFLAGS="${CFLAGS} -DMAKE_LIB -Wno-unused-function -Wno-implicit-int-conversion -Wno-shorten-64-to-32"
XXHASH_DIR=./external/repos/xxHash
(cd ${XXHASH_DIR} && make clean && CFLAGS="-DXXH_NO_STDLIB" make -j8)
cp -v ${XXHASH_DIR}/libxxhash.a ${BUILD_DIR}
