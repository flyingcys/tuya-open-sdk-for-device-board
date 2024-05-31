
#ifndef MBEDTLS_THREADING_ALT_H
#define MBEDTLS_THREADING_ALT_H

typedef struct mbedtls_threading_mutex_t {
    void * mutex;
    char is_valid;
} mbedtls_threading_mutex_t;

#endif /* threading_alt.h */

