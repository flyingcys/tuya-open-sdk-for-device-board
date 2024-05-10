cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(tuya-open-sdk C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(tuyaos_adapter)

add_executable(tuya-open-sdk
    main.c
)

set(include_dirs ".")

# Append TUYAOS_HEADER_DIR to include_dirs
string(REPLACE " " ";" TUYAOS_HEADER_DIR_LIST $ENV{TUYAOS_HEADER_DIR})
foreach(head ${TUYAOS_HEADER_DIR_LIST})
    list(APPEND include_dirs ${head})
endforeach()

# Append TUYAOS_LIBS to target
string(REPLACE " " ";" TUYAOS_LIBS_LIST $ENV{TUYAOS_LIBS})

set(TUYAOS_LIB "")
foreach(lib ${TUYAOS_LIBS_LIST})
    message("TUYAOS_LIBS value: ${lib}")
    list(APPEND TUYAOS_LIB "$ENV{TUYAOS_LIBS_DIR}/lib${lib}.a")
endforeach()

pico_add_extra_outputs(tuya-open-sdk)
target_link_libraries(tuya-open-sdk pico_stdlib ${TUYAOS_LIB} tuyaos_adapter)
target_include_directories(tuya-open-sdk PUBLIC ${include_dirs})