list_subdirectories(BOARD_PUBINC_1 ${BOARD_PATH}/tuyaos/tuyaos_adapter)
set(BOARD_PUBINC_2 ${BOARD_PATH}/esp-idf/components/mbedtls/mbedtls/include)
list(APPEND BOARD_PUBINC_2 ${BOARD_PATH}/esp-idf/components/mbedtls/port/include)
list(APPEND BOARD_PUBINC_2 ${BOARD_PATH}/tuyaos/build/config)
list(APPEND BOARD_PUBINC_2 ${BOARD_PATH}/esp-idf/components/soc/esp32/include)

set(BOARD_PUBINC 
    ${BOARD_PUBINC_1}
    ${BOARD_PUBINC_2})