#!/usr/bin/env bash

set -e

PRODUCT=atlaspackergui

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh


SOKOL_DIR=$(realpath ${SCRIPTDIR}/../external/repos/sokol)
NFD_DIR=$(realpath ${SCRIPTDIR}/../external/nativefiledialog/src/include)
LUA_DIR=$(realpath ${SCRIPTDIR}/../external/repos/lua)
IMGUI_DIR=$(realpath ${SCRIPTDIR}/../external/repos/imgui)

BUILD_DIR=./build/tool

if [ ! -d "${BUILD_DIR}" ]; then
	mkdir -p ${BUILD_DIR}
fi

#OPT="-O0 -g"
FLAGS="-Iinclude -Iexternal -I${SOKOL_DIR} -I${SOKOL_DIR}/util -I${IMGUI_DIR} -I${NFD_DIR} -I${LUA_DIR}"
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

compile_c_file tool/worker.c
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
		-lstb \
		-lnfd \
		-limgui \
		-llua51 \
		${BUILD_DIR}/*.o

