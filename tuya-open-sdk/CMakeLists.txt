cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

project(tuya-open-sdk C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(tuyaos_adapter)

add_executable(tuya-open-sdk
    picow_freertos_ping.c
    ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
)

set(include_dirs ".")
list(APPEND include_dirs             
    ${PICO_LWIP_CONTRIB_PATH}/apps/ping
)

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

target_compile_definitions(tuya-open-sdk PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
    LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
    PING_USE_SOCKETS=1
)

target_link_libraries(tuya-open-sdk
    pico_cyw43_arch_lwip_sys_freertos
    pico_stdlib
    pico_lwip_iperf
    FreeRTOS-Kernel-Heap4
    ${TUYAOS_LIB} 
    tuyaos_adapter
    )
target_include_directories(tuya-open-sdk PUBLIC ${include_dirs})

pico_add_extra_outputs(tuya-open-sdk)
