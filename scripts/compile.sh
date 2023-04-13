#!/usr/bin/env bash

set -e

BUILD_DIR=./build

if [ ! -e ${BUILD_DIR} ]; then
    mkdir -p ${BUILD_DIR}
fi

#DISASSEMBLY="-S -masm=intel"
#PREPROCESS="-E"

if [ "$USE_ASAN" != "" ]; then
    if [ "$CXX" != "g++" ]; then
        ASAN="-fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize=undefined"
        ASAN_LDFLAGS="-fsanitize=address  -fsanitize=undefined"
    fi
    echo Using ASAN
fi

if [ "$OPT" == "" ]; then
    OPT="-O2"
fi
echo Using OPT=$OPT

if [ "$STDCXXVERSION" == "" ]; then
    STDCXXVERSION=c++0x
fi
echo Using STDCXXVERSION: -std=$STDCXXVERSION

if [ "$STDCVERSION" == "" ]; then
    STDCVERSION=c11
fi
echo Using STDCVERSION: -std=$STDCVERSION


if [ "$CXX" == "" ]; then
    CXX=$(which clang++)
fi
echo Using CXX=$CXX  \"$($CXX --version | grep -e "version")\"

if [ "$CC" == "" ]; then
    CC=$(which clang)
fi
echo Using CC=$CC  \"$($CC --version | grep -e "version")\"

if [ "$AR" == "" ]; then
    AR=$(dirname ${CXX})/ar
fi
echo Using AR=$AR

if [ "$LD" == "" ]; then
    LD=${CXX}
fi
echo Using LD=$LD

if [ "${ARCH}" == "" ]; then
    if [ "Darwin" == "$(uname)" ]; then
        if [ "arm64" == "$(arch)" ]; then
            ARCH="-arch arm64"
        else
            ARCH="-arch x86_64"
        fi
    else
        ARCH="-m64"
    fi
fi
echo Using ARCH=${ARCH}

CFLAGS="$CFLAGS -g -std=$STDCVERSION -Wall -Iinclude -Isrc -I. -Iexternal -Itest $ASAN $PREPROCESS"
CXXFLAGS="$CXXFLAGS -g -std=$STDCXXVERSION -Wall -fno-exceptions -Wno-old-style-cast -Wno-double-promotion -Iinclude -Isrc -I. -Iexternal -Itest $ASAN $PREPROCESS"
LDFLAGS="$ASAN_LDFLAGS -g -L${BUILD_DIR}"

if [ "$CXX" == "clang++" ]; then
    CXXFLAGS="$CXXFLAGS -Wall -Weverything -Wno-poison-system-directories -Wno-global-constructors"
fi

if [ "$STDCXXVERSION" != "c++98" ]; then
    CXXFLAGS="$CXXFLAGS -Wno-zero-as-null-pointer-constant -Wno-c++98-compat"
fi

function run_cmd {
    if [ "$VERBOSE" != "0" ]; then
        echo $*
    fi
    $*
}

function compile_cpp_file {
    local name=$1
    local basename=$(basename $name)
    local prefix=$2
    echo "$basename"
    run_cmd ${CXX} -o ${BUILD_DIR}/${prefix}${basename}.o $OPT $DISASSEMBLY ${ARCH} $CXXFLAGS -c ${name}
}

function compile_c_file {
    local name=$1
    local basename=$(basename $name)
    local prefix=$2
    echo "$basename"
    run_cmd ${CC} -o ${BUILD_DIR}/${prefix}${basename}.o $OPT $DISASSEMBLY ${ARCH} $CFLAGS -c ${name}
}

function compile_lib {
    local name=$1
    local objectfile_prefix=$2
    local libsuffix=".a"
    local libprefix="lib"
    if [ "$(uname)" == "Windows" ]; then
        # completely untested
        libsuffix=".lib"
    fi
    local targetlib=${libprefix}${name}${libsuffix}
    echo "Creating library $basename"
    run_cmd ${AR} rcs ${BUILD_DIR}/${targetlib} ${BUILD_DIR}/${objectfile_prefix}*.o
}


function link_exe {
    local name=$1
    shift
    local libraries=$*
    local exesuffix=""
    local libflag="-l"
    if [ "$(uname)" == "Windows" ]; then
        # completely untested
        exesuffix=".exe"
    fi
    local target=${name}${exesuffix}
    echo "Linking executable ${target}"

    local libraries=""
    for library in $*
    do
        libraries="${libraries} ${libflag}${library}"
    done

    run_cmd ${LD} -o ${BUILD_DIR}/${target} ${OPT} ${LDFLAGS} ${ARCH} ${libraries}
}
