/**
 ****************************************************************************************
 *
 * @file arch_main.c
 *
 * @brief Main loop of the application.
 *
 * Copyright (C) Beken Corp 2011-2020
 *
 ****************************************************************************************
 */
#include "include.h"
#include "driver_pub.h"
#include "func_pub.h"
#include "app.h"
#include "uart.h"
#include "arm_arch.h"
#include "ate_app.h"
#include "bk7011_cal_pub.h"
#include "start_type_pub.h"
#include "wdt_pub.h"
#include "icu_pub.h"

beken_semaphore_t extended_app_sema = NULL;
#define  EXTENDED_APP_STACK_SIZE   2048

extern void user_main_entry(void);

void extended_app_launch_over(void)
{  
	OSStatus ret;
	ret = rtos_set_semaphore(&extended_app_sema);
	
	(void)ret;
}
    
void extended_app_waiting_for_launch(void)
{
	OSStatus ret;

	ret = rtos_get_semaphore(&extended_app_sema, BEKEN_WAIT_FOREVER);
	ASSERT(kNoErr == ret);
	
	(void)ret;
}

static void extended_app_task_handler(void *arg)
{
    /* step 0: function layer initialization*/
    func_init_extended();  
	
#ifdef VCS_RELEASE_VERSION
	bk_printf("Version:%s\r\n",VCS_RELEASE_VERSION);
#endif

    /* step 1: startup application layer*/
	#if CFG_ENABLE_ATE_FEATURE
    if(get_ate_mode_state())
    {
	    ate_start();
    }
    else
	#endif // CFG_ENABLE_ATE_FEATURE
    {
	    app_start();
    }
         
	extended_app_launch_over();
	
    rtos_delete_thread( NULL );
}

void extended_app_launch(void)
{
	OSStatus ret;
	
    ret = rtos_init_semaphore(&extended_app_sema, 1);
	ASSERT(kNoErr == ret);

	ret = rtos_create_thread(NULL,
					   THD_EXTENDED_APP_PRIORITY,
					   "extended_app",
					   (beken_thread_function_t)extended_app_task_handler,
					   EXTENDED_APP_STACK_SIZE,
					   (beken_thread_arg_t)0);
	ASSERT(kNoErr == ret);
}


// #include "driver_sys_ctrl.h"
#define REG_ICU_BASE_ADDR                   (0x00802000UL)
#define REG_ICU_INT_ENABLE_ADDR             (REG_ICU_BASE_ADDR + 0x10 * 4)
#define REG_ICU_INT_GLOBAL_ENABLE_ADDR      (REG_ICU_BASE_ADDR + 0x11 * 4)
#define REG_ICU_INT_RAW_STATUS_ADDR         (REG_ICU_BASE_ADDR + 0x12 * 4)
#define REG_ICU_INT_STATUS_ADDR             (REG_ICU_BASE_ADDR + 0x13 * 4)

#define ATE_START_ADDR                      0x120000

void intc_disable_top(void)
{
    u32 reg;
    u32 param;

    param = 0xffffffff;
    reg = REG_READ(REG_ICU_INT_GLOBAL_ENABLE_ADDR);
    reg &= ~(param);
    REG_WRITE(REG_ICU_INT_GLOBAL_ENABLE_ADDR, reg);
}

void intc_forbidden_all(void)
{
    REG_WRITE(REG_ICU_INT_ENABLE_ADDR, 0);
    REG_WRITE(REG_ICU_INT_GLOBAL_ENABLE_ADDR, 0);
    REG_WRITE(REG_ICU_INT_RAW_STATUS_ADDR, REG_READ(REG_ICU_INT_RAW_STATUS_ADDR));
    REG_WRITE(REG_ICU_INT_STATUS_ADDR, REG_READ(REG_ICU_INT_STATUS_ADDR));
}

void sys_forbidden_interrupts(void)
{
    // __disable_irq();
    portDISABLE_IRQ();
    intc_disable_top();
    intc_forbidden_all();
}

void jump_pc_address(u32 ptr)
{
    typedef void (*jump_point)(void);
    jump_point jump = (jump_point)ptr;

    (* jump)();
}

void delay_xtime(u32 time)
{
	u32 i, j;
	for (i=0; i<time; i++){
		for (j=0; j<26000; j++);
	}
}

#include "gpio_pub.h"
#define ATE_GPIO_ID            GPIO21   /* jtag tms pin*/
uint32_t get_ate_state(void)
{
    uint32_t ret;
    uint32_t param;

    param = GPIO_CFG_PARAM(ATE_GPIO_ID, GMODE_INPUT_PULLUP);
    gpio_ctrl( CMD_GPIO_CFG, &param);

    param = ATE_GPIO_ID;
    ret = gpio_ctrl( CMD_GPIO_INPUT, &param);

    return (0 == ret);
}

void entry_main(void)
{
    /* enable ATE since TUYA using GPIO21 as check port */
    if(get_ate_state())
   	{
        sys_forbidden_interrupts();
        /*jump to ate */
        jump_pc_address(ATE_START_ADDR);   
    }

    bk_misc_init_start_type();
    bk_misc_check_start_type();
	{
		INT32 param = PWD_ARM_WATCHDOG_CLK_BIT;
		icu_ctrl(CMD_CLK_PWR_UP, (void *)&param);
		param = 2000;
		wdt_ctrl(WCMD_SET_PERIOD, &param);
	}

    /* step 1: driver layer initialization*/
    driver_init();
	func_init_basic();
    {
		INT32 param = 60000; //UPDATE PERIOD TO 60s as default
		wdt_ctrl(WCMD_SET_PERIOD, &param);
	}
    bk_wdg_finalize();

	user_main_entry();
    
	extended_app_launch();
	
#if (!CFG_SUPPORT_RTT)
    vTaskStartScheduler();
#endif
}
// eof

