#!/bin/bash

# set -e

TOOLCHAIN_NAME=gcc-arm-none-eabi-4_9-2015q1

restult=$(curl -s http://www.ip-api.com/json)
country=$(echo $restult | sed 's/.*"country":"\([^"]*\)".*/\1/')
echo "country: $country"

os_type="$(uname)"
if [ "$os_type" = "Linux" ]; then
    echo "OS: Linux"
    
    if [ $country = "China" ]; then
        TOOLCHAIN_URL=https://images.tuyacn.com/rms-static/4720f5e0-a2ca-11ee-8cd8-b117287658f4-1703470015550.tar.bz2?tyName=gcc-arm-none-eabi-4_9-2015q1-20150306-linux.tar.bz2
    else
        TOOLCHAIN_URL=https://github.com/tuya/T2/releases/download/0.0.1/gcc-arm-none-eabi-4_9-2015q1-20150306-linux.tar.bz2
    fi
    TOOLCHAIN_FILE=gcc-arm-none-eabi-4_9-2015q1.tar.bz2

elif [ "${os_type:0:9}" = "CYGWIN_NT" ] || [ "${os_type:0:10}" = "MINGW64_NT" ]; then
    echo "OS: $os_type"

    if [ $country = "China" ]; then
        TOOLCHAIN_URL=https://images.tuyacn.com/rms-static/47233fd0-a2ca-11ee-af19-cfa45f6de59e-1703470015565.zip?tyName=gcc-arm-none-eabi-4_9-2015q1-20150306-win32.zip
    else
        TOOLCHAIN_URL=https://github.com/tuya/T2/releases/download/0.0.1/gcc-arm-none-eabi-4_9-2015q1-20150306-win32.zip
    fi
    TOOLCHAIN_FILE=gcc-arm-none-eabi-4_9-2015q1.zip
else
    echo "OS: $os_type not support"
    exit 1
fi

FILE_EXTENSION=${TOOLCHAIN_FILE#*.}

cd toolchain
if [ -d "$TOOLCHAIN_NAME" ]; then
    echo "Toolchain $TOOLCHAIN_NAME check Successful"
else
    wget $TOOLCHAIN_URL -O $TOOLCHAIN_FILE
    echo "start decompression"
    if [ $FILE_EXTENSION = "tar.bz2" ]; then
        tar -xvf $TOOLCHAIN_FILE
    elif [ $FILE_EXTENSION = "zip" ]; then
        mkdir -p $TOOLCHAIN_NAME
        unzip $TOOLCHAIN_FILE -d $TOOLCHAIN_NAME
    else
        echo "File not support"
    fi
fi

rm -rf $TOOLCHAIN_FILE

