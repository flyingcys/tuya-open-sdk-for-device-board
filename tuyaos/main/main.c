#include <stdio.h>
#include "driver/gpio.h"
#include "driver/uart.h"

extern void tuya_app_main(void);
void app_main(void)
{
    tuya_app_main();
}
