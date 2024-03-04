#ifndef __CYPRESSTOUCH_H__
#define __CYPRESSTOUCH_H__

// Code resoruces:
// https://github.com/martinezjavier/nook_color/blob/master/cyttsp/
// https://github.com/torvalds/linux/tree/master/drivers/input/touchscreen
// https://github.com/torvalds/linux/blob/master/drivers/input/touchscreen/cyttsp_core.c
// This is rewritten from Linux Kernel code for Arduino to work with Inkplate library.

// Include main Arduino header file.
#include <Arduino.h>

// Include Wire library (I2C Arduino Library).
#include <Wire.h>

// Include Inkplate library (neded for GPIO manipulation).
#include <Inkplate.h>

// Include Cypress touchscreen typedefs.
#include "cypressTouchTypedefs.h"

// Cypress Touch IC I2C address (7 bit I2C address).
#define CPYRESS_TOUCH_I2C_ADDR  0x24

// GPIOs for touchscreen controller.
#define CYPRESS_TOUCH_PWR_MOS_PIN   IO_PIN_B4
#define CYPRESS_TOUCH_RST_PIN       IO_PIN_B2

// Cypress touchscreen controller I2C regs.
#define CYPRESS_TOUCH_BASE_ADDR         0x00
#define CYPRESS_TOUCH_SOFT_RST_MODE     0x01
#define CYPRESS_TOUCH_SYSINFO_MODE      0x10
#define CYPRESS_TOUCH_OPERATE_MODE      0x00
#define CYPRESS_TOUCH_LOW_POWER_MODE    0x04
#define CYPRESS_TOUCH_DEEP_SLEEP_MODE   0x02
#define CYPRESS_TOUCH_REG_ACT_INTRVL    0x1D

// Active Power state scanning/processing refresh interval
#define CYPRESS_TOUCH_ACT_INTRVL_DFLT		0x00 /* ms */
// Low Power state scanning/processing refresh interval
#define CYPRESS_TOUCH_LP_INTRVL_DFLT		0x0A /* ms */
// Touch timeout for the Active power */
#define CYPRESS_TOUCH_TCH_TMOUT_DFLT		0xFF /* ms */

// Max X and Y sizes reported by the TSC.
#define CYPRESS_TOUCH_MAX_X     682
#define CYPRESS_TOUCH_MAX_Y     1023

// Touch Interrupt related stuff.
static volatile bool _touchscreenIntFlag = false;
IRAM_ATTR static void _touchscreenIntCallback()
{
    _touchscreenIntFlag = true;
}

class CypressTouch
{
    public:
        // Library constructor.
        CypressTouch();

        // Initialization function.
        bool begin(TwoWire *_touchI2C, Inkplate *_display);

        // Check if there is any new touch event detected.
        bool available();

        // Get the new touch event data/report.
        bool getTouchData(struct cypressTouchData *_touchData);

        // Disable touchscreen.
        void end();

        // Set the proper power mode for the touchscreen controller.
        bool setPowerMode(uint8_t _powerMode);

        // Scale touch data report to fit screen (and also rotation).
        void scale(struct cypressTouchData *_touchData, uint16_t _xSize, uint16_t _ySize, bool _flipX, bool _flipY, bool _swapXY);

        // Helper function for printing info data on the serial (with [INFO] header and timestamp).
        void printInfo(HardwareSerial *_serial, char *_message);

        // Helper function for printing debug messages to the serial (with [DEBUG] header and timestamp).
        void printDebug(HardwareSerial *_serial, char *_message);

        // Helper function for printig error messages to the serial (with [ERROR] header, timestamp and code halt).
        void printError(HardwareSerial *_serial, char *_message);

    private:
        // Inkplate library internal object pointer.
        Inkplate *_displayPtr = NULL;

        // I2C library object pointer (Arduino Wire Library).
        TwoWire *_touchI2CPtr = NULL;

        // Bootloader struct typedef.
        struct cyttspBootloaderData _blData;

        // System info data typedef.
        struct cyttspSysinfoData _sysData;

        // Method disables or enables power to the Touchscreen.
        void power(bool _pwr);

        // Methods executes HW reset (with Touchscreen RST pin).
        void reset();

        // Method executes SW reset command for Touchscreen via I2C.
        void swReset();

        // Method loads bootloader register from Touchscreen IC via I2C.
        bool loadBootloaderRegs(struct cyttspBootloaderData *_blDataPtr);

        // Method forces Touchscreen Controller to exits bootloader mode and executes preloaded FW code.
        bool exitBootLoaderMode();

        // Force Touchscreen Controller into system info mode.
        bool setSysInfoMode(struct cyttspSysinfoData *_sysDataPtr);

        // Load system into register into their default values.
        bool setSysInfoRegs(struct cyttspSysinfoData *_sysDataPtr);

        // Try to ping Touchscreen controller via I2C (I2C test).
        bool ping(int _retries = 5);

        // Do a handshake for Touchscreen Controller to acknowledge successfull touch report read.
        void handshake();

        // Needs to be removed.
        void regDump(HardwareSerial *_debugSerialPtr, int _startAddress, int _endAddress);

        // Helper method for printing debug, info and error messages.
        void printMessage(HardwareSerial *_serial, char *_msgPrefix, char *_message);

        // Helper method for printing timestamp with millis() Arduino function.
        void printTimestamp(HardwareSerial *_serial);

        // Low-level I2C stuff.
        // Send command to the Touchscreen Controller via I2C.
        bool sendCommand(uint8_t _cmd);

        // Read Touchscreen Controller registers from the I2C by using Arduino Wire libary.
        bool readI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len);

        // Write into Touchscreen Controller registers with I2C by using Arduino Wire library.
        bool writeI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len);
};

#endif