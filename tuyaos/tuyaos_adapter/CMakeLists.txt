set(TUYAOS_ADAPTER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "TUYAOS_ADAPTER_DIR: ${TUYAOS_ADAPTER_DIR}")

file(GLOB_RECURSE SOURCES "*.c")

set(include_dirs
    "include/adc"
    "include/flash"
    "include/gpio"
    "include/system"
    "include/utilities/include"
    )

add_library(tuyaos_adapter ${SOURCES})
