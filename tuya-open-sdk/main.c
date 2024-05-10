/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"

extern tuya_app_main();
int main() {
    stdio_init_all();
    tuya_app_main();
    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
