##
# @file board.cmake
# @brief 
#/

# 设置目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# 设置工具链目录
set(TOOLCHAIN_DIR "${BOARD_PATH}/toolchain/gcc-arm-none-eabi-4_9-2015q1")
set(TOOLCHAIN_PRE "arm-none-eabi-")

# set(TOOLCHAIN_INCLUDE
#     ${TOOLCHAIN_DIR}/include
#     )
# set(TOOLCHAIN_LIB
#     ${TOOLCHAIN_DIR}/lib/gcc
#     )

message(STATUS "[TOP] BOARD_PATH: ${BOARD_PATH}")

# 设置编译器位置
set(CMAKE_AR "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}ar")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}g++")

SET (CMAKE_C_COMPILER_WORKS 1)
SET (CMAKE_CXX_COMPILER_WORKS 1)

# 设置Cmake查找主路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/bin)

# set(CMAKE_INCLUDE_PATH
#     ${TOOLCHAIN_INCLUDE}
#     )
# set(CMAKE_LIBRARY_PATH
#     ${TOOLCHAIN_LIB}
#     )

# 设置CFLAGS
set(CMAKE_C_FLAGS "-g -mthumb -mcpu=arm968e-s -march=armv5te -mthumb-interwork -mlittle-endian -Os -std=c99 -ffunction-sections -Wall -fsigned-char -fdata-sections -Wunknown-pragmas -nostdlib -Wno-unused-function -Wno-unused-but-set-variable -Wno-format")


# LIB_PUBLIC_INC
# execute_process(
#     COMMAND find ${BOARD_PATH}/tuyaos/tuyaos_adapter -type d
#     OUTPUT_VARIABLE BOARD_PUBINC
# )

# string(REGEX REPLACE "\n" ";" BOARD_PUBINC "${BOARD_PUBINC}")
