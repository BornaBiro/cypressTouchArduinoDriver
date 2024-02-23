#include "Inkplate.h"
#include "Wire.h"

#include "cypressTouch.h"

Inkplate display(INKPLATE_1BIT);
CypressTouch touch;

void setup()
{
    Serial.begin(115200);
    touch.printInfo(&Serial, "ESP32 code started");

    Wire.begin();
    touch.printInfo(&Serial, "Wire init done");

    display.begin();

    if (!touch.begin(&Wire, &display))
    {
        touch.printDebug(&Serial, "Touch init failed");
    }
    else
    {
        touch.printDebug(&Serial, "Touch init ok");
    }
}

void loop()
{
    uint8_t myArray[32];
    memset(myArray, 0, sizeof(myArray));

    Wire.beginTransmission(CPYRESS_TOUCH_I2C_ADDR);
    Wire.write(CYPRESS_TOUCH_BASE_ADDR);
    if(Wire.endTransmission() == 0)
    {
        Wire.requestFrom(CPYRESS_TOUCH_I2C_ADDR, 32);
        Wire.readBytes(myArray, sizeof(myArray));

        for (int i = 0; i < 32; i++)
        {
            Serial.printf("0x%02X%c",myArray[i], i != 31?',':' ');
        }
        Serial.println();
    }
    delay(250);
}