#!/usr/bin/env bash

set -e

PRODUCT=atlaspackergui

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh


SOKOL_DIR=$(realpath ${SCRIPTDIR}/../external/repos/sokol)
CIMGUI_DIR=$(realpath ${SCRIPTDIR}/../external/repos/cimgui)
NFD_DIR=$(realpath ${SCRIPTDIR}/../external/nativefiledialog/src/include)
LUA_DIR=$(realpath ${SCRIPTDIR}/../external/repos/lua)

BUILD_DIR=./build/tool

if [ ! -d "${BUILD_DIR}" ]; then
	mkdir -p ${BUILD_DIR}
fi

#OPT="-O0 -g"
CFLAGS="${CFLAGS} -Iinclude -Iexternal -I${SOKOL_DIR} -I${SOKOL_DIR}/util -I${CIMGUI_DIR} -I${NFD_DIR} -I${LUA_DIR}"

function compile_c_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CC} -o ${BUILD_DIR}/${basename}.o ${OPT} $DISASSEMBLY ${ARCH} ${CFLAGS} ${SOKOL_DEFINES} -I${SOKOL_DIR} -c ${name}
}

function compile_objc_file {
    local name=$1
    local basename=$(basename $name)
    echo "$basename"
    run_cmd ${CC} -o ${BUILD_DIR}/${basename}.o -ObjC ${OPT} $DISASSEMBLY ${ARCH} ${CFLAGS} ${SOKOL_DEFINES} -c ${name}
}

compile_c_file tool/worker.c
compile_objc_file tool/editor.c

run_cmd ${LD} -o ${BUILD_DIR}/${PRODUCT} \
		${OPT} \
		${LDFLAGS} \
		${ARCH} \
		-framework Foundation \
		-framework CoreGraphics \
		-framework AppKit \
		-framework OpenGL \
		-lcimgui \
		-latlaspacker \
		-lstb \
		-lnfd \
		-llua51 \
		${BUILD_DIR}/*.o

