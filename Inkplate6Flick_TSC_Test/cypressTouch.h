#ifndef __CYPRESSTOUCH_H__
#define __CYPRESSTOUCH_H__

// Code resoruces:
// https://github.com/martinezjavier/nook_color/blob/master/cyttsp/
// https://github.com/torvalds/linux/tree/master/drivers/input/touchscreen
// https://github.com/torvalds/linux/blob/master/drivers/input/touchscreen/cyttsp_core.c

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
#define CYPRESS_TOUCH_REG_ACT_INTRVL    0x1D

// Active Power state scanning/processing refresh interval
#define CYPRESS_TOUCH_ACT_INTRVL_DFLT		0x00 /* ms */
// Low Power state scanning/processing refresh interval
#define CYPRESS_TOUCH_LP_INTRVL_DFLT		0x0A /* ms */
// Touch timeout for the Active power */
#define CYPRESS_TOUCH_TCH_TMOUT_DFLT		0xFF /* ms */

class CypressTouch
{
    public:
        // Library constructor.
        CypressTouch();

        // Initialization function.
        int begin(TwoWire *_touchI2C, Inkplate *_display);

        void printInfo(HardwareSerial *_serial, char *_message);

        void printDebug(HardwareSerial *_serial, char *_message);

        void printError(HardwareSerial *_serial, char *_message);

    private:
        // Inkplate library internal object pointer.
        Inkplate *_displayPtr = NULL;

        // I2C library object pointer (Arduino Wire Library).
        TwoWire *_touchI2CPtr = NULL;

        // Bootloader struct typedef.
        struct cyttsp_bootloader_data _blData;

        // System info data typedef.
        struct cyttsp_sysinfo_data _sysData;

        void power(bool _pwr);

        void reset();

        void swReset();

        bool loadBootloaderRegs(struct cyttsp_bootloader_data *_blDataPtr);

        bool exitBootLoaderMode();

        bool setSysInfoMode(struct cyttsp_sysinfo_data *_sysDataPtr);

        bool setSysInfoRegs(struct cyttsp_sysinfo_data *_sysDataPtr);

        bool ping(int _retries = 5);

        void regDump(HardwareSerial *_debugSerialPtr, int _startAddress, int _endAddress);

        void printMessage(HardwareSerial *_serial, char *_msgPrefix, char *_message);

        void printTimestamp(HardwareSerial *_serial);

        // Low-level I2C stuff.
        bool sendCommand(uint8_t _cmd);

        bool readI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len);

        bool writeI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len);
};

#endif