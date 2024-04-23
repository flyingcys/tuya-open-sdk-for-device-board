#!/usr/bin/env bash

set -e

OPENSDK_ESPIDF_PATH=esp-idf
if [ -n "$IDF_PATH" ]; then
    echo "IDF_PATH is exist ${IDF_PATH}"
    echo $IDF_PATH
    OPENSDK_ESPIDF_PATH=$IDF_PATH
else
    if [ ! -d ${OPENSDK_ESPIDF_PATH} ]; then
        echo "IDF_PATH is empty"
        git clone https://github.com/espressif/esp-idf --depth=1
        if [ $? -ne 0]; then
            echo "git clone esp-idf failed ..."
            exit 1
        else
            cd esp-idf && git submodule update --init --recursive
            bash ${OPENSDK_ESPIDF_PATH}/install.sh
        fi
    fi
fi

echo ${OPENSDK_ESPIDF_PATH}

. ${OPENSDK_ESPIDF_PATH}/export.sh

cd tuyaos

idf.py build

