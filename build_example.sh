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
TARGET=$8

echo EXAMPLE_NAME=$EXAMPLE_NAME
echo EXAMPLE_VER=$EXAMPLE_VER
echo HEADER_DIR=$HEADER_DIR
echo LIBS_DIR=$LIBS_DIR
echo LIBS=$LIBS
echo OUTPUT_DIR=$OUTPUT_DIR
echo USER_CMD=$USER_CMD
echo TARGET=$TARGET

export TUYAOS_HEADER_DIR=$HEADER_DIR
export TUYAOS_LIBS_DIR=$LIBS_DIR
export TUYAOS_LIBS=$LIBS

if [ "$USER_CMD" = "build" ]; then
    USER_CMD=all
fi

TOP_DIR=$(pwd)

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

cd ${IDF_PATH}
. export.sh
cd -

echo "Build Target: $TARGET"

cd tuyaos
if [ "${USER_CMD}" = "clean" ]; then
    idf.py clean
    rm -rf .target
    exit 0
elif [ "${USER_CMD}" = "menuconfig" ]; then
    idf.py menuconfig
    exit 0
fi

if [ -f ${TOP_DIR}/.target ]; then
    OLD_TARGET=$(cat ${TOP_DIR}/.target)
    echo OLD_TARGET: ${OLD_TARGET}
fi

if [ ! -f ${TOP_DIR}/.target ] || [ x"${TARGET}" != x"${OLD_TARGET}" ] ; then
    echo "set-target ${TARGET}"
    idf.py set-target ${TARGET}
fi

echo ${TARGET} > ${TOP_DIR}/.target

idf.py build


exit 0