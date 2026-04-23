#include "mbed.h"

// --- Hardware Setup ---
DigitalIn buttons[] = {
    DigitalIn(D2, PullDown), DigitalIn(D3, PullDown), 
    DigitalIn(D4, PullDown), DigitalIn(D5, PullDown), 
    DigitalIn(D6, PullDown), DigitalIn(D7, PullDown)
};

DigitalOut led1(LED1); // Warning / Counter Bit 0
DigitalOut led2(LED2); // Lockdown Steady / Counter Bit 1
DigitalOut led3(LED3); // Lockdown Blink / Counter Bit 2

// --- System Constants ---
const int USER_CODE[4]  = {1, 2, 3, 4};
const int ADMIN_CODE[4] = {2, 2, 2, 2};

enum SystemState { NORMAL, WARNING, AFTER_WARNING, LOCKDOWN };
SystemState currentState = NORMAL;

// --- Global Variables ---
int enteredCode[4];
int digitPointer = 0;
int consecutiveFailures = 0;
int totalLockdowns = 0;

Timer stateTimer;
Timer blinkTimer;

// --- Helper Functions ---

void resetInput() {
    digitPointer = 0;
    for(int i = 0; i < 4; i++) enteredCode[i] = -1;
}

void showLockdownCount() {
    // Displays the lockdown count in binary on the 3 LEDs
    led1 = (totalLockdowns & 1);
    led2 = (totalLockdowns & 2);
    led3 = (totalLockdowns & 4);
}

void clearLEDs() {
    led1 = 0; led2 = 0; led3 = 0;
}

int getButtonPress() {
    for (int i = 0; i < 6; i++) {
        if (buttons[i].read() == 1) {
            // Debounce and wait for release
            ThisThread::sleep_for(150ms);
            while (buttons[i].read() == 1); 
            return i;
        }
    }
    return -1;
}

bool validateCode(const int secret[4]) {
    for (int i = 0; i < 4; i++) {
        if (enteredCode[i] != secret[i]) return false;
    }
    return true;
}

// --- State Transition Logic ---

void transitionTo(SystemState newState) {
    currentState = newState;
    resetInput();
    clearLEDs();
    stateTimer.stop();
    stateTimer.reset();
    stateTimer.start();
    blinkTimer.stop();
    blinkTimer.reset();
    blinkTimer.start();
}

int main() {
    // Initial State
    showLockdownCount();
    
    while (true) {
        switch (currentState) {

            case NORMAL:
            case AFTER_WARNING: {
                showLockdownCount(); // Keep count visible
                int key = getButtonPress();
                if (key != -1) {
                    enteredCode[digitPointer++] = key;
                    // Visual feedback for press
                    led2 = 1; ThisThread::sleep_for(50ms); led2 = 0;
                }

                if (digitPointer == 4) {
                    if (validateCode(USER_CODE)) {
                        consecutiveFailures = 0;
                        transitionTo(NORMAL);
                        // Brief green-like success signal
                        led1 = 1; ThisThread::sleep_for(1s); led1 = 0;
                    } else {
                        consecutiveFailures++;
                        led3 = 1; ThisThread::sleep_for(500ms); led3 = 0; // Error signal

                        if (consecutiveFailures == 3) {
                            transitionTo(WARNING);
                        } else if (consecutiveFailures >= 4) {
                            totalLockdowns++;
                            transitionTo(LOCKDOWN);
                        } else {
                            resetInput();
                        }
                    }
                }
                break;
            }

            case WARNING: {
                // Requirement: Slowly blinking LED for 30s, inputs blocked
                if (blinkTimer.elapsed_time() > 500ms) {
                    led1 = !led1;
                    blinkTimer.reset();
                }
                if (stateTimer.elapsed_time() > 30s) {
                    transitionTo(AFTER_WARNING);
                }
                break;
            }

            case LOCKDOWN: {
                // Requirement: One LED steady, another blinks for 1 minute
                led2 = 1; // Steady LED
                
                // Blink LED3 only for the first 60 seconds
                if (stateTimer.elapsed_time() < 60s) {
                    if (blinkTimer.elapsed_time() > 200ms) {
                        led3 = !led3;
                        blinkTimer.reset();
                    }
                } else {
                    led3 = 0; // Stop blinking after 1 minute
                }

                // Check for Admin Override
                int key = getButtonPress();
                if (key != -1) {
                    enteredCode[digitPointer++] = key;
                }

                if (digitPointer == 4) {
                                                                                                                                                                     if (validateCode(ADMIN_CODE)) {
                        consecutiveFailures = 0;
                        transitionTo(NORMAL);
                    } else {
                        resetInput(); // Failed admin attempt
                    }
                }
                break;
            }
        }
        ThisThread::sleep_for(10ms); // System stability
    }
}