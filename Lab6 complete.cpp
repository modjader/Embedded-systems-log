#include "mbed.h"
#include "arm_book_lib.h"
#include "display.h"
#include "matrix_keypad.h"
#include <string.h>
#include <stdio.h>
#define LOOP_DELAY_MS               10
#define STATUS_UPDATE_INTERVAL_MS   60000
#define TEMP_SAFE_THRESHOLD         28.0f
DigitalOut      alarmBuzzer( D7 );
AnalogIn        tempProbe( A1 );
typedef enum {
    MODE_IDLE,         
    MODE_ALERT,         
    MODE_SILENCED       
} OperatingMode;

static OperatingMode g_currentMode = MODE_IDLE;
static uint32_t  g_reportTimer = 0;
static char      g_codeEntry[6] = "";
static uint8_t   g_entryPosition = 0;
static const char MASTER_UNLOCK_KEY[] = "19161";

extern float    temperatureSensorReadCelsius( void );
extern bool     gasDetectorStateRead( void );

static float getTemperatureReading( void )
{
    // LM35: 1
    return tempProbe.read() * 3.3f * 100.0f;
}

static bool isHazardCondition( float temp, bool gas )
{
    return ( temp > TEMP_SAFE_THRESHOLD ) || gas;
}

static void showWelcomeScreen( void )
{
    displayCharPositionWrite( 0, 0 );
    displayStringWrite( "smart home v1.0 " );
    displayCharPositionWrite( 0, 1 );
    displayStringWrite( "All systems Go " );
}

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
                displayStringWrite( "!!WARNING!!" );
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
static void handleCodeDigit( char digit )
{
    if ( g_entryPosition < 5 )
    {
        g_codeEntry[ g_entryPosition ] = digit;
        g_entryPosition++;
        g_codeEntry[ g_entryPosition ] = '\0';

        displayCharPositionWrite( 0, 1 );
        displayStringWrite( "Code:           " );
        displayCharPositionWrite( 6, 1 );
        displayStringWrite( g_codeEntry );

        if ( g_entryPosition == 5 )
        {
            if ( strcmp( g_codeEntry, MASTER_UNLOCK_KEY ) == 0 )
            {
                if ( g_currentMode == MODE_ALERT )
                {
                    g_currentMode = MODE_SILENCED;
                    displayCharPositionWrite( 0, 0 );
                    displayStringWrite( "siren MUTED!    " );
                }
                else
                {
                    displayCharPositionWrite( 0, 0 );
                    displayStringWrite( "access granted" );
                }
            }
            else
            {
                displayCharPositionWrite( 0, 0 );
                displayStringWrite( "the code is wrong, access denied" );
            }
            
            delay( 1500 );
            displayCharPositionWrite( 0, 0 );
            displayStringWrite( "                " );
            displayCharPositionWrite( 0, 1 );
            displayStringWrite( "                " );

            g_entryPosition = 0;
            g_codeEntry[0]   = '\0';
        }
    }
}
static void processKeyInput( char key, bool gasDetected, float tempValue )
{
    if ( key == '\0' ) return;

    if ( key == 'A' )
    {
        displayCharPositionWrite( 0, 0 );
        displayStringWrite( gasDetected ? "Gas: DETECTED!  " : "Gas: Safe       " );
    }
    else if ( key == 'B' )
    {
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
static void refreshStatusReport( void )
{
    if ( g_reportTimer >= STATUS_UPDATE_INTERVAL_MS )
    {
        displayCharPositionWrite( 0, 2 );
        if ( g_currentMode == MODE_ALERT )
        {
            displayStringWrite( "Alarm is: active    " );
        }
        else
        {
            displayStringWrite( "Alarm is: on stand by" );
        }
        g_reportTimer = 0;
    }
    else
    {
        g_reportTimer += LOOP_DELAY_MS;
    }
}
static void initializePeripherals( void )
{
    displayInit( DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER );
    matrixKeypadInit( LOOP_DELAY_MS );
    alarmBuzzer = 0;
    showWelcomeScreen();
}
static void runControlCycle( void )
{
    
    float   temperature = getTemperatureReading();
    bool    gasFlag     = gasDetectorStateRead();
    bool    hazard      = isHazardCondition( temperature, gasFlag );

    updateOperatingMode( hazard );
    char pressedKey = matrixKeypadUpdate();
    processKeyInput( pressedKey, gasFlag, temperature );
    refreshStatusReport();
}
int main( void )
{
    initializePeripherals();

    while ( 1 )
    {
        runControlCycle();
        delay( LOOP_DELAY_MS );
    }
}