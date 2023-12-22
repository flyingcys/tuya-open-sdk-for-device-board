/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of http://www.FreeRTOS.org for more information.
 */
#include "include.h"
#include "mem_pub.h"

#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

#include "tuya_mem_heap.h"
static HEAP_HANDLE s_heap_handle = NULL;
//static HEAP_HANDLE s_heap_tss_handle = NULL;

extern void bk_printf(const char *fmt, ...);

/*-----------------------------------------------------------*/

extern unsigned char _empty_ram;
extern unsigned char _tcmbss_end;

#define TCMBSS_START_ADDRESS (void*)&_tcmbss_end
#define TCMBSS_END_ADDRESS   (void*)(0x003F0000 + 60*1024 - 512)

#define HEAP_START_ADDRESS    (void*)&_empty_ram
#if (CFG_SOC_NAME == SOC_BK7231N)
#define HEAP_END_ADDRESS      (void*)(0x00400000 + 192 * 1024)
#else
#define HEAP_END_ADDRESS      (void*)(0x00400000 + 256 * 1024)
#endif

static void *prvHeapGetHeaderPointer(void)
{
	return (void *)HEAP_START_ADDRESS;
}

static uint32_t prvHeapGetTotalSize(void)
{
	ASSERT(HEAP_END_ADDRESS > HEAP_START_ADDRESS);
	return (HEAP_END_ADDRESS - HEAP_START_ADDRESS);
}

static void prvHeapInit( void )
{
    int ret = 0;
    heap_context_t ctx = {0};
    ctx.dbg_output = bk_printf;
    ctx.enter_critical = vTaskSuspendAll;
    ctx.exit_critical = xTaskResumeAll;
    ret = tuya_mem_heap_init(&ctx);
    if(0 != ret) {
        bk_printf("--------->heap init err:%d", ret);
        return;
    }

    // ret = tuya_mem_heap_create(TCMBSS_START_ADDRESS, TCMBSS_END_ADDRESS-TCMBSS_START_ADDRESS, &s_heap_tss_handle);
    // if(0 != ret) {
    //     bk_printf("--------->heap create tss err:%d", ret);
    // }

    ret = tuya_mem_heap_create(prvHeapGetHeaderPointer(), prvHeapGetTotalSize(), &s_heap_handle);
    if(0 != ret) {
        bk_printf("--------->heap create err:%d", ret);
    }
}

void *pvPortMalloc( size_t xWantedSize )
{
    if(NULL == s_heap_handle) {
        prvHeapInit();
    }

    if(xWantedSize == 0) {
        xWantedSize = 4;
    }

	return tuya_mem_heap_malloc(0, xWantedSize);
}


void vPortFree( void *pv )
{
    tuya_mem_heap_free(0, pv);
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
	return tuya_mem_heap_available(0);
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
	return 0;
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
	/* This just exists to keep the linker quiet. */
}

void *pvPortRealloc( void *pv, size_t xWantedSize )
{
	return tuya_mem_heap_realloc(0, pv, xWantedSize);
}

