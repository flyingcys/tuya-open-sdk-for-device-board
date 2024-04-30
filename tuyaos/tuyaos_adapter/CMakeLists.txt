set(TUYAOS_ADAPTER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "TUYAOS_ADAPTER_DIR: ${TUYAOS_ADAPTER_DIR}")

file(GLOB_RECURSE SOURCES "*.c")

# 获取TUYAOS_HEADER_DIR环境变量的值
set(include_dirs
    "include/adc"
    "include/flash"
    "include/gpio"
    "include/system"
    "include/utilities/include"
    )

# 将值以空格为分隔符分割成单独的值
string(REPLACE " " ";" TUYAOS_HEADER_DIR_LIST $ENV{TUYAOS_HEADER_DIR})

# 输出每个单独的值
foreach(head ${TUYAOS_HEADER_DIR_LIST})
    # message("TUYAOS_HEAD value: ${head}")
    list(APPEND include_dirs ${head})
endforeach()

idf_component_register(SRCS "${SOURCES}"
                    INCLUDE_DIRS "${include_dirs}" PRIV_REQUIRES nvs_flash bt esp_pm esp_adc efuse)