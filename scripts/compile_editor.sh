#!/usr/bin/env bash

set -e

PRODUCT=atlaspackergui

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh


SOKOL_DIR=$(realpath ${SCRIPTDIR}/../external/repos/sokol)
NFD_DIR=$(realpath ${SCRIPTDIR}/../external/nativefiledialog/src/include)
LUA_DIR=$(realpath ${SCRIPTDIR}/../external/repos/lua)
IMGUI_DIR=$(realpath ${SCRIPTDIR}/../external/repos/imgui)
XXHASH_DIR=$(realpath ${SCRIPTDIR}/../external/repos/xxHash)

BUILD_DIR=./build/tool

if [ ! -d "${BUILD_DIR}" ]; then
	mkdir -p ${BUILD_DIR}
fi

#OPT="-O0 -g"
FLAGS="-Iinclude -I. -I${SOKOL_DIR} -I${SOKOL_DIR}/util -I${IMGUI_DIR} -I${NFD_DIR} -I${LUA_DIR} -I${XXHASH_DIR}"
CFLAGS="${CFLAGS} ${FLAGS}"
CXXFLAGS="${CXXFLAGS} ${FLAGS}"

function compile_c_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CC} -o ${BUILD_DIR}/${basename}.o ${OPT} $DISASSEMBLY ${ARCH} ${CFLAGS} ${SOKOL_DEFINES} -I${SOKOL_DIR} -c ${name}
}

function compile_cxx_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CXX} -o ${BUILD_DIR}/${basename}.o ${OPT} $DISASSEMBLY ${ARCH} ${CXXFLAGS} ${SOKOL_DEFINES} -I${SOKOL_DIR} -c ${name}
}

function compile_objc_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CC} -o ${BUILD_DIR}/${basename}.o -ObjC ${OPT} $DISASSEMBLY ${ARCH} ${CFLAGS} ${SOKOL_DEFINES} -c ${name}
}

function compile_objcxx_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CXX} -o ${BUILD_DIR}/${basename}.o -ObjC++ ${OPT} $DISASSEMBLY ${ARCH} ${CXXFLAGS} ${SOKOL_DEFINES} -c ${name}
}

if [ -f ${BUILD_DIR}/${PRODUCT} ]; then
    rm ${BUILD_DIR}/*.o
    rm ${BUILD_DIR}/${PRODUCT}
fi

compile_cxx_file tool/commands_project.cpp
compile_cxx_file tool/commands_misc.cpp
compile_cxx_file tool/gui.cpp
compile_cxx_file tool/hash.cpp
compile_cxx_file tool/image.cpp
compile_cxx_file tool/state.cpp
compile_cxx_file tool/thread.cpp
compile_cxx_file tool/tree.cpp
compile_cxx_file tool/worker.cpp
compile_objcxx_file tool/sys.cpp
compile_objcxx_file tool/editor.cpp

run_cmd ${LD} -o ${BUILD_DIR}/${PRODUCT} \
		${OPT} \
		${LDFLAGS} \
		${ARCH} \
		-framework Foundation \
		-framework CoreGraphics \
		-framework AppKit \
		-framework OpenGL \
        -latlaspacker \
        -limgui \
        -llua51 \
        -lnfd \
        -lstb \
        -lcjson \
        -lxxhash \
		${BUILD_DIR}/*.o

