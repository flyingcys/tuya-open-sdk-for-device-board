#!/usr/bin/env bash
set -e

TOP_DIR=$(pwd)

echo $TOP_DIR
export IDF_PATH=${TOP_DIR}/esp-idf
# export IDF_TOOLS_PATH=${TOP_DIR}/.espressif

echo "Start board prepare ..."

if [ ! -d ${IDF_PATH} ]; then
    echo "IDF_PATH is empty"
    git clone --recursive https://github.com/espressif/esp-idf -b v5.2.1 --depth=1
    if [ $? -ne 0 ]; then
        echo "git clone esp-idf failed ..."
        exit 1
    else
        cd ${IDF_PATH}
        git submodule update --init --recursive
        . ./install.sh
        cd -
        
        echo "git clone esp-idf success ..."
    fi
fi

if [ ! -d ${IDF_TOOLS_PATH} ];then
    echo "IDF_TOOLS_PATH is empty ..."
    cd ${IDF_PATH}
    git submodule update --init --recursive
    . ./install.sh
    cd -
fi

echo "Run board prepare success ..."
