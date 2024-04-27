#!/usr/bin/env bash

set -e

PRODUCT=atlaspackergui

SCRIPTDIR=$(dirname "$0")
source ${SCRIPTDIR}/compile.sh

if [ "${SOKOL_DIR}" == "" ]; then
	echo "You must specify location of the Sokol repository folder using SOKOL_DIR"
	exit 1
fi


SOKOL_DIR=$(realpath ${SOKOL_DIR})

BUILD_DIR=./build/tool
CIMGUI_BUILD_DIR=./build/cimgui

if [ ! -d "${BUILD_DIR}" ]; then
	mkdir -p ${BUILD_DIR}
fi

LIB_SUFFIX=.a
CIMGUI_LIB=${CIMGUI_BUILD_DIR}/libcimgui${LIB_SUFFIX}
CIMGUI_DIR=${CIMGUI_BUILD_DIR}
CIMGUI_DIR=$(realpath ${CIMGUI_DIR})

if [ ! -e "${CIMGUI_LIB}" ]; then
	echo "Couldn't find ${CIMGUI_LIB}"

	pushd ./build
	git clone --recursive git@github.com:cimgui/cimgui.git
	popd

	pushd ${CIMGUI_BUILD_DIR}
	cmake -DIMGUI_STATIC=1 .
	make -j8
	mv cimgui${LIB_SUFFIX} libcimgui${LIB_SUFFIX}
	popd
fi

#OPT="-O0 -g"
CFLAGS="${CFLAGS} -I${SOKOL_DIR} -I${SOKOL_DIR}/util -I${CIMGUI_DIR} -Iexternal/nativefiledialog/src/include"

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

compile_objc_file tool/worker.c
compile_objc_file tool/editor.c

run_cmd ${LD} -o ${BUILD_DIR}/${PRODUCT} \
		${OPT} \
		${LDFLAGS} \
		-L${CIMGUI_BUILD_DIR} \
		${ARCH} \
		-framework Foundation \
		-framework CoreGraphics \
		-framework AppKit \
		-framework OpenGL \
		-lcimgui \
		-latlaspacker \
		-lstb \
		-lnfd \
		${BUILD_DIR}/*.o

