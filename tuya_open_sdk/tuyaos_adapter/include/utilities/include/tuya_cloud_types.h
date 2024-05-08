/*
tuya_cloud_types.h
Copyright(C),2018-2020, 涂鸦科技 www.tuya.comm
*/

#ifndef TUYA_CLOUD_TYPES_H
#define TUYA_CLOUD_TYPES_H

#include "tuya_iot_config.h"
#include <stdarg.h>

#if OPERATING_SYSTEM == SYSTEM_LINUX
#include <ctype.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tuya_error_code.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef int OPERATE_RET;
typedef long long DLONG_T;
typedef DLONG_T *PDLONG_T;
typedef float FLOAT_T;
typedef FLOAT_T *PFLOAT_T;
typedef signed int INT_T;
typedef int *PINT_T;
typedef void *PVOID_T;
typedef char CHAR_T;
typedef char *PCHAR_T;
typedef signed char SCHAR_T;
typedef unsigned char UCHAR_T;
typedef short SHORT_T;
typedef unsigned short USHORT_T;
typedef short *PSHORT_T;
typedef long LONG_T;
typedef unsigned long ULONG_T;
typedef long *PLONG_T;
typedef unsigned char BYTE_T;
typedef BYTE_T *PBYTE_T;
typedef unsigned int UINT_T;
typedef unsigned int *PUINT_T;
typedef int BOOL_T;
typedef BOOL_T *PBOOL_T;
typedef long long int INT64_T;
typedef INT64_T *PINT64_T;
typedef unsigned long long int UINT64_T;
typedef UINT64_T *PUINT64_T;
typedef unsigned int UINT32_T;
typedef unsigned int *PUINT32_T;
typedef int INT32_T;
typedef int *PINT32_T;
typedef short INT16_T;
typedef INT16_T *PINT16_T;
typedef unsigned short UINT16_T;
typedef UINT16_T *PUINT16_T;
typedef signed char INT8_T;
typedef INT8_T *PINT8_T;
typedef unsigned char UINT8_T;
typedef UINT8_T *PUINT8_T;
typedef ULONG_T TIME_MS;
typedef ULONG_T TIME_S;
typedef unsigned int TIME_T;
typedef double DOUBLE_T;
typedef unsigned short WORD_T;
typedef WORD_T *PWORD_T;
typedef unsigned int DWORD_T;
typedef DWORD_T *PDWORD_T;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#ifndef VOID
#define VOID void
#endif

#ifndef VOID_T
#define VOID_T void
#endif


#ifndef CONST
#define CONST const
#endif

#ifndef STATIC
#define STATIC static
#endif

#ifndef SIZEOF
#define SIZEOF sizeof
#endif

#ifndef INLINE
#define INLINE inline
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifndef bool_t
typedef int bool_t;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

typedef size_t SIZE_T;

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef BIT
#define BIT(nr)     (1UL << (nr))
#endif
#define MAKEWORD(a, b) ((WORD_T)(((BYTE_T)(a)) | ((WORD_T)((BYTE_T)(b))) << 8))
#define MAKELONG(a, b) ((LONG_T)(((WORD_T)(a)) | ((DWORD_T)((WORD_T)(b))) << 16))
#define LOWORD(l) ((WORD_T)(l))
#define HIWORD(l) ((WORD_T)(((DWORD_T)(l) >> 16) & 0xFFFF))
#define LOBYTE(w) ((BYTE_T)(w))
#define HIBYTE(w) ((BYTE_T)(((WORD_T)(w) >> 8) & 0xFF))

#define WORD_SWAP(X)    (((X << 8) | (X >> 8))&0xFFFF)
#define DWORD_SWAP(X)   ( (((X)&0xff)<<24) + \
                          (((X)&0xff00)<<8) + \
                          (((X)&0xff0000)>>8) + \
                          (((X)&0xff000000)>>24) )
#define DDWORD_SWAP(X) { \
    unsigned long temp_low, temp_high; \
    temp_low = DWORD_SWAP((unsigned long)((X) & 0xffffffff)); \
    temp_high = DWORD_SWAP((unsigned long)((X) >> 32)); \
    X = 0; \
    X |= temp_low; \
    X <<= 32; \
    X |= temp_high;\
}

#if defined(LITTLE_END) && (LITTLE_END==1)
#define UNI_NTOHS(X) WORD_SWAP(X)
#define UNI_HTONS(X) WORD_SWAP(X)
#define UNI_NTOHL(X) DWORD_SWAP(X)
#define UNI_HTONL(X) DWORD_SWAP(X)
#define UNI_NTOHLL(X) DDWORD_SWAP(X)
#define UNI_HTONLL(X) DDWORD_SWAP(X)

#else

#define UNI_NTOHS(X) X
#define UNI_HTONS(X) X
#define UNI_NTOHL(X) X
#define UNI_HTONL(X) X
#define UNI_NTOHLL(X) X
#define UNI_HTONLL(X) X

#endif

#define OFFSOF(s,m) ((SIZE_T)(&(((s*)0)->m)))
#define CNTSOF(a)   (sizeof(a)/sizeof(a[0]))
#define CNTR_OF(ptr, type, member) \
        ({(type *)( (char *)ptr - OFFSOF(type,member) );}) // continer of

typedef struct
{
    char ip[16];    /* ip addr:  xxx.xxx.xxx.xxx  */
    char mask[16];  /* net mask: xxx.xxx.xxx.xxx  */
    char gw[16];    /* gateway:  xxx.xxx.xxx.xxx  */
}NW_IP_S;

#define MAC_ADDR_LEN 6
typedef struct {
    UINT8_T mac[MAC_ADDR_LEN];
}NW_MAC_S;

typedef enum {
    TUYA_IO_BITS_0,
    TUYA_IO_BITS_1,
    TUYA_IO_BITS_2,
    TUYA_IO_BITS_3,
    TUYA_IO_BITS_4,
    TUYA_IO_BITS_5,
    TUYA_IO_BITS_6,
    TUYA_IO_BITS_7,
    TUYA_IO_BITS_8,
    TUYA_IO_BITS_9,
    TUYA_IO_BITS_10,
    TUYA_IO_BITS_11,
    TUYA_IO_BITS_12,
    TUYA_IO_BITS_13,
    TUYA_IO_BITS_14,
    TUYA_IO_BITS_15,
    TUYA_IO_BITS_16,
    TUYA_IO_BITS_17,
    TUYA_IO_BITS_18,
    TUYA_IO_BITS_19,
    TUYA_IO_BITS_20,
    TUYA_IO_BITS_21,
    TUYA_IO_BITS_22,
    TUYA_IO_BITS_23,
    TUYA_IO_BITS_24,
    TUYA_IO_BITS_25,
    TUYA_IO_BITS_26,
    TUYA_IO_BITS_27,
    TUYA_IO_BITS_28,
    TUYA_IO_BITS_29,
    TUYA_IO_BITS_30,
    TUYA_IO_BITS_31,
}TUYA_ADDR_BITS_DEF_E;

/**
 * @brief adc sample type
 */
typedef enum {
    TUYA_ADC_NUM_0 = 0,          // ADC 0
    TUYA_ADC_NUM_1 = 1,          // ADC 1
    TUYA_ADC_NUM_2 = 2,          // ADC 2
    TUYA_ADC_NUM_3 = 3,          // ADC 3
    TUYA_ADC_NUM_4 = 4,          // ADC 4
    TUYA_ADC_NUM_5 = 5,          // ADC 5
    TUYA_ADC_NUM_6 = 6,			// ADC 6
    TUYA_ADC_NUM_MAX,
} TUYA_ADC_NUM_E;
typedef enum {
    TUYA_ADC_INNER_SAMPLE_VOL = 0,
    TUYA_ADC_EXTERNAL_SAMPLE_VOL
} TUYA_ADC_TYPE_E;

/**
 * @brief adc mode
 */
typedef enum {
    TUYA_ADC_SINGLE = 0,       ///< Single conversion mode --- select one channel to convert at a time.
    TUYA_ADC_CONTINUOUS,       ///< Continuous conversion mode --- select a channel to convert in a specific times.
    TUYA_ADC_SCAN,             ///< Scan mode --- select a group channel to convert at a time.
} TUYA_ADC_MODE_E;

typedef struct {
    UINT32_T ch_0             : 1;
    UINT32_T ch_1             : 1;
    UINT32_T ch_2             : 1;
    UINT32_T ch_3             : 1;
    UINT32_T ch_4             : 1;
    UINT32_T ch_5             : 1;
    UINT32_T ch_6             : 1;
    UINT32_T ch_7             : 1;
    UINT32_T ch_8             : 1;
    UINT32_T ch_9             : 1;
    UINT32_T ch_10            : 1;
    UINT32_T ch_11            : 1;
    UINT32_T ch_12            : 1;
    UINT32_T ch_13            : 1;
    UINT32_T ch_14            : 1;
    UINT32_T ch_15            : 1;
    UINT32_T rsv              :16;
}TUYA_AD_DA_CH_LIST_BIT_T;

typedef union {
    TUYA_AD_DA_CH_LIST_BIT_T bits;
    UINT32_T data;
}TUYA_AD_DA_CH_LIST_U;

/**
 * @brief adc config
 */
typedef struct {
    TUYA_AD_DA_CH_LIST_U  ch_list;       // adc channel list
    UINT8_T ch_nums;        // adc channel number,specifies the number of channels in ch_list that will be converted
    UINT8_T  width;         // sampling width
    UINT32_T freq;          // sampling frequency
    TUYA_ADC_TYPE_E type;   // sampling type
    TUYA_ADC_MODE_E mode;   // sampling mode
    UINT16_T   conv_cnt;    // sampling times
    UINT32_T   ref_vol;     // reference voltage (bat: mv), if dont support set reference voltage, ignore it
} TUYA_ADC_BASE_CFG_T;

/**
 * @brief dac num       //xbf
 */

typedef enum {
    TUYA_DAC_NUM_0 = 0,        // DAC 0
    TUYA_DAC_NUM_1 = 1,        // DAC 1
    TUYA_DAC_NUM_2 = 2,        // DAC 2
    TUYA_DAC_NUM_3 = 3,        // DAC 3
    TUYA_DAC_NUM_4 = 4,        // DAC 4
    TUYA_DAC_NUM_5 = 5,        // DAC 5
    TUYA_DAC_NUM_6 = 6,        // DAC 6
    TUYA_DAC_NUM_MAX,
} TUYA_DAC_NUM_E;
/**
 * @brief dac config       //xbf
 */
typedef struct {
    TUYA_AD_DA_CH_LIST_U  ch_list;       // dac channel list
    UINT8_T  ch_nums;       // dac channel number
    UINT8_T  width;         // output width
    UINT32_T freq;          // convert freq
} TUYA_DAC_BASE_CFG_T;

typedef struct{
	UINT8_T *data;
	UINT32_T len;
}TUYA_DAC_DATA_T;

typedef enum {
    TUYA_DAC_WRITE_FIFO = 0,   /*设置DAC的FIIO的数据*/
	TUYA_DAC_SET_BASE_CFG,     /*设置DAC的基本配置*/
} TUYA_DAC_CMD_E;

typedef struct{
	TUYA_DAC_CMD_E cmd;
	VOID * argu;
}TUYA_DAC_CTL_CONFIG_T;

/**
 * @brief flash type
 * 
 */
typedef enum {
    TUYA_FLASH_TYPE_BTL0 = 0,
    TUYA_FLASH_TYPE_BTL1,
    TUYA_FLASH_TYPE_STACK,
    TUYA_FLASH_TYPE_APP,
    TUYA_FLASH_TYPE_OTA,
    TUYA_FLASH_TYPE_USER0,
    TUYA_FLASH_TYPE_USER1,
    TUYA_FLASH_TYPE_KV_DATA,
    TUYA_FLASH_TYPE_KV_SWAP,
    TUYA_FLASH_TYPE_KV_KEY,
    TUYA_FLASH_TYPE_UF,    
    TUYA_FLASH_TYPE_INFO,
    TUYA_FLASH_TYPE_KV_UF,
    TUYA_FLASH_TYPE_KV_PROTECT,
    TUYA_FLASH_TYPE_RCD,
    TUYA_FLASH_TYPE_MAX,
} TUYA_FLASH_TYPE_E;

/**
 * @brief flash partition information
 * 
 */
typedef struct {
    UINT_T block_size;
    UINT_T start_addr;
    UINT_T size;
} TUYA_FLASH_PARTITION_T;

/**
 * @brief flash type base information
 * 
 */
#ifndef TUYA_FLASH_TYPE_MAX_PARTITION_NUM
#define TUYA_FLASH_TYPE_MAX_PARTITION_NUM 10
#endif

typedef struct {
    UINT_T partition_num;
    TUYA_FLASH_PARTITION_T partition[TUYA_FLASH_TYPE_MAX_PARTITION_NUM];
} TUYA_FLASH_BASE_INFO_T;

/**
 * @brief pinmux config
 */

/**
 * @brief tuya pinmux io name
 */
typedef UINT16_T TUYA_PIN_NAME_E;
/**
 * @brief tuya pinmux io name default define
 */
#define	 TUYA_IO_PIN_0	   0
#define	 TUYA_IO_PIN_1	   1
#define	 TUYA_IO_PIN_2	   2
#define	 TUYA_IO_PIN_3	   3
#define	 TUYA_IO_PIN_4	   4
#define	 TUYA_IO_PIN_5	   5
#define	 TUYA_IO_PIN_6	   6
#define	 TUYA_IO_PIN_7	   7
#define	 TUYA_IO_PIN_8	   8
#define	 TUYA_IO_PIN_9	   9
#define	 TUYA_IO_PIN_10	   10
#define	 TUYA_IO_PIN_11	   11
#define	 TUYA_IO_PIN_12	   12
#define	 TUYA_IO_PIN_13	   13
#define	 TUYA_IO_PIN_14	   14
#define	 TUYA_IO_PIN_15	   15
#define	 TUYA_IO_PIN_16	   16
#define	 TUYA_IO_PIN_17	   17
#define	 TUYA_IO_PIN_18	   18
#define	 TUYA_IO_PIN_19	   19
#define	 TUYA_IO_PIN_20	   20
#define	 TUYA_IO_PIN_21	   21
#define	 TUYA_IO_PIN_22	   22
#define	 TUYA_IO_PIN_23	   23
#define	 TUYA_IO_PIN_24	   24
#define	 TUYA_IO_PIN_25	   25
#define	 TUYA_IO_PIN_26	   26
#define	 TUYA_IO_PIN_27	   27
#define	 TUYA_IO_PIN_28	   28
#define	 TUYA_IO_PIN_29	   29
#define	 TUYA_IO_PIN_30	   30
#define	 TUYA_IO_PIN_31	   31
#define	 TUYA_IO_PIN_32	   32
#define	 TUYA_IO_PIN_33	   33
#define	 TUYA_IO_PIN_34	   34
#define	 TUYA_IO_PIN_35	   35
#define	 TUYA_IO_PIN_36	   36
#define	 TUYA_IO_PIN_37	   37
#define	 TUYA_IO_PIN_38	   38
#define	 TUYA_IO_PIN_39	   39
#define	 TUYA_IO_PIN_40	   40
#define	 TUYA_IO_PIN_41	   41
#define	 TUYA_IO_PIN_42	   42
#define	 TUYA_IO_PIN_43	   43
#define	 TUYA_IO_PIN_44	   44
#define	 TUYA_IO_PIN_45	   45
#define	 TUYA_IO_PIN_46	   46
#define	 TUYA_IO_PIN_47	   47
#define	 TUYA_IO_PIN_48	   48
#define	 TUYA_IO_PIN_49	   49
#define	 TUYA_IO_PIN_50	   50
#define	 TUYA_IO_PIN_51	   51
#define	 TUYA_IO_PIN_52	   52
#define	 TUYA_IO_PIN_53	   53
#define	 TUYA_IO_PIN_54	   54
#define	 TUYA_IO_PIN_55	   55
#define	 TUYA_IO_PIN_56	   56
#define	 TUYA_IO_PIN_57	   57
#define	 TUYA_IO_PIN_58	   58
#define	 TUYA_IO_PIN_59	   59
#define	 TUYA_IO_PIN_60	   60

#define  TUYA_IO_PIN_MAX   0xFFFF
/**
 * @brief tuya pinmux func name
 */

typedef UINT16_T TUYA_PIN_FUNC_E;

/**
 * @brief tuya pinmux default func define
 */

#define  TUYA_IIC0_SCL       0x0
#define  TUYA_IIC0_SDA       0x1
#define  TUYA_IIC1_SCL       0x2
#define  TUYA_IIC1_SDA       0x3
#define  TUYA_IIC2_SCL       0x4
#define  TUYA_IIC2_SDA       0x5

#define  TUYA_UART0_TX       0x100
#define  TUYA_UART0_RX       0x101
#define  TUYA_UART0_RTS      0x102
#define  TUYA_UART0_CTS      0x103
#define  TUYA_UART1_TX       0x104
#define  TUYA_UART1_RX       0x105
#define  TUYA_UART1_RTS      0x106
#define  TUYA_UART1_CTS      0x107
#define  TUYA_UART2_TX       0x108
#define  TUYA_UART2_RX       0x109
#define  TUYA_UART2_RTS      0x10A
#define  TUYA_UART2_CTS      0x10B
#define  TUYA_UART3_TX       0x10C
#define  TUYA_UART3_RX       0x10D
#define  TUYA_UART3_RTS      0x10E
#define  TUYA_UART3_CTS      0x10F

#define  TUYA_SPI0_MISO      0x200
#define  TUYA_SPI0_MOSI      0x201
#define  TUYA_SPI0_CLK       0x202
#define  TUYA_SPI0_CS        0x203
#define  TUYA_SPI1_MISO      0x204
#define  TUYA_SPI1_MOSI      0x205
#define  TUYA_SPI1_CLK       0x206
#define  TUYA_SPI1_CS        0x207
#define  TUYA_SPI2_MISO      0x208
#define  TUYA_SPI2_MOSI      0x209
#define  TUYA_SPI2_CLK       0x20A
#define  TUYA_SPI2_CS        0x20B

#define  TUYA_PWM0           0x300
#define  TUYA_PWM1           0x301
#define  TUYA_PWM2           0x302
#define  TUYA_PWM3           0x303
#define  TUYA_PWM4           0x304
#define  TUYA_PWM5           0x305

#define  TUYA_ADC0           0x400
#define  TUYA_ADC1           0x401
#define  TUYA_ADC2           0x402
#define  TUYA_ADC3           0x403
#define  TUYA_ADC4           0x404
#define  TUYA_ADC5           0x405

#define  TUYA_DAC0           0x500
#define  TUYA_DAC1           0x501
#define  TUYA_DAC2           0x502
#define  TUYA_DAC3           0x503
#define  TUYA_DAC4           0x504
#define  TUYA_DAC5           0x505

#define  TUYA_I2S0_SCK       0x600
#define  TUYA_I2S0_WS        0x601
#define  TUYA_I2S0_SDO_0     0x602
#define  TUYA_I2S0_SDI_0     0x603
#define  TUYA_I2S1_SCK       0x604
#define  TUYA_I2S1_WS        0x605
#define  TUYA_I2S1_SDO_0     0x606
#define  TUYA_I2S1_SDI_0     0x607

#define  TUYA_GPIO           0x700

#define  TUYA_PIN_FUNC_MAX   0xFFFF

typedef  struct {
    TUYA_PIN_NAME_E pin;
    TUYA_PIN_FUNC_E pin_func;
}TUYA_MUL_PIN_CFG_T;
/**
 * @brief gpio num
 */
typedef enum {
    TUYA_GPIO_NUM_0,		    // GPIO 0
    TUYA_GPIO_NUM_1,		    // GPIO 1
    TUYA_GPIO_NUM_2,	        // GPIO 2
    TUYA_GPIO_NUM_3,	        // GPIO 3
    TUYA_GPIO_NUM_4,	        // GPIO 4
    TUYA_GPIO_NUM_5,	        // GPIO 5
    TUYA_GPIO_NUM_6,	        // GPIO 6
    TUYA_GPIO_NUM_7,	        // GPIO 7
    TUYA_GPIO_NUM_8,	        // GPIO 8
    TUYA_GPIO_NUM_9,	        // GPIO 9
    TUYA_GPIO_NUM_10,		// GPIO 10
    TUYA_GPIO_NUM_11,		// GPIO 11
    TUYA_GPIO_NUM_12,		// GPIO 12
    TUYA_GPIO_NUM_13,		// GPIO 13
    TUYA_GPIO_NUM_14,		// GPIO 14
    TUYA_GPIO_NUM_15,		// GPIO 15
    TUYA_GPIO_NUM_16,		// GPIO 16
    TUYA_GPIO_NUM_17,		// GPIO 17
    TUYA_GPIO_NUM_18,		// GPIO 18
    TUYA_GPIO_NUM_19,		// GPIO 19
    TUYA_GPIO_NUM_20,		// GPIO 20
    TUYA_GPIO_NUM_21,		// GPIO 21
    TUYA_GPIO_NUM_22,		// GPIO 22
    TUYA_GPIO_NUM_23,		// GPIO 23
    TUYA_GPIO_NUM_24,		// GPIO 24
    TUYA_GPIO_NUM_25,		// GPIO 25
    TUYA_GPIO_NUM_26,		// GPIO 26
    TUYA_GPIO_NUM_27,		// GPIO 27
    TUYA_GPIO_NUM_28,		// GPIO 28
    TUYA_GPIO_NUM_29,		// GPIO 29
    TUYA_GPIO_NUM_30,		// GPIO 30
    TUYA_GPIO_NUM_31,		// GPIO 31
    TUYA_GPIO_NUM_32,		// GPIO 32
    TUYA_GPIO_NUM_33,		// GPIO 33
    TUYA_GPIO_NUM_34,		// GPIO 34
    TUYA_GPIO_NUM_35,
    TUYA_GPIO_NUM_36,
    TUYA_GPIO_NUM_37,
    TUYA_GPIO_NUM_38,
    TUYA_GPIO_NUM_39,
    TUYA_GPIO_NUM_40,
    TUYA_GPIO_NUM_41,
    TUYA_GPIO_NUM_42,
    TUYA_GPIO_NUM_43,
    TUYA_GPIO_NUM_44,
    TUYA_GPIO_NUM_45,
    TUYA_GPIO_NUM_46,
    TUYA_GPIO_NUM_47,
    TUYA_GPIO_NUM_48,
    TUYA_GPIO_NUM_49,
    TUYA_GPIO_NUM_50,
    TUYA_GPIO_NUM_51,
    TUYA_GPIO_NUM_52,
    TUYA_GPIO_NUM_53,
    TUYA_GPIO_NUM_54,
    TUYA_GPIO_NUM_55,
    TUYA_GPIO_NUM_56,
    TUYA_GPIO_NUM_57,
    TUYA_GPIO_NUM_58,
    TUYA_GPIO_NUM_59,
    TUYA_GPIO_NUM_60,
    TUYA_GPIO_NUM_61,
    TUYA_GPIO_NUM_62,
    TUYA_GPIO_NUM_63,
    TUYA_GPIO_NUM_MAX,
} TUYA_GPIO_NUM_E;

/**
 * @brief gpio level
 */
typedef enum {
    TUYA_GPIO_LEVEL_LOW = 0,
    TUYA_GPIO_LEVEL_HIGH,
} TUYA_GPIO_LEVEL_E;

/**
 * @brief gpio direction
 */
typedef enum {
    TUYA_GPIO_INPUT = 0,
    TUYA_GPIO_OUTPUT,
} TUYA_GPIO_DRCT_E;

/**
 * @brief gpio mode
 */
typedef enum {
    TUYA_GPIO_PULLUP = 0,        // for input
    TUYA_GPIO_PULLDOWN,          // for input
    TUYA_GPIO_HIGH_IMPEDANCE,    // for input
    TUYA_GPIO_FLOATING,          // for input
    TUYA_GPIO_PUSH_PULL,         // for output
    TUYA_GPIO_OPENDRAIN,         // for output
    TUYA_GPIO_OPENDRAIN_PULLUP,  // for output
} TUYA_GPIO_MODE_E;

/**
 * @brief gpio interrupt mode
 */
typedef enum {
    TUYA_GPIO_IRQ_RISE  = 0,
    TUYA_GPIO_IRQ_FALL,
    TUYA_GPIO_IRQ_RISE_FALL,
    TUYA_GPIO_IRQ_LOW,
    TUYA_GPIO_IRQ_HIGH,
} TUYA_GPIO_IRQ_E;

/**
 * @brief gpio config
 */
typedef struct {
    TUYA_GPIO_MODE_E  mode;
    TUYA_GPIO_DRCT_E  direct;
    TUYA_GPIO_LEVEL_E level;
} TUYA_GPIO_BASE_CFG_T;

typedef VOID_T (*TUYA_GPIO_IRQ_CB)(VOID_T *args);

/**
 * @brief gpio interrupt config
 */
typedef struct {
    TUYA_GPIO_IRQ_E      mode;
    TUYA_GPIO_IRQ_CB     cb;
    VOID_T              *arg;
} TUYA_GPIO_IRQ_T;

/**
 * @brief i2c num
 * 
 */
typedef enum {
    TUYA_I2C_NUM_0,		    // I2C 0
    TUYA_I2C_NUM_1,		    // I2C 1
    TUYA_I2C_NUM_2,	        // I2C 2
    TUYA_I2C_NUM_3,	        // I2C 3
    TUYA_I2C_NUM_4,	        // I2C 4
    TUYA_I2C_NUM_5,	        // I2C 5
    TUYA_I2C_NUM_MAX,
} TUYA_I2C_NUM_E;

/**
 * @brief i2c role
 *
 */
typedef enum {
    TUYA_IIC_MODE_MASTER,   ///< IIC Master
    TUYA_IIC_MODE_SLAVE     ///< IIC Slave
}TUYA_IIC_ROLE_E;

/**
 * @brief i2c speed
 *
 */
typedef enum {
    TUYA_IIC_BUS_SPEED_100K      = 0,    ///< Standard Speed (100kHz)
    TUYA_IIC_BUS_SPEED_400K      = 1,    ///< Fast Speed     (400kHz)
    TUYA_IIC_BUS_SPEED_1M        = 2,    ///< Fast+ Speed    (  1MHz)
    TUYA_IIC_BUS_SPEED_3_4M      = 3     ///< High Speed     (3.4MHz)
}TUYA_IIC_SPEED_E;

/**
 * @brief  IIC Address Mode
 *
 */
typedef enum {
    TUYA_IIC_ADDRESS_7BIT        = 0,  ///< 7-bit address mode, or called 8-bit address mode
    TUYA_IIC_ADDRESS_10BIT       = 1   ///< 10-bit address mode, or called 16-bit address mode
} TUYA_IIC_ADDR_MODE_E;

/**
 * @brief i2c cfg
 *
 */
typedef struct {
    TUYA_IIC_ROLE_E      role;
    TUYA_IIC_SPEED_E     speed;
    TUYA_IIC_ADDR_MODE_E addr_width;
} TUYA_IIC_BASE_CFG_T;

typedef struct {
    UINT32_T busy             : 1;        ///< Transmitter/Receiver busy flag,1 is busy.
    UINT32_T mode             : 1;        ///< Mode: 0=Slave, 1=Master
    UINT32_T direction        : 1;        ///< Direction: 0=Transmitter, 1=Receiver
    UINT32_T general_call     : 1;        ///< General Call(address 0) indication (cleared on start of next Slave operation)
    UINT32_T arbitration_lost : 1;        ///< Master lost arbitration(in case of multi-masters) (cleared on start of next Master operation)
    UINT32_T bus_error        : 1;        ///< Bus error detected (cleared on start of next Master/Slave operation)
} TUYA_IIC_STATUS_T;

/**
 * @brief i2c irq event
 *
 */
typedef enum {
    TUYA_IIC_EVENT_TRANSFER_DONE        = 0,  ///< Master/Slave Transmit/Receive finished
    TUYA_IIC_EVENT_TRANSFER_INCOMPLETE  = 1,  ///< Master/Slave Transmit/Receive incomplete transfer
    TUYA_IIC_EVENT_SLAVE_TRANSMIT       = 2,  ///< Slave Transmit operation requested
    TUYA_IIC_EVENT_SLAVE_RECEIVE        = 3,  ///< Slave Receive operation requested
    TUYA_IIC_EVENT_ADDRESS_NACK         = 4,  ///< Address not acknowledged from Slave
    TUYA_IIC_EVENT_GENERAL_CALL         = 5,  ///< General Call indication
    TUYA_IIC_EVENT_ARBITRATION_LOST     = 6,  ///< Master lost arbitration
    TUYA_IIC_EVENT_BUS_ERROR            = 7,  ///< Bus error detected (START/STOP at illegal position)
    TUYA_IIC_EVENT_BUS_CLEAR            = 8   ///< Bus clear finished
} TUYA_IIC_IRQ_EVT_E;

typedef VOID_T (*TUYA_I2C_IRQ_CB)(TUYA_I2C_NUM_E port, TUYA_IIC_IRQ_EVT_E event);

/**
 * @brief i2c flag
 * 
 */
// #define TUYA_I2C_FLAG_WR              (1u << 0) // write flag
// #define TUYA_I2C_FLAG_RD              (1u << 1) // read flag
// #define TUYA_I2C_FLAG_ADDR_10BIT      (1u << 2) // 10bits addr mode
// #define TUYA_I2C_FLAG_NO_START        (1u << 3) // no start
// #define TUYA_I2C_FLAG_IGNORE_NACK     (1u << 4) // ignore NACK
// #define TUYA_I2C_FLAG_NO_READ_ACK     (1u << 5) // read without ACK
// #define TUYA_I2C_FLAG_NO_ADDR         (1u << 6) // masg without addr

/**
 * @brief i2c message struct
 * 
 */
// typedef struct {
//     UINT32_T        flags;
//     UINT16_T        addr;
//     UINT16_T        len;
//     UINT8_T         *buff;
// } TUYA_I2C_MSG_T;

/**
 * @brief PWM flag
 * 
 */
typedef enum {
    TUYA_PWM_NUM_0,		    // PWM 0
    TUYA_PWM_NUM_1,		    // PWM 1
    TUYA_PWM_NUM_2,	        // PWM 2
    TUYA_PWM_NUM_3,	        // PWM 3
    TUYA_PWM_NUM_4,	        // PWM 4
    TUYA_PWM_NUM_5,	        // PWM 5
    TUYA_PWM_NUM_MAX,
} TUYA_PWM_NUM_E;

/**
 * @brief pwm polarity
 */
typedef enum {
    TUYA_PWM_NEGATIVE = 0,
    TUYA_PWM_POSITIVE,
} TUYA_PWM_POLARITY_E;

/**
 * @brief pwm count mode
 */
typedef enum {
    TUYA_PWM_CNT_UP = 0,       //default use
    TUYA_PWM_CNT_UP_AND_DOWN,  //can use in duplex complementation mode.
} TUYA_PWM_COUNT_E;

/**
 * @brief pwm config
 */
typedef struct {
    TUYA_PWM_POLARITY_E polarity;
    TUYA_PWM_COUNT_E    count_mode;
    //pulse duty cycle = duty / cycle; exp duty = 5000,cycle = 10000; pulse duty cycle = 50%
    UINT_T              duty;
    UINT_T              cycle;
    UINT_T              frequency;  // (bet: Hz)
} TUYA_PWM_BASE_CFG_T;

/**
 * @brief spi mode
 * 
 */
typedef enum {
    TUYA_SPI_NUM_0,		    // SPI 0
    TUYA_SPI_NUM_1,		    // SPI 1
    TUYA_SPI_NUM_2,	        // SPI 2
    TUYA_SPI_NUM_3,	        // SPI 3
    TUYA_SPI_NUM_4,	        // SPI 4
    TUYA_SPI_NUM_5,	        // SPI 5
    TUYA_SPI_NUM_MAX,
} TUYA_SPI_NUM_E;

typedef enum {
    TUYA_SPI_MODE0  = 0,    // CPOL = 0, CPHA = 0
    TUYA_SPI_MODE1  = 1,    // CPOL = 0, CPHA = 1
    TUYA_SPI_MODE2  = 2,    // CPOL = 1, CPHA = 0
    TUYA_SPI_MODE3  = 3     // CPOL = 1, CPHA = 1
} TUYA_SPI_MODE_E;

/**
 * @brief spi bit order
 * 
 */
typedef enum {
    TUYA_SPI_ORDER_MSB2LSB  = 0,    // SPI Bit order from MSB to LSB
    TUYA_SPI_ORDER_LSB2MSB  = 1,    // SPI Bit order from LSB to MSB
} TUYA_SPI_BIT_ORDER_E;

/*----- SPI Control Codes: Mode -----*/
typedef enum {
    TUYA_SPI_ROLE_INACTIVE,           ///< SPI Inactive
    TUYA_SPI_ROLE_MASTER,             ///< SPI Master (Output on MOSI, Input on MISO); arg = Bus Speed in bps
    TUYA_SPI_ROLE_SLAVE,              ///< SPI Slave  (Output on MISO, Input on MOSI)
    TUYA_SPI_ROLE_MASTER_SIMPLEX,     ///< SPI Master (Output/Input on MOSI); arg = Bus Speed in bps
    TUYA_SPI_ROLE_SLAVE_SIMPLEX       ///< SPI Slave  (Output/Input on MISO)
} TUYA_SPI_ROLE_E;

/**
 * @brief spi databits
 * 
 */
typedef enum {
    TUYA_SPI_DATA_BIT8  = 0,
    TUYA_SPI_DATA_BIT16,
} TUYA_SPI_DATABITS_E;

/**
 * @brief spi cs mode
 * 
 */
typedef enum {
    TUYA_SPI_AUTO_TYPE  = 0,            // hardware auto set
    TUYA_SPI_SOFT_TYPE,                 // software manual set
    TUYA_SPI_SOFT_ONE_WIRE_TYPE,        // software manual set(miso mosi pin multiplexing)
} TUYA_SPI_TYPE_E;

/**
 * @brief spi config
 * 
 */
typedef struct {
    TUYA_SPI_ROLE_E      role;
    TUYA_SPI_MODE_E      mode;
    TUYA_SPI_TYPE_E      type;
    TUYA_SPI_DATABITS_E  databits;
    TUYA_SPI_BIT_ORDER_E bitorder;
    UINT_T               freq_hz;
    UINT_T               spi_dma_flags; /*!< SPI dma format , 1 use dma */ 
} TUYA_SPI_BASE_CFG_T;

/****** SPI Event *****/
typedef enum {
    TUYA_SPI_EVENT_TRANSFER_COMPLETE = 0,   ///< Data Transfer completed. tkl_spi_transfer to indicate that all the data has been transferred. The driver is ready for the next transfer operation
    TUYA_SPI_EVENT_TX_COMPLETE,             ///< Data Transfer completed. Occurs after call to tkl_spi_send to indicate that all the data has been transferred. The driver is ready for the next transfer operation
    TUYA_SPI_EVENT_RX_COMPLETE,             ///< Data Transfer completed. Occurs after call tkl_spi_receive to indicate that all the data has been transferred. The driver is ready for the next transfer operation
    TUYA_SPI_EVENT_DATA_LOST,               ///< Data lost: Receive overflow / Transmit underflow. Occurs in slave mode when data is requested/sent by master but send/receive/transfer operation has not been started and indicates that data is lost. Occurs also in master mode when driver cannot transfer data fast enough.
    TUYA_SPI_EVENT_MODE_FAULT               ///< Master Mode Fault (SS deactivated when Master).Occurs in master mode when Slave Select is deactivated and indicates Master Mode Fault. The driver is ready for the next transfer operation.
}TUYA_SPI_IRQ_EVT_E;

/**
 * @brief spi cb,used in irq mode
 * 
 */
typedef VOID_T (*TUYA_SPI_IRQ_CB)(TUYA_SPI_NUM_E port, TUYA_SPI_IRQ_EVT_E event);

/**
 * @brief SPI Status
 * 
 */
typedef struct {
    UINT32_T busy       : 1;              ///< Transmitter/Receiver busy flag,1 is busy
    UINT32_T data_lost  : 1;              ///< Data lost: Receive overflow / Transmit underflow (cleared on start of transfer operation)
    UINT32_T mode_fault : 1;              ///< Mode fault detected; optional (cleared on start of transfer operation)
} TUYA_SPI_STATUS_T;

/**
 * @brief i2s message struct
 * 
 */

/**
 * @brief i2s num
 */
typedef enum {
    TUYA_I2S_NUM_0 = 0,                 /*!< I2S port 0 */
    TUYA_I2S_NUM_1 = 1,                 /*!< I2S port 1 */
    TUYA_I2S_NUM_MAX,                   /*!< I2S port max */
} TUYA_I2S_NUM_E;

/**
 * @brief I2S Mode
 * USE IN 4 TYPES, MASTER TX,MASTER RX,SLAVE TX,SLAVE RX,
 */
typedef enum {
    TUYA_I2S_MODE_MASTER       = (0x1 << 0),       /*!< Master mode*/
    TUYA_I2S_MODE_SLAVE        = (0x1 << 1),       /*!< Slave mode*/
    TUYA_I2S_MODE_TX           = (0x1 << 2),       /*!< TX mode*/
    TUYA_I2S_MODE_RX           = (0x1 << 3),       /*!< RX mode*/
} TUYA_I2S_MODE_E;

typedef enum {
    TUYA_I2S_BITS_PER_SAMPLE_8BIT    = 8,            /*!< data bit-width: 8 */
    TUYA_I2S_BITS_PER_SAMPLE_16BIT   = 16,           /*!< data bit-width: 16 */
    TUYA_I2S_BITS_PER_SAMPLE_24BIT   = 24,           /*!< data bit-width: 24 */
    TUYA_I2S_BITS_PER_SAMPLE_32BIT   = 32,           /*!< data bit-width: 32 */
} TUYA_I2S_BITS_PER_SAMP_E;

typedef enum {
    TUYA_I2S_CHANNEL_FMT_RIGHT_LEFT,         /*!< Separated left and right channel */
    TUYA_I2S_CHANNEL_FMT_ALL_RIGHT,          /*!< Load right channel data in both two channels */
    TUYA_I2S_CHANNEL_FMT_ALL_LEFT,           /*!< Load left channel data in both two channels */
    TUYA_I2S_CHANNEL_FMT_ONLY_RIGHT,         /*!< Only load data in right channel (mono mode) */
    TUYA_I2S_CHANNEL_FMT_ONLY_LEFT,          /*!< Only load data in left channel (mono mode) */
} TUYA_I2S_CHANNEL_FMT_E;

typedef enum {
    I2S_COMM_FORMAT_STAND_I2S        = 0X01, /*!< I2S communication I2S Philips standard, data launch at second BCK*/
    I2S_COMM_FORMAT_STAND_MSB        = 0X02, /*!< I2S communication MSB alignment standard, data launch at first BCK*/
    I2S_COMM_FORMAT_STAND_PCM_SHORT  = 0x04, /*!< PCM Short standard, also known as DSP mode. The period of synchronization signal (WS) is 1 bck cycle.*/
    I2S_COMM_FORMAT_STAND_PCM_LONG   = 0x0C, /*!< PCM Long standard. The period of synchronization signal (WS) is channel_bit*bck cycles.*/
    I2S_COMM_FORMAT_STAND_MAX,               /*!< standard max*/
} TUYA_I2S_COMM_FORMAT_E;

typedef struct {
    TUYA_I2S_MODE_E             mode;                       /*!< I2S work mode */
    UINT32_T                    sample_rate;                /*!< I2S sample rate */
    UINT32_T                    mclk;                       /*!< I2S mclk */
    TUYA_I2S_BITS_PER_SAMP_E    bits_per_sample;            /*!< I2S sample bits in one channel */
    TUYA_I2S_CHANNEL_FMT_E      channel_format;             /*!< I2S channel format.*/
    TUYA_I2S_COMM_FORMAT_E      communication_format;       /*!< I2S communication format */
    UINT32_T                    i2s_dma_flags;              /*!< I2S dma format , 1 use dma */  
}TUYA_I2S_BASE_CFG_T;

    // 文件访问权限
#define TUYA_IRUSR  0400    /* Read by owner.  */
#define TUYA_IWUSR  0200    /* Write by owner.  */
#define TUYA_IXUSR  0100    /* Execute by owner.  */
     
     
    // 缓冲区搜索起始位置类型
#define TUYA_SEEK_SET   0   /* Seek from beginning of file.  */
#define TUYA_SEEK_CUR   1   /* Seek from current position.  */
#define TUYA_SEEK_END   2   /* Seek from end of file.  */
     
#define TUYA_R_OK       4
#define TUYA_W_OK       2
#define TUYA_X_OK       1
#define TUYA_F_OK       0
 
typedef VOID_T* TUYA_DIR;
typedef VOID_T* TUYA_FILEINFO;
typedef VOID_T* TUYA_FILE; 


/**
 * @brief cpu sleep mode
 */
typedef enum {
    TUYA_CPU_SLEEP,
    TUYA_CPU_DEEP_SLEEP,
} TUYA_CPU_SLEEP_MODE_E;

/**
 * @brief pre-sleep callback
 */
typedef VOID_T (*TUYA_PRE_SLEEP_CB)(VOID_T);

/**
 * @brief post-wakeup callback
 */
typedef VOID_T (*TUYA_POST_WAKEUP_CB)(VOID_T);

/**
 * @brief sleep callback
 */
typedef struct {
    TUYA_PRE_SLEEP_CB pre_sleep_cb;
    TUYA_POST_WAKEUP_CB post_wakeup_cb;
} TUYA_SLEEP_CB_T;

typedef struct {
    UINT32_T use_ratio;         // cpu used ratio
} TUYA_CPU_INFO_T;

#if OPERATING_SYSTEM != SYSTEM_NON_OS
typedef UINT64_T SYS_TICK_T;
typedef UINT64_T SYS_TIME_T;
#else
typedef UINT_T SYS_TICK_T;
typedef UINT_T SYS_TIME_T;
#endif
/*
 *  reasons for restart
 */
typedef enum {
    TUYA_RESET_REASON_POWERON    = 0,  ///< Poweron reset type, supply voltage < power-on threshold (TY_RST_POWER_OFF)
    TUYA_RESET_REASON_HW_WDOG    = 1,  ///< Hardware watchdog reset occurred (TY_RST_HARDWARE_WATCHDOG)
    TUYA_RESET_REASON_FAULT      = 2,  ///< A access fault occurred (TY_RST_FATAL_EXCEPTION)
    TUYA_RESET_REASON_SW_WDOG    = 3,  ///< Software watchdog reset occurred (TY_RST_SOFTWARE_WATCHDOG)
    TUYA_RESET_REASON_SOFTWARE   = 4,  ///< Software triggered reset (TY_RST_SOFTWARE)
    TUYA_RESET_REASON_DEEPSLEEP  = 5,  ///< Reset caused by entering deep sleep (TY_RST_DEEPSLEEP)
    TUYA_RESET_REASON_EXTERNAL   = 6,  ///< External reset trigger        (TY_RST_HARDWARE)
    TUYA_RESET_REASON_UNKNOWN    = 7,  ///< Underterminable cause
    TUYA_RESET_REASON_FIB        = 8,  ///< Reset originated from the FIB bootloader
    TUYA_RESET_REASON_BOOTLOADER = 8,  ///< Reset relates to an bootloader
    TUYA_RESET_REASON_CRASH      = 10, ///< Software crash
    TUYA_RESET_REASON_FLASH      = 11, ///< Flash failure cause reset
    TUYA_RESET_REASON_FATAL      = 12, ///< A non-recoverable fatal error occurred
    TUYA_RESET_REASON_BROWNOUT   = 13, ///< Brown out
    TUYA_RESET_REASON_UNSUPPORT  = 0xFF,
} TUYA_RESET_REASON_E;


/* ota */
#define TUYA_OTA_FILE_MD5_LEN 16

/*
 * ota upgrade type
 */
typedef enum {
    TUYA_OTA_FULL        = 1,            ///< AB area switch, full package upgrade
    TUYA_OTA_DIFF        = 2,            ///< fixed area, difference package upgrade
} TUYA_OTA_TYPE_E;


typedef enum {
    TUYA_OTA_PATH_AIR    = 0,            ///< OTA from Wired/Wi-Fi/Cellular/NBIoT
    TUYA_OTA_PATH_UART   = 1,            ///< OTA from uart for MF
    TUYA_OTA_PATH_BLE    = 2,            ///< OTA from BLE protocol for subdev
    TUYA_OTA_PATH_ZIGBEE = 3,            ///< OTA from Zigbee protocol for subdev
    TUYA_OTA_PATH_SEC_A = 4,             ///< OTA from multi-section A
    TUYA_OTA_PATH_SEC_B = 5,             ///< OTA from multi-section B
    TUYA_OTA_PATH_INVALID = 255          ///< OTA from multi-section invalid
}TUYA_OTA_PATH_E;


typedef struct {
    UINT32_T len;
    UINT32_T crc32;
    UINT8_T  md5[TUYA_OTA_FILE_MD5_LEN];
} TUYA_OTA_FIRMWARE_INFO_T;

/*
 *  ota pack data, write to flash addr(start_addr + offset)
 */
typedef struct {
    UINT_T   total_len;     ///< ota image totle len
    UINT_T   start_addr;    ///< ota flash start addr
    UINT_T   offset;        ///< ota image offset
    UCHAR_T* data;          ///< ota data
    UINT_T   len;           ///< ota data len
    VOID_T*  pri_data;      ///< private pointer
} TUYA_OTA_DATA_T;

/**
 * @brief uart type
 * 
 */
typedef enum {
    TUYA_UART_NUM_0,		    // UART 0
    TUYA_UART_NUM_1,		    // UART 1
    TUYA_UART_NUM_2,	        // UART 2
    TUYA_UART_NUM_3,	        // UART 3
    TUYA_UART_NUM_4,	        // UART 4
    TUYA_UART_NUM_5,	        // UART 5
    TUYA_UART_NUM_MAX,
} TUYA_UART_NUM_E;

typedef enum {
    TUYA_UART_SYS = 0,
    TUYA_UART_USB,
    TUYA_UART_SDIO,
    TUYA_UART_MAX_TYPE,
} TUYA_UART_TYPE_E;

/**
 * @brief uart databits
 * 
 */
typedef enum {
    TUYA_UART_DATA_LEN_5BIT      = 0x05,
    TUYA_UART_DATA_LEN_6BIT      = 0x06,
    TUYA_UART_DATA_LEN_7BIT      = 0x07,
    TUYA_UART_DATA_LEN_8BIT      = 0x08,
} TUYA_UART_DATA_LEN_E;

/**
 * @brief uart stop bits
 * 
 */
typedef enum {
    TUYA_UART_STOP_LEN_1BIT      = 0x01,
    TUYA_UART_STOP_LEN_1_5BIT1   = 0x02,
    TUYA_UART_STOP_LEN_2BIT      = 0x03,
} TUYA_UART_STOP_LEN_E;

/**
 * @brief uart parity
 * 
 */
typedef enum {
    TUYA_UART_PARITY_TYPE_NONE    = 0,
    TUYA_UART_PARITY_TYPE_ODD     = 1,
    TUYA_UART_PARITY_TYPE_EVEN    = 2,
} TUYA_UART_PARITY_TYPE_E;

typedef enum {
    TUYA_UART_FLOWCTRL_NONE = 0,
    TUYA_UART_FLOWCTRL_RTSCTS,
    TUYA_UART_FLOWCTRL_XONXOFF,
    TUYA_UART_FLOWCTRL_DTRDSR,
} TUYA_UART_FLOWCTRL_TYPE_E;

#define TUYA_UART_PORT_ID(port_type, port_num) ((((port_type) << 16) & 0xFFFF0000) | (port_num))
#define TUYA_UART_GET_PORT_TYPE(port_id) (((port_id) >> 16) & 0xFFFF)
#define TUYA_UART_GET_PORT_NUMBER(port_id) ((port_id) & 0xFFFF)

/**
 * @brief uart config
 * 
 */
typedef struct {
    UINT_T                      baudrate;
    TUYA_UART_PARITY_TYPE_E     parity;
    TUYA_UART_DATA_LEN_E        databits;
    TUYA_UART_STOP_LEN_E        stopbits;
    TUYA_UART_FLOWCTRL_TYPE_E   flowctrl;
} TUYA_UART_BASE_CFG_T;


/**
 * @brief uart irq callback
 * 
 * @param[in] port_id: uart port id
 *                     the high 16bit - uart type
 *                                      it's value must be one of the TUYA_UART_TYPE_E type
 *                     the low 16bit - uart port number
 *                     you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @return none
 */
typedef VOID_T (*TUYA_UART_IRQ_CB)(TUYA_UART_NUM_E port_id);

/**
 * @brief uart contrl command
 *
 */
typedef enum{
    TUYA_UART_SUSPEND_CMD = 0,
    TUYA_UART_RESUME_CMD,
    TUYA_UART_FLUSH_CMD,
    TUYA_UART_RECONFIG_CMD,
    TUYA_UART_USER_CMD,
    TUYA_UART_MAX_CMD = 1000
}TUYA_UART_IOCTL_CMD_E;


typedef struct {
    UINT_T interval_ms;
} TUYA_WDOG_BASE_CFG_T;

/**
 * @brief timer num
 * 
 */
typedef enum {
    TUYA_TIMER_NUM_0,		    // TIMER 0
    TUYA_TIMER_NUM_1,		    // TIMER 1
    TUYA_TIMER_NUM_2,	        // TIMER 2
    TUYA_TIMER_NUM_3,	        // TIMER 3
    TUYA_TIMER_NUM_4,	        // TIMER 4
    TUYA_TIMER_NUM_5,	        // TIMER 5
    TUYA_TIMER_NUM_MAX,
} TUYA_TIMER_NUM_E;

#define TUYA_TIMER_CFG(__CFG, __MODE, __CB, __ARG)      \
        (__CFG)->mode = __MODE;                         \
        (__CFG)->cb   = __CB;                           \
        (__CFG)->arg  = __ARG
    
typedef enum {
    TUYA_TIMER_MODE_ONCE = 0,
    TUYA_TIMER_MODE_PERIOD
} TUYA_TIMER_MODE_E;

typedef VOID (*TUYA_TIMER_ISR_CB)(VOID *args);

typedef struct {
    TUYA_TIMER_MODE_E   mode;
    TUYA_TIMER_ISR_CB   cb;
    VOID                *args;
} TUYA_TIMER_BASE_CFG_T;


/**
 * @brief tuya wake source gpio
 */
typedef struct {
    TUYA_GPIO_NUM_E gpio_num;
    TUYA_GPIO_LEVEL_E level;
} TUYA_WAKEUP_SOURCE_GPIO_T;

/**
 * @brief tuya wake source timer
 */
typedef struct {
    TUYA_TIMER_NUM_E timer_num;
    TUYA_TIMER_MODE_E mode;
    UINT32_T ms;
} TUYA_WAKEUP_SOURCE_TIMER_T;

/**
 * @brief tuya wake source
 */
typedef enum {
    TUYA_WAKEUP_SOURCE_GPIO,
    TUYA_WAKEUP_SOURCE_TIMER,
} TUYA_WAKEUP_SOURCE_E;

/**
 * @brief tuya wake source base cfg
 */
typedef struct {
    TUYA_WAKEUP_SOURCE_E source;
    union {
        TUYA_WAKEUP_SOURCE_GPIO_T gpio_param;
        TUYA_WAKEUP_SOURCE_TIMER_T timer_param;
    } wakeup_para;
}TUYA_WAKEUP_SOURCE_BASE_CFG_T;


#ifndef TUYA_FD_MAX_COUNT
#if defined(SYSTEM_LINUX) && (OPERATING_SYSTEM == SYSTEM_LINUX)
/* max fd numbers in linux */
#define TUYA_FD_MAX_COUNT    (1024)
#else
/* max fd numbers in other system */
#define TUYA_FD_MAX_COUNT    (64)
#endif
#endif

typedef INT_T TUYA_OPT_LEVEL;
typedef INT_T TUYA_OPT_NAME;

/* tuyaos definition of fd operations */
typedef struct {
    UINT8_T placeholder[(TUYA_FD_MAX_COUNT+7)/8];
} TUYA_FD_SET_T;

/* tuyaos definition of socket protocol */
typedef enum {
    PROTOCOL_TCP = 0,
    PROTOCOL_UDP = 1,
    PROTOCOL_RAW = 2,
}TUYA_PROTOCOL_TYPE_E;

/* tuyaos definition of transfer type */
typedef enum {
    TRANS_RECV = 0,
    TRANS_SEND = 1,
}TUYA_TRANS_TYPE_E;


/* tuyaos definition of IP addr */
typedef UINT_T TUYA_IP_ADDR_T;

/* tuyaos errorno */
typedef INT_T TUYA_ERRNO;
#define UNW_SUCCESS       0
#define UNW_FAIL          -1
#define UNW_EINTR         -2
#define UNW_EBADF         -3
#define UNW_EAGAIN        -4
#define UNW_ENOMEM        -28
#define UNW_EFAULT        -5
#define UNW_EBUSY         -6
#define UNW_EINVAL        -7
#define UNW_ENFILE        -8
#define UNW_EMFILE        -9
#define UNW_ENOSPC        -10
#define UNW_EPIPE         -11
#define UNW_EWOULDBLOCK   -12
#define UNW_ENOTSOCK      -13
#define UNW_ENOPROTOOPT   -14
#define UNW_EADDRINUSE    -15
#define UNW_EADDRNOTAVAIL -16
#define UNW_ENETDOWN      -17
#define UNW_ENETUNREACH   -18
#define UNW_ENETRESET     -19
#define UNW_ECONNRESET    -20
#define UNW_ENOBUFS       -21
#define UNW_EISCONN       -22
#define UNW_ENOTCONN      -23
#define UNW_ETIMEDOUT     -24
#define UNW_ECONNREFUSED  -25
#define UNW_EHOSTDOWN     -26
#define UNW_EHOSTUNREACH  -27
#define UNW_EMSGSIZE      -29
#define TUYA_ERRNO_NOT_SUPPORT 255

/** 
* tkl thread priority define 
**/

#define TKL_THREAD_PRI_HIGHEST      8
#define TKL_THREAD_PRI_HIGH         7
#define TKL_THREAD_PRI_BELOW_HIGH   6
#define TKL_THREAD_PRI_ABOVE_NORMAL 5
#define TKL_THREAD_PRI_NORMAL       4
#define TKL_THREAD_PRI_BELOW_NORML  3
#define TKL_THREAD_PRI_ABOVE_LOW    2
#define TKL_THREAD_PRI_LOW          1
#define TKL_THREAD_PRI_LOWEST       0

#ifdef __cplusplus
}
#endif

#endif // TUYA_CLOUD_TYPES_H

