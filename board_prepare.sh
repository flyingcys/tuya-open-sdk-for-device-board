#!/usr/bin/env bash

set -e

TOP_DIR=$(pwd)
echo $TOP_DIR

echo "Start board prepare ..."

TOOLCHAIN_NAME=gcc-arm-none-eabi-10.3-2021.10

PICO_SDK_PATH=${TOP_DIR}/pico-sdk
export PICO_SDK_PATH=${PICO_SDK_PATH}

if [ ! -d ${PICO_SDK_PATH} ]; then
    echo "PICO_SDK_PATH is empty"
    git clone --recursive https://github.com/raspberrypi/pico-sdk -b 1.5.1 --depth=1
    if [ $? -ne 0 ]; then
        echo "git clone pico-sdk failed ..."
        exit 1
    fi
    echo "git clone pico-sdk success ..."
fi

cd ${PICO_SDK_PATH}
git submodule update --init
cd -

FREERTOS_KERNEL_PATH=${TOP_DIR}/FreeRTOS-Kernel
if [ ! -d "${FREERTOS_KERNEL_PATH}" ]; then
    git clone https://github.com/FreeRTOS/FreeRTOS-Kernel -b smp --depth=1
    if [ $? -ne 0 ]; then
        echo "git clone FreeRTOS-Kernel failed ..."
        exit 1
    fi
fi

if [ -d "$TOOLCHAIN_NAME" ]; then
    if [ -n "$(ls -A $TOOLCHAIN_NAME)" ]; then
        echo "Toolchain $TOOLCHAIN_NAME check Successful"
        echo "Run board prepare success ..."
        exit 0
    fi
fi

echo "Start download toolchain"
# restult=$(curl -m 15 -s http://www.ip-api.com/json)
# country=$(echo $restult | sed 's/.*"country":"\([^"]*\)".*/\1/')
# echo "country: $country"

HOST_MACHINE=$(uname -m)

case "$(uname -s)" in
Linux*)
	SYSTEM_NAME="Linux"
    if [ $HOST_MACHINE = "x86_64" ]; then
        TOOLCHAIN_URL=https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
        TOOLCHAIN_FILE=gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
    elif [ $HOST_MACHINE = "aarch64" ]; then
        TOOLCHAIN_URL=https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-aarch64-linux.tar.bz2
        TOOLCHAIN_FILE=gcc-arm-none-eabi-10.3-2021.10-aarch64-linux.tar.bz2
    else
        echo "Toolchain not support, Please download toolchain from https://developer.arm.com/downloads/-/gnu-rm"
        exit 1    
    fi
	;;
Darwin*)
	SYSTEM_NAME="Apple"
    if [ $HOST_MACHINE = "x86_64" ]; then
        TOOLCHAIN_URL=https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-mac.tar.bz2
        TOOLCHAIN_FILE=gcc-arm-none-eabi-10.3-2021.10-mac.tar.bz2
    else
        echo "Toolchain not support, Please download toolchain from https://developer.arm.com/downloads/-/gnu-rm"
        exit 1
    fi
	;;
MINGW* | CYGWIN* | MSYS*)
	SYSTEM_NAME="Windows"
    TOOLCHAIN_URL=https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-win32.zip
    TOOLCHAIN_FILE=gcc-arm-none-eabi-10.3-2021.10-win32.zip
	;;
*)
	SYSTEM_NAME="Unknown"
    exit 1
	;;
esac

echo "Running on [$SYSTEM_NAME]"

FILE_EXTENSION=${TOOLCHAIN_FILE##*.}

wget $TOOLCHAIN_URL -O $TOOLCHAIN_FILE
echo "start decompression"
echo "FILE_EXTENSION: ${FILE_EXTENSION}"
if [ $FILE_EXTENSION = "bz2" ]; then
    tar -xvf $TOOLCHAIN_FILE
elif [ $FILE_EXTENSION = "zip" ]; then
    mkdir -p $TOOLCHAIN_NAME
    unzip $TOOLCHAIN_FILE -d $TOOLCHAIN_NAME
else
    echo "File not support"
    exit 1
fi

rm -rf $TOOLCHAIN_FILE

echo "Run board prepare success ..."


