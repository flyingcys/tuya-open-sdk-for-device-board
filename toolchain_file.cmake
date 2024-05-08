
# set target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# set toolchain
message(STATUS "[TOP] BOARD_PATH: ${BOARD_PATH}")

set(TOOLCHAIN_DIR "${BOARD_PATH}/gcc-arm-none-eabi-10.3-2021.10")
set(TOOLCHAIN_PRE "arm-none-eabi-")

set(CMAKE_AR "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}ar")
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/${TOOLCHAIN_PRE}g++")

SET (CMAKE_C_COMPILER_WORKS 1)
SET (CMAKE_CXX_COMPILER_WORKS 1)

# set CFLAGS
# -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=deprecated-declarations
set(CMAKE_C_FLAGS "-mcpu=cortex-m0plus -mthumb -O3 -DNDEBUG -Wall -Wno-format -Wno-unused-function -Wno-maybe-uninitialized -ffunction-sections -fdata-sections -std=gnu11")
