cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(tuya-open-sdk C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(tuya-open-sdk
    main.c
)

pico_add_extra_outputs(tuya-open-sdk)
target_link_libraries(tuya-open-sdk pico_stdlib)