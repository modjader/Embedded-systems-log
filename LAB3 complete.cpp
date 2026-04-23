//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

//=====[Declaration and initialization of public global objects]===============

DigitalIn enterButton(BUTTON1);
DigitalIn gasDetector(D2);
DigitalIn overTempDetector(D3);
DigitalIn aButton(D4);
DigitalIn bButton(D5);
DigitalIn cButton(D6);
DigitalIn dButton(D7);

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

//=====[Declaration and initialization of public global variables]=============

bool alarmState = OFF;
int numberOfIncorrectCodes = 0;

// New variables for simulated alarms and monitoring mode
bool simulatedGasAlarm = false;
bool simulatedTempAlarm = false;
bool monitoringMode = false;

// Timer for periodic monitoring messages
Timer monitoringTimer;
const int MONITORING_INTERVAL_MS = 2000;

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();

void alarmActivationUpdate();
void alarmDeactivationUpdate();

void uartTask();
void availableCommands();

// New function prototypes
void sendGasAlarmState();
void sendTempAlarmState();
void checkAndSendAlarmWarnings();
void resetAlarms();

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    inputsInit();
    outputsInit();
    monitoringTimer.start();
    
    while (true) {
        alarmActivationUpdate();
        alarmDeactivationUpdate();
        uartTask();
        
        // Periodic monitoring message if enabled
        if (monitoringMode) {
            if (monitoringTimer.elapsed_time() >= std::chrono::milliseconds(MONITORING_INTERVAL_MS)) {
                char msg[60];
                sprintf(msg, "Gas: %s, Temp: %s\r\n",
                        simulatedGasAlarm ? "ACTIVE" : "CLEAR",
                        simulatedTempAlarm ? "ACTIVE" : "CLEAR");
                uartUsb.write(msg, strlen(msg));
                monitoringTimer.reset();
            }
        }
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    gasDetector.mode(PullDown);
    overTempDetector.mode(PullDown);
    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    dButton.mode(PullDown);
}

void outputsInit()
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

void alarmActivationUpdate()
{
    if ( gasDetector || overTempDetector ) {
        alarmState = ON;
    }
    alarmLed = alarmState;
}

void alarmDeactivationUpdate()
{
    if ( numberOfIncorrectCodes < 5 ) {
        if ( aButton && bButton && cButton && dButton && !enterButton ) {
            incorrectCodeLed = OFF;
        }
        if ( enterButton && !incorrectCodeLed && alarmState ) {
            if ( aButton && bButton && !cButton && !dButton ) {
                alarmState = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                incorrectCodeLed = ON;
                numberOfIncorrectCodes = numberOfIncorrectCodes + 1;
            }
        }
    } else {
        systemBlockedLed = ON;
    }
}

void uartTask()
{
    char receivedChar = '\0';
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
        
        switch (receivedChar) {
            case '1':   // Toggle gas alarm simulation
                simulatedGasAlarm = !simulatedGasAlarm;
                if (simulatedGasAlarm) {
                    uartUsb.write("WARNING: GAS DETECTED\r\n", 23);
                } else {
                    uartUsb.write("Gas alarm simulation OFF\r\n", 24);
                }
                break;
                
            case '2':   // Request current gas alarm state
                sendGasAlarmState();
                break;
                
            case '3':   // Request current over-temperature alarm state
                sendTempAlarmState();
                break;
                
            case '4':   // Toggle over-temperature alarm simulation
                simulatedTempAlarm = !simulatedTempAlarm;
                if (simulatedTempAlarm) {
                    uartUsb.write("WARNING: TEMPERATURE TOO HIGH\r\n", 30);
                } else {
                    uartUsb.write("Temperature alarm simulation OFF\r\n", 31);
                }
                break;
                
            case '5':   // Acknowledge/Reset both alarms
                resetAlarms();
                break;
                
            case '6':   // Enter/Exit continuous monitoring mode
                monitoringMode = !monitoringMode;
                if (monitoringMode) {
                    uartUsb.write("Monitoring mode ENABLED\r\n", 25);
                    monitoringTimer.reset();
                } else {
                    uartUsb.write("Monitoring mode DISABLED\r\n", 26);
                }
                break;
                
            default:
                availableCommands();
                break;
        }
        
        // Check and send immediate warnings when alarms become active
        checkAndSendAlarmWarnings();
    }
}

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "1: Toggle gas alarm simulation\r\n", 32 );
    uartUsb.write( "2: Get gas alarm state\r\n", 24 );
    uartUsb.write( "3: Get temperature alarm state\r\n", 31 );
    uartUsb.write( "4: Toggle temperature alarm simulation\r\n", 39 );
    uartUsb.write( "5: Acknowledge/Reset all alarms\r\n", 32 );
    uartUsb.write( "6: Toggle continuous monitoring mode\r\n\r\n", 39 );
}

// New function implementations
void sendGasAlarmState()
{
    if (simulatedGasAlarm) {
        uartUsb.write("GAS ALARM ACTIVE\r\n", 18);
    } else {
        uartUsb.write("GAS ALARM CLEAR\r\n", 17);
    }
}

void sendTempAlarmState()
{
    if (simulatedTempAlarm) {
        uartUsb.write("TEMP ALARM ACTIVE\r\n", 19);
    } else {
        uartUsb.write("TEMP ALARM CLEAR\r\n", 18);
    }
}

void checkAndSendAlarmWarnings()
{
    // This function is called after each command to provide immediate feedback
    // The warnings are already sent when toggling, but we could also check here
    // for any external changes (none in this simulation).
}

void resetAlarms()
{
    simulatedGasAlarm = false;
    simulatedTempAlarm = false;
    uartUsb.write("ALARMS RESET\r\n", 14);
}