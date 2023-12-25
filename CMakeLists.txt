##
# @file CMakeLists.txt
# @brief 
#/

# LIB_SRCS
file(GLOB_RECURSE LIB_SRCS "${BOARD_PATH}/tuyaos_adapter/*.c")

# LIB_PUBLIC_INC
list_subdirectories(${BOARD_PATH}/tuyaos_adapter LIB_PUBLIC_INC)

########################################
# Target Configure
########################################
add_library(${BOARD_LIB})

target_sources(${BOARD_LIB}
    PRIVATE
        ${LIB_SRCS}
    )
    
target_include_directories(${BOARD_LIB}
    PUBLIC
        ${LIB_PUBLIC_INC}
    )

