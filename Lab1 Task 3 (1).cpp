/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);

DigitalOut* leds[] = {&led1, &led2, &led3};

int main() {
    int index = 0;
    int direction = 1;

    while (true) {
        // Turn all LEDs off
        led1 = led2 = led3 = 0;

        // Turn current LED on
        *leds[index] = 1;
        ThisThread::sleep_for(200ms);

        // Turn it off
        *leds[index] = 0;
        ThisThread::sleep_for(200ms);

        // Move index
        index += direction;

        // Reverse at ends
        if (index == 2 || index == 0) {
            direction = -direction;
        }
    }
}