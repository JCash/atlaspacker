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

if [ ! -d ./external/repos/stb ]; then
    echo "Cloning stb"
    (cd ./external/repos && git clone https://github.com/nothings/stb.git)
fi

if [ ! -d ./external/repos/imgui ]; then
    echo "Cloning Dear ImGui"
    (cd ./external/repos && git clone --recursive git@github.com:ocornut/imgui.git)
fi
