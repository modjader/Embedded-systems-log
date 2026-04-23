#include "mbed.h"
#include "arm_book_lib.h"
#include "display.h"
#include "matrix_keypad.h"
#include <string.h>
#include <stdio.h>

// ============================================
// CONFIGURATION PARAMETERS
// ============================================
#define LOOP_DELAY_MS               10
#define STATUS_UPDATE_INTERVAL_MS   60000
#define TEMP_SAFE_THRESHOLD         28.0f

// --------------------------------------------
// Hardware Pins & Peripherals
// --------------------------------------------
DigitalOut      alarmBuzzer( D7 );
AnalogIn        tempProbe( A1 );

// --------------------------------------------
// Operational Modes
// --------------------------------------------
typedef enum {
    MODE_IDLE,          // Normal, no alarm
    MODE_ALERT,         // Hazard detected, buzzer active
    MODE_SILENCED       // Hazard present but alarm suppressed by user
} OperatingMode;

static OperatingMode g_currentMode = MODE_IDLE;

// --------------------------------------------
// Timing & Reporting State
// --------------------------------------------
static uint32_t  g_reportTimer = 0;
static char      g_codeEntry[6] = "";
static uint8_t   g_entryPosition = 0;
static const char MASTER_UNLOCK_KEY[] = "11111";

// --------------------------------------------
// External Sensor Access
// --------------------------------------------
extern float    temperatureSensorReadCelsius( void );
extern bool     gasDetectorStateRead( void );

// ============================================
// INTERNAL HELPER FUNCTIONS
// ============================================

/**
 * Convert LM35 analog reading to temperature in Celsius.
 */
static float getTemperatureReading( void )
{
    // LM35: 1
    return tempProbe.read() * 3.3f * 100.0f;
}

/**
 * Determine if any safety threshold has been exceeded.
 */
static bool isHazardCondition( float temp, bool gas )
{
    return ( temp > TEMP_SAFE_THRESHOLD ) || gas;
}

/**
 * Clear the display and show the initial welcome message.
 */
static void showWelcomeScreen( void )
{
    displayCharPositionWrite( 0, 0 );
    displayStringWrite( "System Online  " );
    displayCharPositionWrite( 0, 1 );
    displayStringWrite( "System Safe     " );
}

/**
 * mode transitions.
 */
static void updateOperatingMode( bool hazardPresent )
{
    switch ( g_currentMode )
    {
        case MODE_IDLE:
            alarmBuzzer = 0;
            if ( hazardPresent )
            {
                g_currentMode = MODE_ALERT;
                displayCharPositionWrite( 0, 3 );
                displayStringWrite( "WARN: LIMIT HIT!" );
            }
            break;

        case MODE_ALERT:
            alarmBuzzer = 1;
            if ( !hazardPresent )
            {
                g_currentMode = MODE_IDLE;
                displayCharPositionWrite( 0, 3 );
                displayStringWrite( "                " );
            }
            break;

        case MODE_SILENCED:
            alarmBuzzer = 0;
            if ( !hazardPresent )
            {
                g_currentMode = MODE_IDLE;
                displayCharPositionWrite( 0, 3 );
                displayStringWrite( "                " );
            }
            break;
    }
}

/**
 * Process a single digit entered for the deactivation code.
 */
static void handleCodeDigit( char digit )
{
    if ( g_entryPosition < 5 )
    {
        g_codeEntry[ g_entryPosition ] = digit;
        g_entryPosition++;
        g_codeEntry[ g_entryPosition ] = '\0';

        // Display entered digits
        displayCharPositionWrite( 0, 1 );
        displayStringWrite( "Code:           " );
        displayCharPositionWrite( 6, 1 );
        displayStringWrite( g_codeEntry );

        // Evaluate when 5 digits collected
        if ( g_entryPosition == 5 )
        {
            if ( strcmp( g_codeEntry, MASTER_UNLOCK_KEY ) == 0 )
            {
                if ( g_currentMode == MODE_ALERT )
                {
                    g_currentMode = MODE_SILENCED;
                    displayCharPositionWrite( 0, 0 );
                    displayStringWrite( "Alarm MUTED!    " );
                }
                else
                {
                    displayCharPositionWrite( 0, 0 );
                    displayStringWrite( "System is Safe  " );
                }
            }
            else
            {
                displayCharPositionWrite( 0, 0 );
                displayStringWrite( "Wrong Code!     " );
            }

            // Pause, then clear the code entry line
            delay( 1500 );
            displayCharPositionWrite( 0, 0 );
            displayStringWrite( "                " );
            displayCharPositionWrite( 0, 1 );
            displayStringWrite( "                " );

            // Reset entry buffer
            g_entryPosition = 0;
            g_codeEntry[0]   = '\0';
        }
    }
}

/**
 * Process a keypress from the matrix keypad.
 */
static void processKeyInput( char key, bool gasDetected, float tempValue )
{
    if ( key == '\0' ) return;

    if ( key == '4' )
    {
        // Gas detector status
        displayCharPositionWrite( 0, 0 );
        displayStringWrite( gasDetected ? "Gas: DETECTED!  " : "Gas: Safe       " );
    }
    else if ( key == '5' )
    {
        // Temperature reading
        char buffer[ 17 ];
        sprintf( buffer, "Temp: %.1f C     ", tempValue );
        displayCharPositionWrite( 0, 0 );
        displayStringWrite( buffer );
    }
    else if ( key >= '0' && key <= '9' )
    {
        handleCodeDigit( key );
    }
}

/**
 * Update the alarm status on the LCD.
 */
static void refreshStatusReport( void )
{
    if ( g_reportTimer >= STATUS_UPDATE_INTERVAL_MS )
    {
        displayCharPositionWrite( 0, 2 );
        if ( g_currentMode == MODE_ALERT )
        {
            displayStringWrite( "Alarm is: ON    " );
        }
        else
        {
            displayStringWrite( "Alarm is: OFF   " );
        }
        g_reportTimer = 0;
    }
    else
    {
        g_reportTimer += LOOP_DELAY_MS;
    }
}

// ============================================
// SYSTEM INITIALIZATION
// ============================================
static void initializePeripherals( void )
{
    displayInit( DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER );
    matrixKeypadInit( LOOP_DELAY_MS );
    alarmBuzzer = 0;
    showWelcomeScreen();
}

// ============================================
// MAIN CONTROL LOOP
// ============================================
static void runControlCycle( void )
{
    // Acquire sensor data
    float   temperature = getTemperatureReading();
    bool    gasFlag     = gasDetectorStateRead();
    bool    hazard      = isHazardCondition( temperature, gasFlag );

    // Mode transitions
    updateOperatingMode( hazard );

    // Handle user interaction
    char pressedKey = matrixKeypadUpdate();
    processKeyInput( pressedKey, gasFlag, temperature );
 
    // Periodic status display
    refreshStatusReport();
}

// ============================================
// ENTRY POINT
// ============================================
int main( void )
{
    initializePeripherals();

    while ( 1 )
    {
        runControlCycle();
        delay( LOOP_DELAY_MS );
    }
}
