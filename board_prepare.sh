#!/usr/bin/env bash
set -e

TOP_DIR=$(pwd)

echo "Start board prepare ..."

if [ ! -d ${OPENSDK_ESPIDF_PATH} ]; then
    echo "IDF_PATH is empty"
    git clone --recursive https://github.com/espressif/esp-idf -b v5.2.1 --depth=1
    if [ $? -ne 0]; then
        echo "git clone esp-idf failed ..."
        exit 1
    else
        pushd ${OPENSDK_ESPIDF_PATH}
        git submodule update --init --recursive
        . install.sh
        popd
        echo "git clone esp-idf success ..."
    fi
fi

if [ ! -d ${IDF_TOOLS_PATH} ];then
    echo "IDF_TOOLS_PATH is empty ..."
    pushd ${OPENSDK_ESPIDF_PATH}
    git submodule update --init --recursive
    . install.sh
    popd
fi

echo "Run board prepare success ..."
