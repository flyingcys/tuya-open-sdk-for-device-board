TOP_DIR = ../../..
TOP_INCLUDE_DIR = $(TOP_DIR)/include

# Initialize tool chain  /usr/bin/gcc-arm-none-eabi-4_9-2015q1/bin
# -------------------------------------------------------------------

CROSS_COMPILE = $(ARM_GCC_TOOLCHAIN)/arm-none-eabi-

CFG_BLE_5X_VERSION ?= 1
CFG_BLE_5X_RW ?= 1
CFG_BLE_5X_USE_RWIP_LIB ?= 1
CFG_GIT_VERSION ?= ""
CFG_WRAP_LIBC  ?= 1

ota_idx = 1

# Compilation tools
AR = $(CROSS_COMPILE)ar
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
NM = $(CROSS_COMPILE)nm
LD = $(CROSS_COMPILE)gcc
GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

Q := @
ifeq ($(V),1)
Q := 
endif
OS := $(shell uname)

ifeq ($(findstring CYGWIN, $(OS)), CYGWIN) 
PICK = $(AMEBA_TOOLDIR)pick.exe
PAD  = $(AMEBA_TOOLDIR)padding.exe
CHKSUM = $(AMEBA_TOOLDIR)checksum.exe
OTA = $(AMEBA_TOOLDIR)ota.exe
else
PICK = $(AMEBA_TOOLDIR)pick
PAD  = $(AMEBA_TOOLDIR)padding
CHKSUM = $(AMEBA_TOOLDIR)checksum
OTA = $(AMEBA_TOOLDIR)ota
endif

ifeq ($(ECHO),)
ECHO=echo
endif

-include .config

# -------------------------------------------------------------------
# Initialize target name and target object files
# -------------------------------------------------------------------

WPA_VERSION := hostapd-2.5
ifeq ($(CFG_USE_WPA_29),1)
WPA_VERSION := wpa_supplicant-2.9
endif

all: application 

TARGET=Debug

OBJ_DIR=$(TARGET)
BIN_DIR=$(TARGET)



# -------------------------------------------------------------------
# Include folder list
# -------------------------------------------------------------------
INCLUDES =

INCLUDES += -I./beken378/common
INCLUDES += -I./beken378/demo
INCLUDES += -I./beken378/app
INCLUDES += -I./beken378/app/config
INCLUDES += -I./beken378/app/standalone-station
INCLUDES += -I./beken378/app/standalone-ap
INCLUDES += -I./beken378/ip/common
INCLUDES += -I./beken378/ip/ke/
INCLUDES += -I./beken378/ip/mac/
INCLUDES += -I./beken378/ip/lmac/src/hal
INCLUDES += -I./beken378/ip/lmac/src/mm
INCLUDES += -I./beken378/ip/lmac/src/ps
INCLUDES += -I./beken378/ip/lmac/src/rd
INCLUDES += -I./beken378/ip/lmac/src/rwnx
INCLUDES += -I./beken378/ip/lmac/src/rx
INCLUDES += -I./beken378/ip/lmac/src/scan
INCLUDES += -I./beken378/ip/lmac/src/sta
INCLUDES += -I./beken378/ip/lmac/src/tx
INCLUDES += -I./beken378/ip/lmac/src/vif
INCLUDES += -I./beken378/ip/lmac/src/rx/rxl
INCLUDES += -I./beken378/ip/lmac/src/tx/txl
INCLUDES += -I./beken378/ip/lmac/src/p2p
INCLUDES += -I./beken378/ip/lmac/src/chan
INCLUDES += -I./beken378/ip/lmac/src/td
INCLUDES += -I./beken378/ip/lmac/src/tpc
INCLUDES += -I./beken378/ip/lmac/src/tdls
INCLUDES += -I./beken378/ip/umac/src/mesh
INCLUDES += -I./beken378/ip/umac/src/mfp
INCLUDES += -I./beken378/ip/umac/src/rc
INCLUDES += -I./beken378/ip/umac/src/apm
INCLUDES += -I./beken378/ip/umac/src/bam
INCLUDES += -I./beken378/ip/umac/src/llc
INCLUDES += -I./beken378/ip/umac/src/me
INCLUDES += -I./beken378/ip/umac/src/rxu
INCLUDES += -I./beken378/ip/umac/src/scanu
INCLUDES += -I./beken378/ip/umac/src/sm
INCLUDES += -I./beken378/ip/umac/src/txu
INCLUDES += -I./beken378/driver/include
INCLUDES += -I./beken378/driver/common/reg
INCLUDES += -I./beken378/driver/entry
INCLUDES += -I./beken378/driver/dma
INCLUDES += -I./beken378/driver/intc
INCLUDES += -I./beken378/driver/phy
INCLUDES += -I./beken378/driver/pwm
INCLUDES += -I./beken378/driver/rc_beken
INCLUDES += -I./beken378/driver/flash
INCLUDES += -I./beken378/driver/rw_pub
INCLUDES += -I./beken378/driver/common/reg
INCLUDES += -I./beken378/driver/common
INCLUDES += -I./beken378/driver/uart
INCLUDES += -I./beken378/driver/sys_ctrl
INCLUDES += -I./beken378/driver/gpio
INCLUDES += -I./beken378/driver/general_dma
INCLUDES += -I./beken378/driver/spidma
INCLUDES += -I./beken378/driver/icu
INCLUDES += -I./beken378/driver/spi
INCLUDES += -I./beken378/func/include
INCLUDES += -I./beken378/func/misc
INCLUDES += -I./beken378/func/rf_test
INCLUDES += -I./beken378/func/bk7011_cal
INCLUDES += -I./beken378/func/user_driver
INCLUDES += -I./beken378/func/power_save
INCLUDES += -I./beken378/func/uart_debug
INCLUDES += -I./beken378/func/ethernet_intf
INCLUDES += -I./beken378/func/$(WPA_VERSION)/hostapd
INCLUDES += -I./beken378/func/$(WPA_VERSION)/bk_patch
INCLUDES += -I./beken378/func/$(WPA_VERSION)/src/utils
INCLUDES += -I./beken378/func/$(WPA_VERSION)/src/ap
INCLUDES += -I./beken378/func/$(WPA_VERSION)/src/common
INCLUDES += -I./beken378/func/$(WPA_VERSION)/src/drivers
INCLUDES += -I./beken378/func/$(WPA_VERSION)/src
INCLUDES += -I./beken378/func/$(WPA_VERSION)/wpa_supplicant
INCLUDES += -I./beken378/func/$(WPA_VERSION)/bk_patch
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2/src
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2/port
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2/src/include
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2/src/include/netif
#INCLUDES += -I./beken378/func/lwip_intf/lwip-2.0.2/src/include/lwip
INCLUDES += -I../tuyaos/tuyaos_adapter/include
INCLUDES += -I../tuyaos/tuyaos_adapter/include/lwip
INCLUDES += -I./beken378/func/temp_detect
INCLUDES += -I./beken378/func/spidma_intf
INCLUDES += -I./beken378/func/rwnx_intf
INCLUDES += -I./beken378/func/joint_up
#INCLUDES += -I./beken378/func/bk_tuya_pwm
INCLUDES += -I./beken378/os/include
INCLUDES += -I./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/portable/Keil/ARM968es
INCLUDES += -I./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/include
INCLUDES += -I./beken378/os/FreeRTOSv9.0.0

ifeq ($(CFG_WRAP_LIBC),1)
INCLUDES += -I./beken378/func/libc
endif

# For WPA3
ifeq ($(CFG_WPA3),1)
INCLUDES += -I./beken378/func/wolfssl
endif

# -------------------------------------------------------------------
# Source file list
# -------------------------------------------------------------------
SRC_C =
DRAM_C =
SRC_OS =

#application layer
SRC_C += ./beken378/app/app_bk.c
SRC_C += ./beken378/app/ate_app.c
SRC_C += ./beken378/app/config/param_config.c
SRC_C += ./beken378/app/standalone-ap/sa_ap.c
SRC_C += ./beken378/app/standalone-station/sa_station.c

#demo module
SRC_C += ./beken378/demo/ieee802_11_demo.c

#driver layer
SRC_C += ./beken378/driver/common/dd.c
SRC_C += ./beken378/driver/common/drv_model.c
SRC_C += ./beken378/driver/dma/dma.c
SRC_C += ./beken378/driver/driver.c
SRC_C += ./beken378/driver/entry/arch_main.c
#SRC_C += ./beken378/driver/fft/fft.c
SRC_C += ./beken378/driver/flash/flash.c
SRC_C += ./beken378/driver/general_dma/general_dma.c
SRC_C += ./beken378/driver/gpio/gpio.c
#SRC_C += ./beken378/driver/i2s/i2s.c
SRC_C += ./beken378/driver/icu/icu.c
SRC_C += ./beken378/driver/intc/intc.c
SRC_C += ./beken378/driver/irda/irda.c
SRC_C += ./beken378/driver/macphy_bypass/mac_phy_bypass.c
SRC_C += ./beken378/driver/phy/phy_trident.c
SRC_C += ./beken378/driver/pwm/pwm.c
SRC_C += ./beken378/driver/pwm/pwm_new.c
SRC_C += ./beken378/driver/pwm/mcu_ps_timer.c
SRC_C += ./beken378/driver/pwm/bk_timer.c
SRC_C += ./beken378/driver/rw_pub/rw_platf_pub.c
SRC_C += ./beken378/driver/saradc/saradc.c
SRC_C += ./beken378/driver/spi/spi_bk7231n.c
SRC_C += ./beken378/driver/spi/spi_master_bk7231n_new.c
SRC_C += ./beken378/driver/spi/spi_slave_bk7231n.c
SRC_C += ./beken378/driver/spidma/spidma.c
SRC_C += ./beken378/driver/sys_ctrl/sys_ctrl.c
SRC_C += ./beken378/driver/uart/Retarget.c
SRC_C += ./beken378/driver/uart/uart_bk.c
SRC_C += ./beken378/driver/uart/printf.c
SRC_C += ./beken378/driver/wdt/wdt.c

####################################################
#rw head start
#ifeq (${CFG_BLE_5X_VERSION}${CFG_BLE_5X_RW},11)
ifeq ("${CFG_BLE_5X_VERSION}", "1")
ifeq ("${CFG_BLE_5X_RW}", "1")
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/inc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/attc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/attm
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/atts
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattm
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cm
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/import/reg
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/em/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/import
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/common/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/ecc_p256/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/h4tl/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_pub/prf
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/rwip/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/rwip/import/reg
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/nvds/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/config
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/driver/reg
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/driver/rf
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/driver/uart
INCLUDES += -I./beken378/driver/ble_5_x_rw/platform/7231n/entry
INCLUDES += -I./beken378/driver/ble_5_x_rw/arch/armv5
INCLUDES += -I./beken378/driver/ble_5_x_rw/arch/armv5/ll
INCLUDES += -I./beken378/driver/ble_5_x_rw/arch/armv5/compiler
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_pub/profiles/comm/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_pub/app/api
INCLUDES += -I./beken378/driver/ble_5_x_rw/ble_pub/ui
#rw head end

####################################################
ifeq ("${CFG_BLE_5X_USE_RWIP_LIB}", "0")
#rw source start
#ble lib
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_cte.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_hci.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_past.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_sig.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_smp.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapc/gapc_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_actv.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_addr.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_adv.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_cfg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_init.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_list.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_per_sync.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_scan.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_smp.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gap/gapm/gapm_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/attc/attc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/attm/attm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/attm/attm_db.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/atts/atts.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc/gattc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc/gattc_rc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc/gattc_sdp.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc/gattc_svc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattc/gattc_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattm/gattm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/gatt/gattm/gattm_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc/l2cc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc/l2cc_lecb.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc/l2cc_pdu.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc/l2cc_sig.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cc/l2cc_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/l2c/l2cm/l2cm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/hl/src/rwble_hl/rwble_hl.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/co/ble_util_buf.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_chmap_upd.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_clk_acc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_con_upd.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_cte.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_dbg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_disconnect.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_dl_upd.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_encrypt.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_feat_exch.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_hci.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_le_ping.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_llcp.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_past.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_phy_upd.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llc/llc_ver_exch.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_adv.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_con.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_init.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_per_adv.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_scan.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_sync.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/lld/lld_test.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_adv.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_hci.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_init.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_scan.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/ble/ll/src/llm/llm_test.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_fc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_msg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_tl.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/src/sch_alarm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/src/sch_arb.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/src/sch_plan.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/src/sch_prog.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/sch/src/sch_slice.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_c1.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_ccm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_cmac.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_f4.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_f5.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_f6.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_g2.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_k1.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_k2.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_k3.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_k4.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_rpa.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/aes/src/ble_aes_s1.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/common/src/common_list.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/common/src/common_utils.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_iqgen.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_mwsgen.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_swdiag.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_trc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_trc_mem.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/dbg/src/dbg_trc_tl.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ecc_p256/src/ecc_p256.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/h4tl/src/h4tl.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_event.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_mem.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_msg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_queue.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/ke/src/kernel_timer.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/modules/rwip/rwip_driver.c
####################################################
endif  ###ifeq ("${CFG_BLE_5X_USE_RWIP_LIB}", "0")

#ble pub
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/rwip/src/rwip.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/rwip/src/rwble.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/entry/ble_main.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/driver/rf/rf_xvr.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/driver/uart/uart_ble.c
SRC_C += ./beken378/driver/ble_5_x_rw/platform/7231n/driver/rf/ble_rf_port.c
ifeq ("${CFG_USE_BK_HOST}", "1")
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_fc.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_msg.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_lib/ip/hci/src/hci_tl.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/prf/prf.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/prf/prf_utils.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/profiles/comm/src/comm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/profiles/comm/src/comm_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/app/src/app_comm.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/app/src/app_ble.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/app/src/app_task.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/ui/ble_ui.c
SRC_C += ./beken378/driver/ble_5_x_rw/ble_pub/app/src/app_ble_task.c
endif
#rw source end
else  ###ifeq ("${CFG_BLE_5X_VERSION}", "1") && ("${CFG_BLE_5X_RW}", "1")

endif ##ifeq ("${CFG_BLE_5X_VERSION}", "1")
endif ##ifeq ("${CFG_BLE_5X_RW}", "1")
####################################################

#function layer
SRC_C += ./beken378/func/func.c
SRC_C += ./beken378/func/bk7011_cal/bk7231U_cal.c
SRC_C += ./beken378/func/bk7011_cal/bk7231N_cal.c
SRC_C += ./beken378/func/bk7011_cal/manual_cal_bk7231U.c
SRC_C += ./beken378/func/joint_up/role_launch.c
SRC_C += ./beken378/func/hostapd_intf/hostapd_intf.c
SRC_C += ./beken378/func/$(WPA_VERSION)/bk_patch/ddrv.c
SRC_C += ./beken378/func/$(WPA_VERSION)/bk_patch/signal.c
SRC_C += ./beken378/func/$(WPA_VERSION)/bk_patch/sk_intf.c
SRC_C += ./beken378/func/$(WPA_VERSION)/bk_patch/fake_socket.c
SRC_C += ./beken378/func/$(WPA_VERSION)/hostapd/main_none.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-unwrap.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-wrap.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/rc4.c
ifeq ($(CFG_WPA_CRYPTO_MBEDTLS),1)
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/crypto_mbedtls.c
else
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-internal.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-internal-dec.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-internal-enc.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/md5.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/md5-internal.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha1.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha1-internal.c
endif
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha1-pbkdf2.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha1-prf.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/tls_none.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ap_config.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ap_drv_ops.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ap_list.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ap_mlme.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/beacon.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/drv_callbacks.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/hostapd.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/hw_features.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ieee802_11_auth.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ieee802_11.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ieee802_11_ht.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ieee802_11_shared.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/ieee802_1x.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/sta_info.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/tkip_countermeasures.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/utils.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/wmm.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/wpa_auth.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/wpa_auth_glue.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/wpa_auth_ie.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/hw_features_common.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/ieee802_11_common.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/wpa_common.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/drivers/driver_beken.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/drivers/driver_common.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/drivers/drivers.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/l2_packet/l2_packet_none.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/rsn_supp/wpa.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/rsn_supp/wpa_ie.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/utils/common.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/utils/eloop.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/utils/os_none.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/utils/wpabuf.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/blacklist.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/bss.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/config.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/config_none.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/events.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/main_supplicant.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/notify.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/wmm_ac.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/wpa_scan.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/wpas_glue.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/wpa_supplicant.c

ifeq ($(CFG_USE_WPA_29),1)
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/op_classes.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/rsn_supp/pmksa_cache.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/ap/pmksa_cache_auth.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/notifier.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/wpa_psk_cache.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/utils/wpa_debug.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/ctrl_iface.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/wlan.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/sme.c
SRC_C += ./beken378/func/$(WPA_VERSION)/wpa_supplicant/cmd_wlan.c
endif

# for WPA3
ifeq ($(CFG_WPA3),1)
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/sae.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/common/dragonfly.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-ctr.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-omac1.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/aes-siv.c
#SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/crypto_wolfssl.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/crypto_mbedtls-bignum.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/crypto_mbedtls-ec.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/dh_group5.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/dh_groups.c
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha256.c
ifneq ($(CFG_WPA_CRYPTO_MBEDTLS),1)
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha256-internal.c
endif
SRC_C += ./beken378/func/$(WPA_VERSION)/src/crypto/sha256-prf.c
endif # CFG_WPA3

ifeq ($(CFG_WRAP_LIBC),1)
SRC_C += ./beken378/func/libc/stdio/lib_libvscanf.c
SRC_C += ./beken378/func/libc/stdio/lib_memsistream.c
SRC_C += ./beken378/func/libc/stdio/lib_meminstream.c
SRC_C += ./beken378/func/libc/stdio/lib_sscanf.c
SRC_C += ./beken378/func/libc/stdio/lib_vsscanf.c
SRC_C += ./beken378/func/libc/stdlib/lib_strtod.c
SRC_C += ./beken378/func/libc/stdlib/lib_qsort.c
endif

# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/port/ethernetif.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/port/net.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/port/sys_arch.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/api_lib.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/api_msg.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/err.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/netbuf.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/netdb.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/netifapi.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/sockets.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/api/tcpip.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/def.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/dns.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/inet_chksum.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/init.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ip.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/autoip.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/dhcp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/etharp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/icmp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/igmp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/ip4_addr.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/ip4.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv4/ip4_frag.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/dhcp6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/ethip6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/icmp6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/inet6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/ip6_addr.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/ip6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/ip6_frag.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/mld6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/ipv6/nd6.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/mem.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/memp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/netif.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/pbuf.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/raw.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/stats.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/sys.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/tcp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/tcp_in.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/tcp_out.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/timeouts.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/core/udp.c
# SRC_C += ./beken378/func/lwip_intf/lwip-2.0.2/src/netif/ethernet.c
SRC_C += ./beken378/func/lwip_intf/dhcpd/dhcp-server.c
SRC_C += ./beken378/func/lwip_intf/dhcpd/dhcp-server-main.c
SRC_C += ./beken378/func/misc/fake_clock.c
SRC_C += ./beken378/func/misc/target_util.c
SRC_C += ./beken378/func/misc/start_type.c
SRC_C += ./beken378/func/misc/soft_encrypt.c
SRC_C += ./beken378/func/power_save/power_save.c
SRC_C += ./beken378/func/power_save/manual_ps.c
SRC_C += ./beken378/func/power_save/mcu_ps.c
SRC_C += ./beken378/func/rf_test/rx_sensitivity.c
SRC_C += ./beken378/func/rf_test/tx_evm.c
SRC_C += ./beken378/func/rwnx_intf/rw_ieee80211.c
SRC_C += ./beken378/func/rwnx_intf/rw_msdu.c
SRC_C += ./beken378/func/rwnx_intf/rw_msg_rx.c
SRC_C += ./beken378/func/rwnx_intf/rw_msg_tx.c
SRC_C += ./beken378/func/sim_uart/gpio_uart.c
SRC_C += ./beken378/func/sim_uart/pwm_uart.c
SRC_C += ./beken378/func/spidma_intf/spidma_intf.c
SRC_C += ./beken378/func/temp_detect/temp_detect.c
SRC_C += ./beken378/func/uart_debug/cmd_evm.c
SRC_C += ./beken378/func/uart_debug/cmd_help.c
SRC_C += ./beken378/func/uart_debug/cmd_reg.c
SRC_C += ./beken378/func/uart_debug/cmd_rx_sensitivity.c
SRC_C += ./beken378/func/uart_debug/command_line.c
SRC_C += ./beken378/func/uart_debug/command_table.c
SRC_C += ./beken378/func/uart_debug/udebug.c
SRC_C += ./beken378/func/user_driver/BkDriverFlash.c
SRC_C += ./beken378/func/user_driver/BkDriverRng.c
SRC_C += ./beken378/func/user_driver/BkDriverGpio.c
SRC_C += ./beken378/func/user_driver/BkDriverPwm.c
SRC_C += ./beken378/func/user_driver/BkDriverUart.c
SRC_C += ./beken378/func/user_driver/BkDriverWdg.c
SRC_C += ./beken378/func/user_driver/BkDriverTimer.c
SRC_C += ./beken378/func/wlan_ui/wlan_cli.c

# For WPA3: wolfssl
ifeq ($(CFG_WPA3),1)
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/wolfmath.c
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/memory.c
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/tfm.c
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/ecc.c

# wpa_supplicant 2.9 needs random generator
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/random.c
#SRC_C += ./beken378/func/wolfssl/wolfcrypt/src/sha256.c
endif

SRC_C += ./beken378/func/wlan_ui/wlan_ui.c
#SRC_C += ./beken378/func/bk_tuya_pwm/bk_tuya_pwm.c
SRC_C += ./beken378/func/net_param_intf/net_param.c

#rwnx ip module
# SRC_C += ./beken378/ip/common/co_dlist.c
# SRC_C += ./beken378/ip/common/co_list.c
# SRC_C += ./beken378/ip/common/co_math.c
# SRC_C += ./beken378/ip/common/co_pool.c
# SRC_C += ./beken378/ip/common/co_ring.c
# SRC_C += ./beken378/ip/ke/ke_env.c
# SRC_C += ./beken378/ip/ke/ke_event.c
# SRC_C += ./beken378/ip/ke/ke_msg.c
# SRC_C += ./beken378/ip/ke/ke_queue.c
# SRC_C += ./beken378/ip/ke/ke_task.c
# SRC_C += ./beken378/ip/ke/ke_timer.c
# SRC_C += ./beken378/ip/lmac/src/chan/chan.c
# SRC_C += ./beken378/ip/lmac/src/hal/hal_desc.c
# SRC_C += ./beken378/ip/lmac/src/hal/hal_dma.c
# SRC_C += ./beken378/ip/lmac/src/hal/hal_machw.c
# SRC_C += ./beken378/ip/lmac/src/hal/hal_mib.c
# SRC_C += ./beken378/ip/lmac/src/mm/mm.c
# SRC_C += ./beken378/ip/lmac/src/mm/mm_bcn.c
# SRC_C += ./beken378/ip/lmac/src/mm/mm_task.c
# SRC_C += ./beken378/ip/lmac/src/mm/mm_timer.c
# SRC_C += ./beken378/ip/lmac/src/p2p/p2p.c
# SRC_C += ./beken378/ip/lmac/src/ps/ps.c
# SRC_C += ./beken378/ip/lmac/src/rd/rd.c
# SRC_C += ./beken378/ip/lmac/src/rwnx/rwnx.c
# SRC_C += ./beken378/ip/lmac/src/rx/rx_swdesc.c
# SRC_C += ./beken378/ip/lmac/src/rx/rxl/rxl_cntrl.c
# SRC_C += ./beken378/ip/lmac/src/rx/rxl/rxl_hwdesc.c
# SRC_C += ./beken378/ip/lmac/src/scan/scan.c
# SRC_C += ./beken378/ip/lmac/src/scan/scan_shared.c
# SRC_C += ./beken378/ip/lmac/src/scan/scan_task.c
# SRC_C += ./beken378/ip/lmac/src/sta/sta_mgmt.c
# SRC_C += ./beken378/ip/lmac/src/td/td.c
# SRC_C += ./beken378/ip/lmac/src/tdls/tdls.c
# SRC_C += ./beken378/ip/lmac/src/tdls/tdls_task.c
# SRC_C += ./beken378/ip/lmac/src/tpc/tpc.c
# SRC_C += ./beken378/ip/lmac/src/tx/tx_swdesc.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_buffer.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_buffer_shared.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_cfm.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_cntrl.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_frame.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_frame_shared.c
# SRC_C += ./beken378/ip/lmac/src/tx/txl/txl_hwdesc.c
# SRC_C += ./beken378/ip/lmac/src/vif/vif_mgmt.c
# SRC_C += ./beken378/ip/mac/mac.c
# SRC_C += ./beken378/ip/mac/mac_ie.c
# SRC_C += ./beken378/ip/umac/src/apm/apm.c
# SRC_C += ./beken378/ip/umac/src/apm/apm_task.c
# SRC_C += ./beken378/ip/umac/src/bam/bam.c
# SRC_C += ./beken378/ip/umac/src/bam/bam_task.c
# SRC_C += ./beken378/ip/umac/src/me/me.c
# SRC_C += ./beken378/ip/umac/src/me/me_mgmtframe.c
# SRC_C += ./beken378/ip/umac/src/me/me_mic.c
# SRC_C += ./beken378/ip/umac/src/me/me_task.c
# SRC_C += ./beken378/ip/umac/src/me/me_utils.c
# SRC_C += ./beken378/ip/umac/src/rc/rc.c
# SRC_C += ./beken378/ip/umac/src/rc/rc_basic.c
# SRC_C += ./beken378/ip/umac/src/rxu/rxu_cntrl.c
# SRC_C += ./beken378/ip/umac/src/scanu/scanu.c
# SRC_C += ./beken378/ip/umac/src/scanu/scanu_shared.c
# SRC_C += ./beken378/ip/umac/src/scanu/scanu_task.c
# SRC_C += ./beken378/ip/umac/src/sm/sm.c
# SRC_C += ./beken378/ip/umac/src/sm/sm_task.c
# SRC_C += ./beken378/ip/umac/src/txu/txu_cntrl.c 

ifeq ($(CFG_WPA3),1)
# SRC_C += ./beken378/ip/umac/src/mfp/mfp.c
# SRC_C += ./beken378/ip/umac/src/mfp/mfp_bip.c
endif

#operation system module
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/croutine.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/event_groups.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/list.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/portable/Keil/ARM968es/port.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/portable/MemMang/heap_6.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/queue.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/tasks.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/FreeRTOS/Source/timers.c
SRC_OS += ./beken378/os/FreeRTOSv9.0.0/rtos_pub.c
SRC_C += ./beken378/os/mem_arch.c
SRC_C += ./beken378/os/platform_stub.c
SRC_C += ./beken378/os/str_arch.c

#examples for customer
TY_SRC_DIRS += $(shell find ../tuyaos/tuyaos_adapter/src -type d)
TY_SRC_DIRS += $(shell find ../tuyaos/tuyaos_adapter/include -type d)

SRC_C += $(foreach dir, $(TY_SRC_DIRS), $(wildcard $(dir)/*.c)) # need export
SRC_C += $(foreach dir, $(TY_SRC_DIRS), $(wildcard $(dir)/*.cpp)) 
SRC_C += $(foreach dir, $(TY_SRC_DIRS), $(wildcard $(dir)/*.s)) 
SRC_C += $(foreach dir, $(TY_SRC_DIRS), $(wildcard $(dir)/*.S)) 

#assembling files
SRC_S = 
SRC_S +=  ./beken378/driver/entry/boot_handlers.S
SRC_S +=  ./beken378/driver/entry/boot_vectors.S

# Generate obj list
# -------------------------------------------------------------------
# remove leading ../, ../.., ../../.. 
OBJ_LIST = $(patsubst $(OBJ_DIR)/../%,$(OBJ_DIR)/%,$(patsubst $(OBJ_DIR)/../%,$(OBJ_DIR)/%,$(patsubst $(OBJ_DIR)/../%,$(OBJ_DIR)/%,$(SRC_C:%.c=$(OBJ_DIR)/%.o))))
# The same as previoius, but cost too must time
# OBJ_LIST = $(foreach n,$(SRC_C:%.c=$(OBJ_DIR)/%.o),$(shell echo $(n) | sed -e 's|\.\./||g'))
DEPENDENCY_LIST = $(patsubst %.o,%.d,$(OBJ_LIST))

OBJ_S_LIST = $(SRC_S:%.S=$(OBJ_DIR)/%.O)
DEPENDENCY_S_LIST = $(SRC_S:%.S=$(OBJ_DIR)/%.d)

OBJ_OS_LIST = $(SRC_OS:%.c=$(OBJ_DIR)/%.marm.o)
DEPENDENCY_OS_LIST = $(SRC_OS:%.c=$(OBJ_DIR)/%.d)

# Compile options
# -------------------------------------------------------------------
CFLAGS =
CFLAGS += -g -mthumb -mcpu=arm968e-s -march=armv5te -mthumb-interwork -mlittle-endian -Os -std=c99 -ffunction-sections -Wall -fsigned-char -fdata-sections -Wunknown-pragmas -nostdlib -Wno-unused-function -Wno-unused-but-set-variable

OSFLAGS =
OSFLAGS += -g -marm -mcpu=arm968e-s -march=armv5te -mthumb-interwork -mlittle-endian -Os -std=c99 -ffunction-sections -Wall -fsigned-char -fdata-sections -Wunknown-pragmas

ASMFLAGS = 
ASMFLAGS += -g -marm -mthumb-interwork -mcpu=arm968e-s -march=armv5te -x assembler-with-cpp

LFLAGS = 
LFLAGS += -g -Wl,--gc-sections -marm -mcpu=arm968e-s -mthumb-interwork -nostdlib -Xlinker -Map=tuya.map
LFLAGS += -Wl,-wrap,malloc -Wl,-wrap,_malloc_r -Wl,-wrap,free -Wl,-wrap,_free_r -Wl,-wrap,zalloc -Wl,-wrap,calloc -Wl,-wrap,realloc  -Wl,-wrap,_realloc_r
LFLAGS += -Wl,-wrap,printf -Wl,-wrap,vsnprintf -Wl,-wrap,snprintf -Wl,-wrap,sprintf -Wl,-wrap,puts

# stdlib wrapper
ifeq ($(CFG_WRAP_LIBC),1)
LFLAGS += -Wl,-wrap,strtod -Wl,-wrap,qsort
LFLAGS += -Wl,-wrap,sscanf
endif

# For WPA3
ifeq ($(CFG_WPA3),1)
WOLFSSL_CFLAGS += -DWOLFSSL_BEKEN
CFLAGS += $(WOLFSSL_CFLAGS)
endif

LIBFLAGS =
LIBFLAGS += -L./beken378/lib/ -lrwnx
ifeq ("${CFG_BLE_5X_USE_RWIP_LIB}", "1")
ifeq ("${CFG_USE_BK_HOST}", "1")
LIBFLAGS += -L./beken378/lib/ -lble
else
LIBFLAGS += -L./beken378/lib/ -lblenohost
endif
endif

ifeq ("${CFG_GIT_VERSION}", "")

else
CFLAGS += -DVCS_RELEASE_VERSION=\"$(CFG_GIT_VERSION)\"
endif

# Compile
# -------------------------------------------------------------------
# add tuya iot lib compile support
# -------------------------------------------------------------------
LIBFLAGS += -L $(LIBS_DIR) $(addprefix -l, $(LIBS))
ifeq ("${CFG_USE_BK_HOST}", "0")
ifneq ($(wildcard $(TOP_DIR)/libs/libtal_ble_host_stack.a), )
LIBFLAGS += -L $(TOP_DIR)/libs/ -ltal_ble_host_stack
endif
endif
CFLAGS += -DUSER_SW_VER=\"$(APP_VERSION)\" -DAPP_BIN_NAME=\"$(APP_BIN_NAME)\"

# -------------------------------------------------------------------
# add tuya iot application compile support
# -------------------------------------------------------------------
TY_OUTPUT = ${OUTPUT_DIR}/$(APP_BIN_NAME)_$(APP_VERSION)
TY_INC_DIRS += ${HEADER_DIR}
#TY_INC_DIRS += $(shell find $(TOP_DIR)/sdk/include -type d)
#SDK_INCLUDE_DIRS := $(shell find $(TOP_DIR)/sdk -name include -type d)
#TY_INC_DIRS += $(foreach dir,$(SDK_INCLUDE_DIRS),$(shell find $(dir) -type d))

#TY_INC_DIRS := $(shell find $(TOP_INCLUDE_DIR)/adapter -name include -type d)
# TY_INC_DIRS += $(shell find $(TOP_DIR)/include -type d)
# TY_INC_DIRS += $(shell find $(TOP_DIR)/${APP_DIR}/$(APP_BIN_NAME)/include -type d)
# TY_INC_DIRS += $(shell find ../tuyaos/tuyaos_adapter/include -type d)
#TY_INC_DIRS += $(shell find $(TOP_DIR)/include -type d)
TY_INC_DIRS += $(shell find ../tuyaos/tuyaos_adapter/include -type d)
# TY_INC_DIRS += $(shell find $(TOP_DIR)/samples/$(APP_BIN_NAME)/include -type d)
#TY_INC_DIRS += $(shell find $(TOP_DIR)/samples/$(APP_BIN_NAME) -type d)

INCLUDES += $(foreach base_dir, $(TY_INC_DIRS), $(addprefix -I , $(base_dir))) 

$(TY_OBJS): %.o : %.c
	echo "build $@ ..."
	@$(CC) $(CFLAGS) $(TY_IOT_CFLAGS) $(INCLUDES) -c $< -o $@
	@$(CC) $(CFLAGS) $(TY_IOT_CFLAGS) $(INCLUDES) -c $< -MM -MT $@ -MF $(OBJ_DIR)/$(notdir $(patsubst %.o,%.d,$@))
	@cp $@ $(OBJ_DIR)/$(notdir $@)
	@chmod 777 $(OBJ_DIR)/$(notdir $@)

sinclude $(TY_DEPENDENCY_LIST)

CUR_PATH = $(shell pwd)	
.PHONY: application

test_target: prerequirement $(OBJ_LIST) $(OBJ_S_LIST) $(OBJ_OS_LIST) $(TY_IOT_LIB)
ifneq ($(COMPONENTS_LIB),)
ifneq ($(COMP_SRCS),)
	@$(MAKE) -f application.mk comp_libs
endif # COMP_SRCS
endif # COMPONENTS_LIB


application: test_target
ifeq ("${ota_idx}", "1")
	$(Q)$(ECHO) "  $(GREEN)LD   $(APP_BIN_NAME)_$(APP_VERSION).axf$(NC)"
	$(Q)$(LD) $(LFLAGS) -o $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).axf  $(OBJ_LIST) $(OBJ_S_LIST) $(OBJ_OS_LIST) $(LIBFLAGS) -T./beken378/build/bk7231n_ota.ld
else ifeq ("${ota_idx}", "2")
else
	@echo ===========================================================
	@echo ota_idx must be "1" or "2"
	@echo ===========================================================
endif

	$(NM) $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).axf | sort > $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).map
	$(OBJDUMP) -d $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).axf > $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).asm
	$(OBJCOPY) -O binary $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).axf $(TY_OUTPUT)/$(APP_BIN_NAME)_$(APP_VERSION).bin
# Generate build info
# -------------------------------------------------------------------	

vpath % ..:../..:../../..
.PHONY: prerequirement
prerequirement:
	
	echo prerequirement0
	@echo ===========================================================
	@echo Build $(APP_BIN_NAME)
	@echo ===========================================================
	echo prerequirement1
	mkdir -p $(OBJ_DIR)
#mkdir -p $(BIN_DIR)
#@echo $(OBJ_LIST)
#@echo $(OBJ_S_LIST)
#@echo $(OBJ_OS_LIST)
#@echo $(DEPENDENCY_LIST)
	echo prerequirement2
	
	@# add tuya bin output
	@mkdir -p $(TY_OUTPUT)

$(OBJ_DIR)/%.o: %.c
	$(Q)if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(Q)$(ECHO) "  $(GREEN)CC   $<$(NC)"
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $< -MM -MT $@ -MF $(patsubst %.o,%.d,$@)

$(OBJ_DIR)/%.O: %.S
	$(Q)if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(Q)$(ECHO) "  $(GREEN)AS   $<$(NC)"
	$(Q)$(CC) $(ASMFLAGS) $(INCLUDES) -c $< -o $@
	$(Q)$(CC) $(ASMFLAGS) $(INCLUDES) -c $< -MM -MT $@ -MF $(patsubst %.O,%.d,$@)

$(OBJ_DIR)/%.marm.o: %.c
	$(Q)if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(Q)$(ECHO) "  $(GREEN)CC   $<$(NC)"
	$(Q)$(CC) $(OSFLAGS) $(INCLUDES) -c $< -o $@
	$(Q)$(CC) $(OSFLAGS) $(INCLUDES) -c $< -MM -MT $@ -MF $(patsubst %.marm.o,%.d,$@)

-include $(DEPENDENCY_LIST)
-include $(DEPENDENCY_S_LIST)
-include $(DEPENDENCY_OS_LIST)

# -------------------------------------------------------------------	
# Generate build info
# -------------------------------------------------------------------	
.PHONY: setup
setup:
	@echo "----------------"
	@echo Setup $(GDB_SERVER)
	@echo "----------------"
	
.PHONY: debug
debug:
	@if [ ! -f $(FLASH_TOOLDIR)/rtl_gdb_debug.txt ] ; then echo Please do \"make setup GDB_SERVER=[jlink or openocd]\" first; echo && false ; fi
ifeq ($(findstring CYGWIN, $(OS)), CYGWIN) 
	$(FLASH_TOOLDIR)/Check_Jtag.sh
	cmd /c start $(GDB) -x $(FLASH_TOOLDIR)/rtl_gdb_debug.txt 
else
	$(GDB) -x $(FLASH_TOOLDIR)/rtl_gdb_debug.txt
endif

.PHONY: clean
clean:
ifneq ($(COMPONENTS_LIB),)
ifneq ($(COMP_SRCS),)
	@$(MAKE) -f application.mk comp_libs_clean
endif # COMP_SRCS
endif # COMPONENTS_LIB
	@echo "clean $(TY_OUTPUT)"
	-rm -f .config
	rm -rf $(TARGET)
	rm -rf $(TY_OUTPUT)
	
