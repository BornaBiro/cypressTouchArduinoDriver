#include "Inkplate.h"
#include "Wire.h"

#include "cypressTouch.h"

Inkplate display(INKPLATE_1BIT);
CypressTouch touch;

volatile bool _touchIntFlag = false;

IRAM_ATTR void touchInterrupt()
{
    _touchIntFlag = true;
}

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

    pinMode(36, INPUT);
    attachInterrupt(digitalPinToInterrupt(36), touchInterrupt, FALLING);
}

typedef struct __attribute__((__packed__)) touchData
{
    uint16_t x;
    uint16_t y;
    uint8_t z;
};

touchData touchDataReport;

void loop()
{
    if (_touchIntFlag)
    {
        _touchIntFlag = false;

        uint8_t myArray[32];
        memset(myArray, 0, sizeof(myArray));

        Wire.beginTransmission(CPYRESS_TOUCH_I2C_ADDR);
        Wire.write(CYPRESS_TOUCH_BASE_ADDR);
        if(Wire.endTransmission() == 0)
        {
            Wire.requestFrom(CPYRESS_TOUCH_I2C_ADDR, 32);
            Wire.readBytes(myArray, sizeof(myArray));

            touch.handshake();

            for (int i = 0; i < 32; i++)
            {
                Serial.printf("0x%02X%c",myArray[i], i != 31?',':' ');
            }
            Serial.println();

            //memcpy(&touchDataReport, &myArray[3], sizeof(touchDataReport));
            //Serial.printf("Touch: Mode=%3d, Stat=%1d ",myArray[1], myArray[2]);
            //Serial.printf("Ch1: X=%4d Y=%4d Z=%3d ID=%3d ", myArray[3] << 8 | myArray[4], myArray[5] << 8 | myArray[6], myArray[7], myArray[8]);
            //Serial.printf("Ch2: X=%4d Y=%4d Z=%3d ID=%3d ", myArray[9] << 8 | myArray[10], myArray[11] << 8 | myArray[12], myArray[13], myArray[14]);
            //Serial.printf("Gesture: CNT=%3d, ID=%3d\r\n", myArray[15], myArray[16]);
            // Serial.print("-----------------------------------");

            // Serial.print(myArray[3] << 8 | myArray[4], DEC);
            // Serial.print(',');
            // Serial.print(myArray[5] << 8 | myArray[6], DEC);
            // Serial.println();
        }
    }
}