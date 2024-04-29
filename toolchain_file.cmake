
# 设置目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# 设置工具链目录
set(TOOLCHAIN_DIR "${BOARD_PATH}/.espressif/tools/xtensa-esp-elf/esp-13.2.0_20230928/xtensa-esp-elf")
set(TOOLCHAIN_PRE "xtensa-esp-elf-")

message(STATUS "[TOP] BOARD_PATH: ${BOARD_PATH}")

set(CMAKE_AR "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}ar")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}g++")

SET (CMAKE_C_COMPILER_WORKS 1)
SET (CMAKE_CXX_COMPILER_WORKS 1)

# 设置Cmake查找主路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/bin)


# 设置CFLAGS
# -Werror=all -Werror
set(CMAKE_C_FLAGS "-mlongcalls -Wno-frame-address -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-bzero -fno-builtin-stpcpy -fno-builtin-strncpy -Wall -Wextra -Wwrite-strings -Wformat=2 -Wno-format-nonliteral -Wvla -Wlogical-op -Wshadow -Wformat-signedness -Wformat-overflow=2 -Wformat-truncation -Wmissing-declarations -Wmissing-prototypes -ffunction-sections -fdata-sections -Wall -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-enum-conversion -gdwarf-4 -ggdb -Og -fno-shrink-wrap -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -Wno-old-style-declaration")
