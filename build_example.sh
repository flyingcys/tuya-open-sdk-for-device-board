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

. board_prepare.sh 

export IDF_TOOLS_PATH=${TOP_DIR}/.espressif

cd ${IDF_PATH}
. export.sh
cd -



echo "Build Target: $TARGET"

cd tuyaos
if [ "${USER_CMD}" = "clean" ]; then
    idf.py fullclean
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
    idf.py clean
    idf.py fullclean
    echo "================set-target ${TARGET}"
    idf.py set-target ${TARGET}
fi

if [ "${USER_CMD}" = "all" ]; then
    idf.py build
    idf.py menuconfig
    idf.py size-files
    idf.py app-size
    idf.py flash
    idf.py monitor
    idf.py erase_flash
    idf.py flash_idf_monitor
    idf.py flash_partitions
    idf.py set-target ${TARGET}
fi

idf.py build

echo ${TARGET} > ${TOP_DIR}/.target

exit 0