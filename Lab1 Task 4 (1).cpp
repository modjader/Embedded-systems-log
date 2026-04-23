/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);

int main() {
    
    for (int i = 0; i < 5; i++) {
        led1 = led2 = led3 = 1;
        ThisThread::sleep_for(200ms);

        led1 = led2 = led3 = 0;
        ThisThread::sleep_for(200ms);
    }

    
    led1 = 1;
    led2 = 0;
    led3 = 0;

    while (true) {
    
    }
}