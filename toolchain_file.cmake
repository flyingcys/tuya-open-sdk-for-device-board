
# 设置目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# 设置工具链目录
set(TOOLCHAIN_DIR "/home/cys/.espressif/tools/xtensa-esp-elf/esp-13.2.0_20230928/xtensa-esp-elf")
set(TOOLCHAIN_PRE "xtensa-esp32-elf-")

message(STATUS "[TOP] BOARD_PATH: ${BOARD_PATH}")

set(CMAKE_AR "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}ar")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}g++")

SET (CMAKE_C_COMPILER_WORKS 1)
SET (CMAKE_CXX_COMPILER_WORKS 1)

# 设置Cmake查找主路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/bin)


# 设置CFLAGS
# -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=deprecated-declarations
set(CMAKE_C_FLAGS "-mlongcalls -Wno-frame-address -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-enum-conversion -gdwarf-4 -ggdb -Og -fno-shrink-wrap -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -std=gnu17 -Wno-old-style-declaration -Wno-address")
