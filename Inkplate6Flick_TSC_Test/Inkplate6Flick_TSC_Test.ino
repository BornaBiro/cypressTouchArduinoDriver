
// Include Inkplate library - needed for the I/O expander to power up touchscreen power supply.
#include "Inkplate.h"

// Include Arduino Wire library.
#include "Wire.h"

// Innclude custom written library for cypress touch driver.
#include "cypressTouch.h"

// Create Inkplate object.
Inkplate display(INKPLATE_1BIT);

// Create object for the Cypress touchscreen used on ED060XC3
CypressTouch touch;

void setup()
{
    // Init. serial ycommunication.
    Serial.begin(115200);

    // Send welcome message on the serial sw we know ESP32 is alive.
    touch.printInfo(&Serial, "ESP32 code started");

    // Init. Arduino wire (I2C) library.
    Wire.begin();

    // Print debug message.
    touch.printInfo(&Serial, "Wire init done");

    // Init. Inkplate library for the I/O expander.
    display.begin();

    // Send Inkplate and Wire object pointers into the Cypress touch library.
    // Init. the library,
    if (!touch.begin(&Wire, &display))
    {
        // Print error Message if touch init has failed.
        // This will also halt the code.
        touch.printError(&Serial, "Touch init failed");
    }
    else
    {
        // Print debug message.
        touch.printDebug(&Serial, "Touch init ok");
    }
}

void loop()
{
    // Check for the new data from the touch.
    if (touch.available())
    {
        // New data available? Read it!
        struct cypressTouchData tsData;

        if (touch.getTouchData(&tsData))
        {
            char _strBuffer[300];
            sprintf(_strBuffer, "RAW-> Fingers: %1d CH1_X:%4d CH1_Y:%4d CH1_Z:%3d CH2_X:%4d CH2_Y:%4d CH2_Z:%3d, touchType:%3d", tsData.fingers, tsData.x[0], tsData.y[0], tsData.z[0], tsData.x[1], tsData.y[1], tsData.z[1], tsData.detectionType);
            touch.printInfo(&Serial, _strBuffer);

            // Scale it to fit ED060XC3.
            touch.scale(&tsData, 1024, 758, false, true, true);
            sprintf(_strBuffer, "Scaled-> Fingers: %1d CH1_X:%4d CH1_Y:%4d CH1_Z:%3d CH2_X:%4d CH2_Y:%4d CH2_Z:%3d, touchType:%3d", tsData.fingers, tsData.x[0], tsData.y[0], tsData.z[0], tsData.x[1], tsData.y[1], tsData.z[1], tsData.detectionType);
            touch.printInfo(&Serial, _strBuffer);
        }
        else
        {
            touch.printInfo(&Serial, "Touch data read error");
        }
    }
}