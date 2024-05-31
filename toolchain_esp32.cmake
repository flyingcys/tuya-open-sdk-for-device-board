
# set target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# set toolchain
message(STATUS "[TOP] BOARD_PATH: ${BOARD_PATH}")

set(TOOLCHAIN_DIR "${BOARD_PATH}/.espressif/tools/xtensa-esp-elf/esp-13.2.0_20230928/xtensa-esp-elf/bin")
set(TOOLCHAIN_PRE "xtensa-esp32-elf-")

set(CMAKE_AR "${TOOLCHAIN_DIR}/${TOOLCHAIN_PRE}ar")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/${TOOLCHAIN_PRE}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/${TOOLCHAIN_PRE}g++")

SET (CMAKE_C_COMPILER_WORKS 1)
SET (CMAKE_CXX_COMPILER_WORKS 1)

# set CFLAGS
# -Werror=all 
set(CMAKE_C_FLAGS "-std=c99 -mlongcalls -Wno-frame-address -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-enum-conversion -gdwarf-4 -ggdb -Og -fno-shrink-wrap -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -std=gnu17 -Wno-old-style-declaration -Wno-address -Wno-unused-function -Wformat-overflow=2 -Wno-unused-variable -Wno-=unused-but-set-variable -Wno-deprecated-declarations")
