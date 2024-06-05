list_subdirectories(BOARD_PUBINC_1 ${BOARD_PATH}/tuya_open_sdk/tuyaos_adapter)
set(BOARD_PUBINC_2 ${BOARD_PATH}/esp-idf/components/mbedtls/mbedtls/include)

set(BOARD_PUBINC 
    ${BOARD_PUBINC_1}
    ${BOARD_PUBINC_2})