#ifndef _TKL_WIFI_LOG_H_
#define _TKL_WIFI_LOG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LEVEL_OVER_LOGN = 0,
    LEVEL_OVER_LOGE = 1,
    LEVEL_OVER_LOGW,
    LEVEL_OVER_LOGI,
    LEVEL_OVER_LOGD,
    LEVEL_OVER_LOGV,
};

#define DEBUG_STR     "%d, %s: "
#define DEBUG_ARGS    __LINE__,__FUNCTION__

#ifndef DEBUG_LEVEL
#define TNLOG_DEBUG_LEVEL LEVEL_OVER_LOGD
#else
#define TNLOG_DEBUG_LEVEL LEVEL_OVER_LOGV       // DEBUG_LEVEL
#endif

#define TKL_LOGE(format,arg ...)                                \
do{                                                             \
    printf("ERROR: " DEBUG_STR format "\n", DEBUG_ARGS, ##arg); \
} while (0)

#define TKL_LOGW(format,arg ...)                                \
do{                                                             \
    if (TNLOG_DEBUG_LEVEL >= LEVEL_OVER_LOGW)                   \
        printf("WARN:"DEBUG_STR format "\n", DEBUG_ARGS, ##arg);\
} while (0)

#define TKL_LOGI(format,arg ...)                                \
do{                                                             \
    if (TNLOG_DEBUG_LEVEL >= LEVEL_OVER_LOGI)                   \
        printf(DEBUG_STR format "\n", DEBUG_ARGS, ##arg);       \
} while (0)

#define TKL_LOGD(format,arg ...)                                \
do{                                                             \
    if (TNLOG_DEBUG_LEVEL >= LEVEL_OVER_LOGD)                   \
        printf(DEBUG_STR format "\n", DEBUG_ARGS, ##arg);       \
} while (0)

#define TKL_LOGV(format,arg ...)                                \
do{                                                             \
    if (TNLOG_DEBUG_LEVEL >= LEVEL_OVER_LOGV)                   \
        printf(DEBUG_STR format "\n", DEBUG_ARGS, ##arg);       \
} while (0)

#ifdef __cplusplus
}
#endif

#endif/* __TKL_WIFI_LOG_H__ */

