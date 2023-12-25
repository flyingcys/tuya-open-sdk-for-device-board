/**
 * @file tkl_queue.c
 * @brief the default weak implements of tuya os queue, this implement only used when OS=linux
 * @version 0.1
 * @date 2019-08-15
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct rpa_queue_t {
    VOID_T **data;
    volatile UINT32_T nelts; /**< # elements */
    UINT32_T in;             /**< next empty location */
    UINT32_T out;            /**< next filled location */
    UINT32_T bounds;         /**< max size of queue */
    UINT32_T full_waiters;
    UINT32_T empty_waiters;
    pthread_mutex_t *one_big_mutex;
    pthread_cond_t *not_empty;
    pthread_cond_t *not_full;
    INT_T terminated;
} rpa_queue_t;

#define RPA_WAIT_NONE    0
#define RPA_WAIT_FOREVER -1

#define rpa_queue_full(queue)  ((queue)->nelts == (queue)->bounds)
#define rpa_queue_empty(queue) ((queue)->nelts == 0)

STATIC VOID_T set_timeout(struct timespec *abstime, INT_T wait_ms)
{
    clock_gettime(CLOCK_REALTIME, abstime);
    /* add seconds */
    abstime->tv_sec += (wait_ms / 1000);
    /* add and carry microseconds */
    long ms = abstime->tv_nsec / 1000000L;
    ms += wait_ms % 1000;
    while (ms > 1000) {
        ms -= 1000;
        abstime->tv_sec += 1;
    }
    abstime->tv_nsec = ms * 1000000L;
}

STATIC VOID_T rpa_queue_destroy(rpa_queue_t *queue)
{
    /* Ignore errors here, we can't do anything about them anyway. */
    pthread_cond_destroy(queue->not_empty);
    pthread_cond_destroy(queue->not_full);
    pthread_mutex_destroy(queue->one_big_mutex);
}

STATIC BOOL_T rpa_queue_create(rpa_queue_t **q, UINT32_T queue_capacity)
{
    rpa_queue_t *queue;
    queue = malloc(sizeof(rpa_queue_t));
    if (!queue) {
        return false;
    }
    *q = queue;
    memset(queue, 0, sizeof(rpa_queue_t));

    if (!(queue->one_big_mutex = malloc(sizeof(pthread_mutex_t))))
        return false;
    if (!(queue->not_empty = malloc(sizeof(pthread_cond_t))))
        return false;
    if (!(queue->not_full = malloc(sizeof(pthread_cond_t))))
        return false;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    INT_T rv = pthread_mutex_init(queue->one_big_mutex, &attr);
    if (rv != 0) {
        goto error;
    }

    rv = pthread_cond_init(queue->not_empty, NULL);
    if (rv != 0) {
        goto error;
    }

    rv = pthread_cond_init(queue->not_full, NULL);
    if (rv != 0) {
        goto error;
    }

    /* Set all the data in the queue to NULL */
    queue->data          = malloc(queue_capacity * sizeof(VOID_T *));
    queue->bounds        = queue_capacity;
    queue->nelts         = 0;
    queue->in            = 0;
    queue->out           = 0;
    queue->terminated    = 0;
    queue->full_waiters  = 0;
    queue->empty_waiters = 0;

    return true;

error:
    free(queue);
    return false;
}

STATIC BOOL_T rpa_queue_trypush(rpa_queue_t *queue, VOID_T *data)
{
    BOOL_T rv;

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_full(queue)) {
        rv = pthread_mutex_unlock(queue->one_big_mutex);
        return false; // EAGAIN;
    }

    queue->data[queue->in] = data;
    queue->in++;
    if (queue->in >= queue->bounds) {
        queue->in -= queue->bounds;
    }
    queue->nelts++;

    if (queue->empty_waiters) {
        rv = pthread_cond_signal(queue->not_empty);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

STATIC BOOL_T rpa_queue_timedpush(rpa_queue_t *queue, VOID_T *data, INT_T wait_ms)
{
    BOOL_T rv;

    if (wait_ms == RPA_WAIT_NONE)
        return rpa_queue_trypush(queue, data);

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_full(queue)) {
        if (!queue->terminated) {
            queue->full_waiters++;
            if (wait_ms == RPA_WAIT_FOREVER) {
                rv = pthread_cond_wait(queue->not_full, queue->one_big_mutex);
            } else {
                struct timespec abstime;
                set_timeout(&abstime, wait_ms);
                rv = pthread_cond_timedwait(queue->not_full, queue->one_big_mutex,
                                            &abstime);
            }
            queue->full_waiters--;
            if (rv != 0) {
                pthread_mutex_unlock(queue->one_big_mutex);
                return false;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (rpa_queue_full(queue)) {
            rv = pthread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return false;
            }
            if (queue->terminated) {
                return false; /* no more elements ever again */
            } else {
                return false; // EINTR;
            }
        }
    }

    queue->data[queue->in] = data;
    queue->in++;
    if (queue->in >= queue->bounds) {
        queue->in -= queue->bounds;
    }
    queue->nelts++;

    if (queue->empty_waiters) {
        rv = pthread_cond_signal(queue->not_empty);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

STATIC BOOL_T rpa_queue_trypop(rpa_queue_t *queue, VOID_T **data)
{
    BOOL_T rv;

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_empty(queue)) {
        rv = pthread_mutex_unlock(queue->one_big_mutex);
        return false; // EAGAIN;
    }

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out++;
    if (queue->out >= queue->bounds) {
        queue->out -= queue->bounds;
    }
    if (queue->full_waiters) {
        rv = pthread_cond_signal(queue->not_full);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

STATIC BOOL_T rpa_queue_timedpop(rpa_queue_t *queue, VOID_T **data, INT_T wait_ms)
{
    BOOL_T rv;

    if (wait_ms == RPA_WAIT_NONE)
        return rpa_queue_trypop(queue, data);

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    /* Keep waiting until we wake up and find that the queue is not empty. */
    if (rpa_queue_empty(queue)) {
        if (!queue->terminated) {
            queue->empty_waiters++;
            if (wait_ms == RPA_WAIT_FOREVER) {
                rv = pthread_cond_wait(queue->not_empty, queue->one_big_mutex);
            } else {
                struct timespec abstime;
                set_timeout(&abstime, wait_ms);
                rv = pthread_cond_timedwait(queue->not_empty, queue->one_big_mutex,
                                            &abstime);
            }
            queue->empty_waiters--;
            if (rv != 0) {
                pthread_mutex_unlock(queue->one_big_mutex);
                return false;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (rpa_queue_empty(queue)) {
            rv = pthread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return false;
            }
            if (queue->terminated) {
                return false; /* no more elements ever again */
            } else {
                return false; // EINTR;
            }
        }
    }

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out++;
    if (queue->out >= queue->bounds) {
        queue->out -= queue->bounds;
    }
    if (queue->full_waiters) {
        rv = pthread_cond_signal(queue->not_full);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

typedef struct {
    rpa_queue_t *queue;
    INT_T msgsize;
} TKL_QUEUE_T;

/**
 * @brief Create message queue
 *
 * @param[in] msgsize message size
 * @param[in] msgcount message number
 * @param[out] queue the queue handle created
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_queue_create_init(TKL_QUEUE_HANDLE *handle, INT_T msgsize, INT_T msgcount)
{
    TKL_QUEUE_T *queue = NULL;

    if ((NULL == handle) || (0 == msgsize) || (0 == msgcount)) {
        return OPRT_INVALID_PARM;
    }

    queue = (TKL_QUEUE_T *)malloc(SIZEOF(TKL_QUEUE_T));
    if (!queue) {
        return OPRT_MALLOC_FAILED;
    }

    if (!rpa_queue_create(&queue->queue, msgcount)) {
        return OPRT_OS_ADAPTER_QUEUE_CREAT_FAILED;
    }
    queue->msgsize = msgsize;

    *handle = (TKL_QUEUE_HANDLE)queue;

    return OPRT_OK;
}

/**
 * @brief post a message to the message queue
 *
 * @param[in] queue the handle of the queue
 * @param[in] data the data of the message
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_queue_post(CONST TKL_QUEUE_HANDLE handle, VOID_T *data, UINT_T timeout)
{
    if (NULL == handle || NULL == data) {
        return OPRT_INVALID_PARM;
    }

    TKL_QUEUE_T *queue = (TKL_QUEUE_T *)handle;
    INT_T wait_ms      = 0;

    VOID_T *buf = (VOID_T *)malloc(queue->msgsize);
    if (NULL == buf) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(buf, (VOID_T *)data, queue->msgsize);

    if (timeout == TKL_QUEUE_WAIT_FROEVER) {
        wait_ms = RPA_WAIT_FOREVER;
    } else {
        wait_ms = timeout;
    }

    if (!rpa_queue_timedpush(queue->queue, buf, wait_ms)) {
        return OPRT_OS_ADAPTER_QUEUE_SEND_FAIL;
    }

    return OPRT_OK;
}

/**
 * @brief fetch message from the message queue
 *
 * @param[in] queue the message queue handle
 * @param[inout] msg the message fetch form the message queue
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
TUYA_WEAK_ATTRIBUTE OPERATE_RET tkl_queue_fetch(CONST TKL_QUEUE_HANDLE handle, VOID_T *msg, UINT_T timeout)
{
    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    TKL_QUEUE_T *queue = (TKL_QUEUE_T *)handle;
    VOID_T *buf        = NULL;
    INT_T wait_ms;

    if (timeout == TKL_QUEUE_WAIT_FROEVER) {
        wait_ms = RPA_WAIT_FOREVER;
    } else {
        wait_ms = timeout;
    }

    if (!rpa_queue_timedpop(queue->queue, (VOID_T **)&buf, wait_ms)) {
        return OPRT_OS_ADAPTER_QUEUE_RECV_FAIL;
    }

    if (buf) {
        memcpy((VOID_T *)msg, buf, queue->msgsize);
        free(buf);
    }

    return OPRT_OK;
}

/**
 * @brief free the message queue
 *
 * @param[in] queue the message queue handle
 *
 * @return VOID_T
 */
TUYA_WEAK_ATTRIBUTE VOID_T tkl_queue_free(CONST TKL_QUEUE_HANDLE handle)
{
    if (NULL == handle) {
        return;
    }

    TKL_QUEUE_T *queue = (TKL_QUEUE_T *)handle;
    rpa_queue_destroy(queue->queue);
    free(queue);
}
