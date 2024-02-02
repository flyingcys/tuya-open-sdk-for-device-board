#!/usr/bin/sh
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

APP_BIN_NAME=$EXAMPLE_NAME
APP_VERSION=$EXAMPLE_VER

if [ "$USER_CMD" = "build" ]; then
    USER_CMD=all
fi

echo APP_BIN_NAME=$APP_BIN_NAME
echo APP_VERSION=$EXAMPLE_VER
echo USER_CMD=$USER_CMD
echo LIBS_DIR=$LIBS_DIR
echo LIBS=$LIBS
echo OUTPUT_DIR=$OUTPUT_DIR
echo HEADER_DIR=$HEADER_DIR

echo "Start Compile"
set -e

if type python3 >/dev/null 2>&1; then
	PYTHON=python3
elif type python2 >/dev/null 2>&1; then
	PYTHON=python2
else
	echo "#####################################################"
	echo "Error:"
	echo "Please run the following command to install the dependent libraries:"
	echo "sudo apt install -y python3"
	echo "or"
	echo "sudo apt install -y python2"
	echo "#####################################################"
	exit 1;
fi

if ! type make >/dev/null 2>&1; then
	echo "#####################################################"
	echo "Error:"
	echo "Please run the following command to install the dependent libraries:"
	echo "sudo apt install -y build-essential"
	echo "#####################################################"
	exit 1;
fi

TOOLCHAIN=toolchain/gcc-arm-none-eabi-4_9-2015q1/bin
export ARM_GCC_TOOLCHAIN=${TOOLCHAIN}

GCC_VERSION=$(${TOOLCHAIN}/arm-none-eabi-gcc --version)
echo ${GCC_VERSION}
if [ "x${GCC_VERSION}" = "x" ]; then
	echo "#####################################################"
	echo "Error:"
	echo "Please run the following command to install the dependent libraries:"
	echo "sudo apt install -y libc6-i386"
	echo "#####################################################"
	exit 1
fi

SYSTEM=`uname -s`
echo "system:"$SYSTEM
if [ $SYSTEM = "Linux" ]; then
	TOOL_DIR=package_tool/linux
	OTAFIX=${TOOL_DIR}/otafix
	ENCRYPT=${TOOL_DIR}/encrypt
	BEKEN_PACK=${TOOL_DIR}/beken_packager
	RT_OTA_PACK_TOOL=${TOOL_DIR}/rt_ota_packaging_tool_cli
	TY_PACKAGE=${TOOL_DIR}/package
else
	TOOL_DIR=package_tool/windows
	OTAFIX=${TOOL_DIR}/otafix.exe
	ENCRYPT=${TOOL_DIR}/encrypt.exe
	BEKEN_PACK=${TOOL_DIR}/beken_packager.exe
	RT_OTA_PACK_TOOL=${TOOL_DIR}/rt_ota_packaging_tool_cli.exe
	TY_PACKAGE=${TOOL_DIR}/package.exe
fi

TYUTOOL_DIR=$(pwd)/tools/tyutool

cd beken_os

if [ "$USER_CMD" = "clean" ];then
	# make clean
	make APP_BIN_NAME=$APP_BIN_NAME APP_VERSION=$APP_VERSION LIBS_DIR=$LIBS_DIR LIBS="$LIBS"  OUTPUT_DIR=$OUTPUT_DIR  HEADER_DIR="$HEADER_DIR" $USER_CMD -j -C ./
	echo "*************************************************************************"
	echo "************************CLEAN SUCCESS************************************"
	echo "*************************************************************************"
	exit 0
fi

make APP_BIN_NAME=$APP_BIN_NAME APP_VERSION=$APP_VERSION LIBS_DIR=$LIBS_DIR LIBS="$LIBS" OUTPUT_DIR=$OUTPUT_DIR  HEADER_DIR="$HEADER_DIR" $USER_CMD -j -C ./

APP_BIN_DIR=${OUTPUT_DIR}/${APP_BIN_NAME}_${APP_VERSION}

echo "Start Combined"
cp ${APP_BIN_DIR}/${APP_BIN_NAME}_${APP_VERSION}.bin tools/generate/

cd tools/generate/
./${ENCRYPT} ${APP_BIN_NAME}_${APP_VERSION}.bin 510fb093 a3cbeadc 5993a17e c7adeb03 10000
${PYTHON} mpytools.py t2_bootloader_enc.bin ${APP_BIN_NAME}_${APP_VERSION}_enc.bin

./${BEKEN_PACK} config.json

echo "End Combined"
cp all_1.00.bin ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin
rm all_1.00.bin

cp ${APP_BIN_NAME}_${APP_VERSION}_enc_uart_1.00.bin ${APP_BIN_NAME}_UA_${APP_VERSION}.bin
rm ${APP_BIN_NAME}_${APP_VERSION}_enc_uart_1.00.bin

#generate ota file
echo "generate ota file"
./${RT_OTA_PACK_TOOL} -f ${APP_BIN_NAME}_${APP_VERSION}.bin -v $CURRENT_TIME -o ${APP_BIN_NAME}_${APP_VERSION}.rbl -p app -c gzip -s aes -k 0123456789ABCDEF0123456789ABCDEF -i 0123456789ABCDEF
./${TY_PACKAGE} ${APP_BIN_NAME}_${APP_VERSION}.rbl ${APP_BIN_NAME}_UG_${APP_VERSION}.bin $APP_VERSION 
rm ${APP_BIN_NAME}_${APP_VERSION}.rbl
rm ${APP_BIN_NAME}_${APP_VERSION}.bin
rm ${APP_BIN_NAME}_${APP_VERSION}.cpr
rm ${APP_BIN_NAME}_${APP_VERSION}.out
rm ${APP_BIN_NAME}_${APP_VERSION}_enc.bin

echo "ug_file size:"
ls -l ${APP_BIN_NAME}_UG_${APP_VERSION}.bin | awk '{print $5}'
if [ `ls -l ${APP_BIN_NAME}_UG_${APP_VERSION}.bin | awk '{print $5}'` -gt 679936 ];then
	echo "**********************${APP_BIN_NAME}_$APP_VERSION.bin***************"
	echo "************************** too large ********************************"
	rm ${APP_BIN_NAME}_UG_${APP_VERSION}.bin
	rm ${APP_BIN_NAME}_UA_${APP_VERSION}.bin
	rm ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin
	exit 1
fi

if [ -f combine.sh ]; then
	# TOTAL_ADDR=$((16#200000))
	# START_ADDR=$((16#0))
	# ATE_ADDR=$((16#132000))

 #    bash combine.sh QIO_no_ate $TOTAL_ADDR ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin $START_ADDR t2_ate $ATE_ADDR
	bash combine.sh QIO_no_ate 2097152 ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin 0 t2_ate 1253376
	cp QIO_no_ate ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin
	rm QIO_no_ate
fi

# echo "$(pwd)"
cp ${APP_BIN_NAME}_UG_${APP_VERSION}.bin  ${APP_BIN_DIR}/${APP_BIN_NAME}_UG_${APP_VERSION}.bin
cp ${APP_BIN_NAME}_UA_${APP_VERSION}.bin  ${APP_BIN_DIR}/${APP_BIN_NAME}_UA_${APP_VERSION}.bin
cp ${APP_BIN_NAME}_QIO_${APP_VERSION}.bin ${APP_BIN_DIR}/${APP_BIN_NAME}_QIO_${APP_VERSION}.bin

echo ""

echo "###################################################################################################################"
echo "Project ${APP_BIN_NAME} build complete. To flash, run this command:"
if [ $SYSTEM = "Linux" ]; then
	echo "${TYUTOOL_DIR}/cli write -d T2 -p [Port] -b [Baudrate] -f ${APP_BIN_DIR}/${APP_BIN_NAME}_QIO_${APP_VERSION}.bin"
	echo "Port: /dev/ttyACM0 or /dev/ttyUSB0 ..."
else
	echo "${TYUTOOL_DIR}/cli.exe write -d T2 -p [Port] -b [Baudrate] -f ${APP_BIN_DIR}/${APP_BIN_NAME}_QIO_${APP_VERSION}.bin"
	echo "Port: COM3 or COM4 ..."
fi
echo "Baudrate: 921600 or 1500000 or 2000000 ..."
echo ""
echo "Flash tool user manual at ${TYUTOOL_DIR}/README.md"
echo "###################################################################################################################"
echo ""
