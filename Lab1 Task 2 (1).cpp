/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
Ticker ticker;

int counter1 = 0, counter2 = 0, counter3 = 0;
const int LED1_TICKS = 2;
const int LED2_TICKS = 1;
const int LED3_TICKS = 4;

void updateLEDs() {
    if (++counter1 >= LED1_TICKS) { led1 = !led1; counter1 = 0; }
    if (++counter2 >= LED2_TICKS) { led2 = !led2; counter2 = 0; }
    if (++counter3 >= LED3_TICKS) { led3 = !led3; counter3 = 0; }
}

int main() {
    ticker.attach(&updateLEDs, 500ms);
    while (true) {
        // Main loop can perform other tasks
    }
}
