set(TUYAOS_ADAPTER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SOURCES "*.c")

set(include_dirs
    "include/adc"
    "include/flash"
    "include/gpio"
    "include/system"
    "include/utilities/include"
)

message(STATUS "=================SOURCES: ${SOURCES}")

# Append TUYAOS_HEADER_DIR to include_dirs
string(REPLACE " " ";" TUYAOS_HEADER_DIR_LIST $ENV{TUYAOS_HEADER_DIR})
foreach(head ${TUYAOS_HEADER_DIR_LIST})
    list(APPEND include_dirs ${head})
endforeach()

add_library(tuyaos_adapter STATIC ${SOURCES})
target_include_directories(tuyaos_adapter PRIVATE ${include_dirs})