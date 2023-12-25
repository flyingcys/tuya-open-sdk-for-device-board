##
# @file board.cmake
# @brief 
#/

# 设置目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR Linux)

# 设置工具链目录
set(TOOLCHAIN_DIR "/usr")
set(TOOLCHAIN_INCLUDE
    ${TOOLCHAIN_DIR}/include
    )
set(TOOLCHAIN_LIB
    ${TOOLCHAIN_DIR}/lib/gcc
    )

# 设置编译器位置
set(CMAKE_C_COMPILER "${TOOLCHAIN_DIR}/bin/gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_DIR}/bin/g++")

# 设置Cmake查找主路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR})
set(CMAKE_INCLUDE_PATH
    ${TOOLCHAIN_INCLUDE}
    )
set(CMAKE_LIBRARY_PATH
    ${TOOLCHAIN_LIB}
    )

# 设置CFLAGS
set(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g")
