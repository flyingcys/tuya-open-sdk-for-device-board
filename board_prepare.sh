#!/usr/bin/env bash

set -e

TARGET=$1
echo TARGET=$TARGET
if [ x"$TARGET" = x"" ]; then
    echo "Usage: <./ board_prepare.sh esp32|esp32s2|esp32s3|esp32c2|esp32c3|esp32c6>"
    exit 1
fi

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
rm -rf tuyaos/sdkconfig
rm -rf tuyaos/sdkconfig.old
rm -rf tuyaos/build

if [ x"$TARGET" = x"esp32" ]; then
    ln -s toolchain_esp32.cmake toolchain_file.cmake
elif [ x"$TARGET" = x"esp32s2" ]; then
    ln -s toolchain_esp32s2.cmake toolchain_file.cmake
elif [ x"$TARGET" = x"esp32s3" ]; then
    ln -s toolchain_esp32s3.cmake toolchain_file.cmake
elif [ x"$TARGET" = x"esp32c2" ] || [ x"$TARGET" = x"esp32c3"]; then
    ln -s toolchain_esp32c3.cmake toolchain_file.cmake
elif [ x"$TARGET" = x"esp32c6" ]; then
    ln -s toolchain_esp32c6.cmake toolchain_file.cmake
else
    echo "TARGET is empty ..."
    exit 1
fi

CONTENT="
#ifndef MBEDTLS_THREADING_ALT_H
#define MBEDTLS_THREADING_ALT_H

typedef struct mbedtls_threading_mutex_t {
    void * mutex;
    char is_valid;
} mbedtls_threading_mutex_t;

#endif /* threading_alt.h */
"

# 指定文件名
FILENAME="threading_alt.h"
echo -e "$CONTENT" > "$FILENAME"

cp -f ${FILENAME} ${IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls

echo "Run board prepare success ..."
