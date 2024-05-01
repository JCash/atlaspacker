#! /usr/bin/env bash

set -e

mkdir -p ./external/repos

if [ ! -d ./external/repos/lua ]; then
    echo "Cloning Lua"
    (cd ./external/repos && git clone https://github.com/lua/lua.git)
fi

if [ ! -d ./external/repos/sokol ]; then
    echo "Cloning Sokol"
    (cd ./external/repos && git clone https://github.com/floooh/sokol.git)
fi

if [ ! -d ./external/repos/cimgui ]; then
    echo "Cloning cimgui"
    (cd ./external/repos && git clone --recursive https://github.com/cimgui/cimgui.git)
fi

if [ ! -d ./external/repos/stb ]; then
    echo "Cloning stb"
    (cd ./external/repos && git clone https://github.com/nothings/stb.git)
fi
