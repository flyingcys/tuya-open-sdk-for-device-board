#!/usr/bin/env bash

set -e

print_not_null()
{
    # $1 为空，返回错误
    if [ x"$1" = x"" ]; then
        return 1
    fi

    echo "$1"
}

TARGET=$1
echo TARGET=$TARGET

TOP_DIR=$(pwd)
echo $TOP_DIR

echo "Start board prepare ..."

IDF_PATH=${TOP_DIR}/esp-idf
IDF_TOOLS_PATH=${TOP_DIR}/.espressif
export IDF_PATH
export IDF_TOOLS_PATH
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

rm -rf .target

rm -rf toolchain_file.cmake

if [ "$TARGET" = "esp32" ]; then
    ln -s toolchain_esp32.cmake toolchain_file.cmake
elif [ "$TARGET" = "esp32s2" ]; then
    ln -s toolchain_esp32s2.cmake toolchain_file.cmake
elif [ "$TARGET" = "esp32s3" ]; then
    ln -s toolchain_esp32s3.cmake toolchain_file.cmake
elif [ "$TARGET" = "esp32c2" ] || [ "$TARGET" = "esp32c3"]; then
    ln -s toolchain_esp32c3.cmake toolchain_file.cmake
elif [ "$TARGET" = "esp32c6" ]; then
    ln -s toolchain_esp32c6.cmake toolchain_file.cmake
else
    echo "TARGET is empty ..."
    exit 1
fi

echo "Run board prepare success ..."
