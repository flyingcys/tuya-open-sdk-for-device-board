idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

set(TUYA_OPENSDK_LIB_PATH $ENV{TUYAOS_LIBS_DIR})

# 将值以空格为分隔符分割成单独的值
string(REPLACE " " ";" TUYAOS_LIBS_LIST $ENV{TUYAOS_LIBS})

# 输出每个单独的值
foreach(lib ${TUYAOS_LIBS_LIST})
    message("TUYAOS_LIBS value: ${lib}")
    add_prebuilt_library(${lib} "${TUYA_OPENSDK_LIB_PATH}/lib${lib}.a" REQUIRES tuyaos_adapter)
    target_link_libraries(${COMPONENT_LIB} PUBLIC ${lib})
endforeach()

