/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);   // Added
DigitalOut led3(LED3);   // Added

int main() {
    while (true) {
        led1 = !led1;
        led2 = !led2;    // Added
        led3 = !led3;    // Added
        ThisThread::sleep_for(500ms);
    }
}
