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

string(REPLACE " " ";" TUYAOS_HEADER_DIR_LIST $ENV{TUYAOS_HEADER_DIR})

foreach(head ${TUYAOS_HEADER_DIR_LIST})
    list(APPEND include_dirs ${head})
endforeach()

message(STATUS "TUYAOS_ADAPTER_INCLUDE_DIRS: ${include_dirs}"
)
idf_component_register(SRCS "${SOURCES}"
                    INCLUDE_DIRS "${include_dirs}" REQUIRES mbedtls nvs_flash bt esp_pm esp_adc efuse soc driver app_update)
                    