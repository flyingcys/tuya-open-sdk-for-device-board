#!/usr/bin/env bash
# 参数说明：
# $1 - example name: echo_app_top
# $2 - example version: 1.0.0
# $3 - header files directory:
# $4 - libs directory:
# $5 - libs: tuyaos tuyaapp
# $6 - output directory:

print_not_null()
{
    # $1 为空，返回错误
    if [ x"$1" = x"" ]; then
        return 1
    fi

    echo "$1"
}

set -e
cd `dirname $0`

EXAMPLE_NAME=$1
EXAMPLE_VER=$2
HEADER_DIR=$3
LIBS_DIR=$4
LIBS=$5
OUTPUT_DIR=$6
USER_CMD=$7

echo EXAMPLE_NAME=$EXAMPLE_NAME
echo EXAMPLE_VER=$EXAMPLE_VER
echo HEADER_DIR=$HEADER_DIR
echo LIBS_DIR=$LIBS_DIR
echo LIBS=$LIBS
echo OUTPUT_DIR=$OUTPUT_DIR
echo USER_CMD=$USER_CMD

export TUYAOS_HEADER_DIR=$HEADER_DIR
export TUYAOS_LIBS_DIR=$LIBS_DIR
export TUYAOS_LIBS=$LIBS

if [ "$USER_CMD" = "build" ]; then
    USER_CMD=all
fi

TOP_DIR=$(pwd)
PICO_SDK_PATH =${TOP_DIR}/pico-sdk
export PICO_SDK_PATH

TOOLCHAIN_NAME=gcc-arm-none-eabi-10.3-2021.10
PICO_TOOLCHAIN=${TOP_DIR}/${TOOLCHAIN_NAME}
echo "PICO_TOOLCHAIN=$PICO_TOOLCHAIN"
export PATH=$PATH:${PICO_TOOLCHAIN}

cd tuya_open_sdk
if [ "${USER_CMD}" = "clean" ]; then
    make clean
    exit 0

fi

if [ ! -f ${TOP_DIR}/.target ] || [ x"${TARGET}" != x"${OLD_TARGET}" ] ; then
    echo "set-target ${TARGET}"
    idf.py set-target ${TARGET}
fi

echo ${TARGET} > ${TOP_DIR}/.target

idf.py build


exit 0