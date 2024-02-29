// Include main header file of the library
#include "cypressTouch.h"

// Macro helpers.
#define GET_BOOTLOADERMODE(reg)		(((reg) & 0x10) >> 4)

// Library constructor.
CypressTouch::CypressTouch()
{

}

// Initialization function.
int CypressTouch::begin(TwoWire *_touchI2C, Inkplate *_display)
{
    // Copy library objects into the internal ones.
    _displayPtr = _display;
    _touchI2CPtr = _touchI2C;

    // Initialize Wire library (just in case).
    _touchI2CPtr->begin();

    // Set GPIO pins.
    _displayPtr->pinModeIO(CYPRESS_TOUCH_PWR_MOS_PIN, OUTPUT, IO_INT_ADDR);
    _displayPtr->pinModeIO(CYPRESS_TOUCH_RST_PIN, OUTPUT, IO_INT_ADDR);

    // Enable the power to the touch.
    power(true);

    // Do a HW reset.
    reset();

    // Try to ping it.
    if(!ping(5)) return false;

    // Issue a SW reset.
    sendCommand(0x01);

    // // Read bootloader data.
    // loadBootloaderRegs(&_blData);

    // // Dump it!
    // char temp[250];
    // sprintf(temp, "bl_file=0x%02X", _blData.bl_file);
    // printDebug(&Serial, temp);
    // sprintf(temp, "bl_status=0x%02X", _blData.bl_status);
    // printDebug(&Serial, temp);
    // sprintf(temp, "bl_error=0x%02X", _blData.bl_error);
    // printDebug(&Serial, temp);
    // sprintf(temp, "blver_hi=0x%02X", _blData.blver_hi);
    // printDebug(&Serial, temp);
    // sprintf(temp, "blver_lo=0x%02X", _blData.blver_lo);
    // printDebug(&Serial, temp);
    // sprintf(temp, "bld_blver_hi=0x%02X", _blData.bld_blver_hi);
    // printDebug(&Serial, temp);
    // sprintf(temp, "bld_blver_lo=0x%02X", _blData.bld_blver_lo);
    // printDebug(&Serial, temp);
    // sprintf(temp, "ttspver_hi=0x%02X", _blData.ttspver_hi);
    // printDebug(&Serial, temp);
    // sprintf(temp, "ttspver_lo=0x%02X", _blData.ttspver_lo);
    // printDebug(&Serial, temp);
    // sprintf(temp, "appid_hi=0x%02X", _blData.appid_hi);
    // printDebug(&Serial, temp);
    // sprintf(temp, "appid_lo=0x%02X", _blData.appid_lo);
    // printDebug(&Serial, temp);
    // sprintf(temp, "appver_hi=0x%02X", _blData.appver_hi);
    // printDebug(&Serial, temp);
    // sprintf(temp, "appver_lo=0x%02X", _blData.appver_lo);
    // printDebug(&Serial, temp);
    // sprintf(temp, "cid_0=0x%02X", _blData.cid_0);
    // printDebug(&Serial, temp);
    // sprintf(temp, "cid_1=0x%02X", _blData.cid_1);
    // printDebug(&Serial, temp);
    // sprintf(temp, "cid_2=0x%02X", _blData.cid_2);
    // printDebug(&Serial, temp);

    // Exit bootloader mode. - Does not exit bootloader propery!
    if (!exitBootLoaderMode())
    {
        printError(&Serial, "Failed to exit bootloader mode!");
    }

    handshake();

    // Set mode to system info mode.
    if (!setSysInfoMode(&_sysData))
    {
        printDebug(&Serial, "Failed to enter system info mode");
    }

    handshake();

    setSysInfoRegs(&_sysData);

    handshake();

    sendCommand(CYPRESS_TOUCH_OPERATE_MODE);

    handshake();

    uint8_t _distDefaultValue = 0xF8;
    writeI2CRegs(0x1E, &_distDefaultValue, 1);

    // Everything went ok? Return 1 for success.
    return 1;
}

void CypressTouch::power(bool _pwr)
{
    if (_pwr)
    {
        // Enable the power MOSFET.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_PWR_MOS_PIN, LOW, IO_INT_ADDR);

        // Wait a little bit before proceeding any further.
        delay(250);

        // Set reset pin to high.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, HIGH, IO_INT_ADDR);

        // Wait a little bit.
        delay(10);
    }
    else
    {
        // Disable the power MOSFET switch.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_PWR_MOS_PIN, HIGH, IO_INT_ADDR);

        // Wait a bit to discharge caps.
        delay(250);

        // Set reset pin to low.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, LOW, IO_INT_ADDR);
    }
}

void CypressTouch::reset()
{
    // Toggle RST line. Loggic low must be at least 1ms, re-init after reset not specified, 10 ms (from Linux kernel).
    _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, HIGH, IO_INT_ADDR);
    delay(10);
    _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, LOW, IO_INT_ADDR);
    delay(2);
    _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, HIGH, IO_INT_ADDR);
    delay(10);
}

void CypressTouch::swReset()
{
    sendCommand(CYPRESS_TOUCH_SOFT_RST_MODE);
}

bool CypressTouch::loadBootloaderRegs(struct cyttsp_bootloader_data *_blDataPtr)
{
    // Bootloader temp. registers array.
    uint8_t _bootloaderData[16];

    if (!readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _bootloaderData, 16)) return false;

    // Parse Bootloader data into typedef struct.
    memcpy(_blDataPtr, _bootloaderData, 16);

    return true;
}

bool CypressTouch::exitBootLoaderMode()
{
    // Bootloader command array.
    uint8_t _blCommandArry[] = 
    {
        0x00,   // File offset.
        0xFF,   // Command.
        0xA5,   // Exit bootloader command.
        0, 1, 2, 3, 4, 5, 6, 7  // Default keys.
    };

    // Write bootloader settings.
    writeI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _blCommandArry, sizeof(_blCommandArry));

    // Wait a little bit.
    delay(250);

    // Get bootloader data.
    struct cyttsp_bootloader_data _bootloaderData;
    loadBootloaderRegs(&_bootloaderData);

    // Check for validity.
    if (GET_BOOTLOADERMODE(_bootloaderData.bl_status)) return false;

    // If everything went ok return true.
    return true;
}

bool CypressTouch::setSysInfoMode(struct cyttsp_sysinfo_data *_sysDataPtr)
{
    // Change mode to system info.
    sendCommand(CYPRESS_TOUCH_SYSINFO_MODE);

    // Wait a bit.
    delay(50);

    // Buffer for the system info data.
    uint8_t _sysInfoArray[32];

    // Read the registers.
    if (!readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _sysInfoArray, sizeof(_sysInfoArray)))
    {
        printDebug(&Serial, "Failed to read I2C - System info mode");
        return false;
    }

    for (int i = 0; i < 32; i++)
    {
        Serial.printf("0x%02X%c", _sysInfoArray[i], i != (sizeof(_sysInfoArray) - 1)?',':' ');
    }

    // Copy into struct typedef.
    memcpy(_sysDataPtr, _sysInfoArray, sizeof(_sysInfoArray));

    // Do a handshake!
    handshake();
    //handshake();

    // Check TTS version.
    if (!_sysDataPtr->tts_verh && !_sysDataPtr->tts_verl)
    {
        printDebug(&Serial, "TTS Version fail!");
        return false;
    }

    // Everything went ok? Return true for success.
    return true;
}

bool CypressTouch::setSysInfoRegs(struct cyttsp_sysinfo_data *_sysDataPtr)
{
    // Modify registers to the default values.
    _sysDataPtr->act_intrvl = CYPRESS_TOUCH_ACT_INTRVL_DFLT;
    _sysDataPtr->tch_tmout = CYPRESS_TOUCH_TCH_TMOUT_DFLT;
    _sysDataPtr->lp_intrvl = CYPRESS_TOUCH_LP_INTRVL_DFLT;

    uint8_t _regs[] = {_sysDataPtr->act_intrvl, _sysDataPtr->tch_tmout, _sysDataPtr->lp_intrvl};

    // Send the registers to the I2C. Check if failed. If failed, return false.
    if (!writeI2CRegs(0x1D, _regs, 3)) return false;

    // Wait a little bit.
    delay(50);

    // Everything went ok? Return true for success.
    return true;
}

void CypressTouch::handshake()
{
    // Read the hst_mode register (address 0x00).
    uint8_t _hstModeReg = 0;
    readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, &_hstModeReg, 1);
    _hstModeReg ^= 0x80;
    writeI2CRegs(CYPRESS_TOUCH_BASE_ADDR, &_hstModeReg, 1);
}

bool CypressTouch::ping(int _retries)
{
    // Sucess / return variable. Set it by default on fail.
    int _retValue = 1;

    // Try to ping multiple times in a row (just in case TSC is not in low power mode).
    // Delay between retires is 25ms (just a wildguess, don't have any documentation).
    for (int i = 0; i < _retries; i++)
    {
        // Ping the TSC (touchscreen controller) on I2C.
        _touchI2CPtr->beginTransmission(CPYRESS_TOUCH_I2C_ADDR);
        _retValue = _touchI2CPtr->endTransmission();

        // Return value is 0? That means ACK, TSC found!
        if (_retValue == 0)
        {
            return true;
        }

        // TSC not found? Try again, but before retry wait a little bit.
        delay(25);
    }

    // Got here? Not good, TSC not found, return error.
    return false;
}

void CypressTouch::regDump(HardwareSerial *_debugSerialPtr, int _startAddress, int _endAddress)
{
    // Check the size of the request. Reading more than 32 bytes over I2C is not possible.
    int _len = abs(_endAddress - _startAddress);
    if (_len > 32)
    {
        printDebug(_debugSerialPtr, "Reading more than 32 bytes over I2C on Arduino is not possible");
    }

    // Check for order.
    if (_startAddress > _endAddress)
    {
        // Swap them!
        int _temp = _startAddress;
        _startAddress = _endAddress;
        _endAddress = _startAddress;
        printInfo(_debugSerialPtr, "Start and end I2C register address are swapped");
    }

    // Make a request!
    Wire.beginTransmission(CPYRESS_TOUCH_I2C_ADDR);
    Wire.write(_startAddress);
    Wire.endTransmission();
    Wire.requestFrom(CPYRESS_TOUCH_I2C_ADDR, _len);
    
    // Print them!
    for (int i = _startAddress; i < _endAddress; i++)
    {
        char _tempArray[40];
        sprintf(_tempArray, "REG 0x%02X, Value: 0x%02X", i, Wire.read());
        printDebug(_debugSerialPtr, _tempArray);
    }
}

void CypressTouch::printMessage(HardwareSerial *_serial, char *_msgPrefix, char *_message)
{
    printTimestamp(_serial);
    _serial->print(" - [");
    _serial->print(_msgPrefix);
    _serial->print("]: ");
    _serial->print(_message);
    _serial->println();
}

void CypressTouch::printInfo(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "INFO", _message);
}

void CypressTouch::printDebug(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "DEBUG", _message);
}

void CypressTouch::printError(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "ERROR", _message);
    while (1)
    {
        delay(100);
    }
}

void CypressTouch::printTimestamp(HardwareSerial *_serial)
{
    unsigned long _millisCapture = millis();
    int _h = _millisCapture / 3600000ULL;
    int _m = _millisCapture / 60000ULL % 60;
    int _s = _millisCapture / 1000ULL % 60;
    int _ms = _millisCapture % 1000ULL;
    _serial->printf("%02d:%02d:%02d;%03d", _h, _m, _s, _ms);
}

bool CypressTouch::sendCommand(uint8_t _cmd)
{
    // Init I2C communication.
    _touchI2CPtr->beginTransmission(CPYRESS_TOUCH_I2C_ADDR);
    
    // I'm not sure about this?
    // Write I2C sub-address (register address).
    Wire.write(CYPRESS_TOUCH_BASE_ADDR);

    // Write command.
    Wire.write(_cmd);

    // Wait a little bit.
    delay(20);

    // Send to I2C!
    return Wire.endTransmission() == 0?true:false;
}

bool CypressTouch::readI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len)
{
    // Init I2C communication!
    Wire.beginTransmission(CPYRESS_TOUCH_I2C_ADDR);

    // I'm not sure about this?
    // Send command byte.
    Wire.write(_cmd);

    // Write reg to the I2C! If I2C send has failed, return false.
    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    // Read back data from the regs.
    // Watchout! Arduino Wire library can only read 32 bytes at the times
    int _index = 0;
    while (_len > 0)
    {
        // Check for the size of the remaining buffer.
        int _i2cLen = _len > 32?32:_len;

        // Read the bytes from the I2C.
        Wire.requestFrom(CPYRESS_TOUCH_I2C_ADDR, _i2cLen);
        Wire.readBytes(_buffer + _index, _i2cLen);
        
        // Update the buffer index position.
        _index += _i2cLen;

        // Update the lenght.
        _len -= _i2cLen;
    }

    // Everything went ok? Return true.
    return true;
}

bool CypressTouch::writeI2CRegs(uint8_t _cmd, uint8_t *_buffer, int _len)
{
    // Init I2C communication!
    Wire.beginTransmission(CPYRESS_TOUCH_I2C_ADDR);

    // I'm not sure about this?
    // Send command byte.
    Wire.write(_cmd);

    // Write data.
    Wire.write(_buffer, _len);

    // Write reg to the I2C! If I2C send has failed, return false.
    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    // Everything went ok? Return true.
    return true;
}
