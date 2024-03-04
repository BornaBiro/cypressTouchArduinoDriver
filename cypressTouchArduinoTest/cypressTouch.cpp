// Include main header file of the library
#include "cypressTouch.h"

// Macro helpers.
#define GET_BOOTLOADERMODE(reg)		(((reg) & 0x10) >> 4)

/**
 * @brief Constructor for a new CypressTouch object.
 * 
 */
CypressTouch::CypressTouch()
{

}

// Initialization function.
/**
 * @brief   
 * 
 * @param       TwoWire *_touchI2C
 *              Arduino TwoWore object (I2C library). Needed for I2C Touch communication.
 * @param       Inkplate *_display
 *              Arduino Inkplate library - Needed fopr PCAL I/O expander for Power MOSFET enable for
 *              Touchscreen power supply.
 * @return      bool
 *              true - Touchscreen Controller initialization ok.
 *              false - Touchscreen Controller initialization failed.
 */
bool CypressTouch::begin(TwoWire *_touchI2C, Inkplate *_display)
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

    // Read bootloader data.
    loadBootloaderRegs(&_blData);

    // Exit bootloader mode. - Does not exit bootloader propery!
    if (!exitBootLoaderMode())
    {
        printError(&Serial, "Failed to exit bootloader mode");
    }

    // Set mode to system info mode.
    if (!setSysInfoMode(&_sysData))
    {
        printError(&Serial, "Failed to enter system info mode");
    }

    // Set system info regs.
    if (!setSysInfoRegs(&_sysData))
    {
        printError(&Serial, "Failed to enter system info registers");
    }

    // Switch it into operate mode (also can be in deep sleep mode as well as low power mode).
    sendCommand(CYPRESS_TOUCH_OPERATE_MODE);

    // Set dist value for detection?
    uint8_t _distDefaultValue = 0xF8;
    writeI2CRegs(0x1E, &_distDefaultValue, 1);

    // Add interrupt callback.
    pinMode(36, INPUT);
    attachInterrupt(digitalPinToInterrupt(36), _touchscreenIntCallback, FALLING);

    // Clear the interrpt flag.
    _touchscreenIntFlag = false;

    // Everything went ok? Return 1 for success.
    return 1;
}

/**
 * @brief       Function check for new touch event.
 * 
 * @return      bool
 *              true - New touch event has been detected. Use getTouchData method to read it.
 *              false - No new touch data.
 * 
 * @note        New touch event is detected by the touchscreen controller interrupt line.
 */
bool CypressTouch::available()
{
    // Return the interrupt flag (interrrupt triggered - new touch data available).
    return _touchscreenIntFlag != 0?true:false;
}


/**
 * @brief       Get the new touch event data from the touchscreen controller.
 * 
 * @param       struct cypressTouchData _touchData
 *              Pointer to the structure for the touch report data (such as X, Y and
 *              Z values of each touch channel, nuber of fingers etc.)  
 * 
 * @return      bool
 *              true - Touch data is successfully read and the data is valid.
 *              false - Touch data read has failed.
 */
bool CypressTouch::getTouchData(struct cypressTouchData *_touchData)
{
    // Check for the null-pointer trap.
    if (_touchData == NULL) return false;

    // Clear touch interrupt flag.
    _touchscreenIntFlag = false;

    // Clear struct for touchscreen data.
    memset(_touchData, 0, sizeof(cypressTouchData));

    // Buffer for the I2C registers.
    uint8_t _regs[32];

    // Read registers for the touch data (32 bytes of data).
    // If read failed for some reason, return false.
    if (!readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _regs, sizeof(_regs))) return false;

    // Send a handshake.
    handshake();

    // Parse the data!
    // Data goes as follows:
    // [1 byte] Handshake bit - Must be written back with xor on last MSB bit for TSC knows that INT has been read.
    // [1 byte] Something? It changes with every new data. Data is always 0x00, 0x40, 0x80, 0xC0)
    // [1 byte] Number of fingers detected - Zero, one or two.
    // [2 bytes] X value position of the finger that has been detected first.
    // [2 bytes] Y value position of the finger that has been detected first.
    // [1 byte] Z value or the presusre os the touch on the first finger.
    // [1 byte] Type of detection - 0 or 255 finger released
    // [2 bytes] X value position of the finger that has been detected second.
    // [2 bytes] Y value position of the finger that has been detected second.
    // [1 byte] Z value or the presusre os the touch on the second finger.
    _touchData->x[0] = _regs[3] << 8 | _regs[4];
    _touchData->y[0] = _regs[5] << 8 | _regs[6];
    _touchData->z[0] = _regs[7];
    _touchData->x[1] = _regs[9] << 8 | _regs[10];
    _touchData->y[1] = _regs[11] << 8 | _regs[12];
    _touchData->z[1] = _regs[13];
    _touchData->detectionType = _regs[8];
    _touchData->fingers = _regs[2];

    // Everything went ok? Return true.
    return true;
}

/**
 * @brief       Disable touchscreen. Detach interrupt, clear interrput flag, disable power to the 
 *              Touchscreen Controller.
 * 
 */
void CypressTouch::end()
{
    // Detach interrupt.
    detachInterrupt(36);

    // Clear interrupt flag.
    _touchscreenIntFlag = false;

    // Disable the power to the touch.
    power(false);
}


/**
 * @brief       Set power mode of the Touchscreen Controller. There are 3 modes
 *              CYPRESS_TOUCH_OPERATE_MODE - Normal mode (fast response, higher accuracy, higher power consumption).
 *                                           Current ~ 15mA.
 *              CYPRESS_TOUCH_LOW_POWER_MODE - After few seconds of inactivity, TSC goes into low power ode and periodically
 *                                             goes into operating mode to check for touch event. Current ~4mA.
 *              CYPRESS_TOUCH_DEEP_SLEEP_MODE - Disable TSC. Current ~25uA.
 * 
 * @param       uint8_t _powerMode
 *              Power mode - Can only be CYPRESS_TOUCH_OPERATE_MODE, CYPRESS_TOUCH_LOW_POWER_MODE or CYPRESS_TOUCH_DEEP_SLEEP_MODE.
 *              [defined in cypressTouch.h]
 * 
 * @return      bool
 *              true - Power mode is successfully selected.
 *              false - Power mode select failed.  
 */
bool CypressTouch::setPowerMode(uint8_t _powerMode)
{
    // Check for the parameters.
    if ((_powerMode == CYPRESS_TOUCH_DEEP_SLEEP_MODE) || (_powerMode == CYPRESS_TOUCH_LOW_POWER_MODE) || (_powerMode == CYPRESS_TOUCH_OPERATE_MODE))
    {
        
        // Set new power mode setting.
        return sendCommand(_powerMode);
    }
    
    // Otherwise return false.
    return false;
}

/**
 * @brief       Method scales, flips and swaps X and Y cooridinates to ensure X and Y matches the screen.
 * 
 * @param       struct cypressTouchData _touchData
 *              Defined in cypressTouchTypedefs.h. Filled touch data report. 
 * @param       uint16_t _xSize
 *              Screen size in pixels for X axis.
 * @param       uint16_t _ySize
 *              Screen size in pixels for Y axis.
 * @param       bool _flipX
 *              Flip the direction of the X axis.
 * @param       bool _flipY 
 *              Flip the direction of the Y axis.
 * @param       bool _swapXY
 *              Swap X and Y cooridinates.
 */
void CypressTouch::scale(struct cypressTouchData *_touchData, uint16_t _xSize, uint16_t _ySize, bool _flipX, bool _flipY, bool _swapXY)
{
    // Temp. variables for the mapped value.
    uint16_t _mappedX = 0;
    uint16_t _mappedY = 0;

    // Map both touch channels.
    for (int i = 0; i < 2; i++)
    {
        // Check for the flip.
        if (_flipX) _touchData->x[i] = CYPRESS_TOUCH_MAX_X - _touchData->x[i];
        if (_flipY) _touchData->y[i] = CYPRESS_TOUCH_MAX_Y - _touchData->y[i];

        // Check for X and Y swap.
        if (_swapXY)
        {
            uint16_t _temp = _touchData->x[i];
            _touchData->x[i] = _touchData->y[i];
            _touchData->y[i] = _temp;
        }

        // Map X value.
        _mappedX = map(_touchData->x[i], 0, CYPRESS_TOUCH_MAX_X, 0, _xSize);

        // Map Y value.
        _mappedX = map(_touchData->y[i], 0, CYPRESS_TOUCH_MAX_Y, 0, _ySize);
    }
}

/**
 * @brief       Enable or disable power to the Touchscreen Controller.
 * 
 * @param       bool _pwr
 *              true - Enable power to the Touchscreen/Touchscreen Controller.
 *              false - Disable power to the Touchscreen/Touchscreen Controller to reduce power
 *              consunption in sleep or to do power cycle.
 */
void CypressTouch::power(bool _pwr)
{
    if (_pwr)
    {
        // Enable the power MOSFET.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_PWR_MOS_PIN, HIGH, IO_INT_ADDR);

        // Wait a little bit before proceeding any further.
        delay(50);

        // Set reset pin to high.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, HIGH, IO_INT_ADDR);

        // Wait a little bit.
        delay(50);
    }
    else
    {
        // Disable the power MOSFET switch.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_PWR_MOS_PIN, LOW, IO_INT_ADDR);

        // Wait a bit to discharge caps.
        delay(50);

        // Set reset pin to low.
        _displayPtr->digitalWriteIO(CYPRESS_TOUCH_RST_PIN, LOW, IO_INT_ADDR);
    }
}

/**
 * @brief       Method does a HW reset by using RST pin on the Touchscreen/Touchscreen Controller.
 * 
 */
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

/**
 * @brief       Method executes a SW reset by using I2C command.
 * 
 */
void CypressTouch::swReset()
{
    // Issue a command for SW reset.
    sendCommand(CYPRESS_TOUCH_SOFT_RST_MODE);

    // Wait a little bit.
    delay(20);
}

/**
 * @brief       Function reads bootloader registers from the Touchscreen Controller.
 * 
 * @param       struct cyttspBootloaderData *_blDataPtr
 *              Defined in cypressTouchTypedefs.h, pointer to the struct cyttspBootloaderData to
 *              store bootloader registers data.
 * 
 * @return      bool
 *              true - Loading bootloader data register was successfull.
 *              false - Loading bootloader data from the registers has failed.
 */
bool CypressTouch::loadBootloaderRegs(struct cyttspBootloaderData *_blDataPtr)
{
    // Bootloader temp. registers array.
    uint8_t _bootloaderData[16];

    if (!readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _bootloaderData, 16)) return false;

    // Parse Bootloader data into typedef struct.
    memcpy(_blDataPtr, _bootloaderData, 16);

    return true;
}

/**
 * @brief       Method forces Touchscreen Controller to exit bootloader mode and enters normal
 *              operating mode - to load preloaded firmware (possibly TTSP - TrueTouch Standard Product Firmware).
 * 
 * @return      bool
 *              true - Touchscreen Controller quit bootloader mode and loaded TTSP FW that is currently executing.
 *              false - Touchscreen Controller failed to exit bootloader mode.
 * 
 * @note        It exiting bootloader mode fails reading touch events will fail. Do not go further with the code for the
 *              Touchscreen.
 */
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

    // Wait a little bit - Must be long delay, otherwise setSysInfoMode will fail!
    // Delay of 150ms will fail - tested!
    delay(500);

    // Get bootloader data.
    struct cyttspBootloaderData _bootloaderData;
    loadBootloaderRegs(&_bootloaderData);

    // Check for validity.
    if (GET_BOOTLOADERMODE(_bootloaderData.bl_status)) return false;

    // If everything went ok return true.
    return true;
}

/**
 * @brief       Set Touchscreen Controller into System Info mode.
 * 
 * @param       struct cyttspSysinfoData *_sysDataPtr
 *              Defined cypressTouchTypedefs.h, pointer to the struct for the system info registers.
 * 
 * @return      bool
 *              true - System Info mode usccessfully set.
 *              false - System Info mode failed.
 * 
 * @note        As soon as this fails, stop the Touchscreen from executing, touch data will be invalid. 
 */
bool CypressTouch::setSysInfoMode(struct cyttspSysinfoData *_sysDataPtr)
{
    // Change mode to system info.
    sendCommand(CYPRESS_TOUCH_SYSINFO_MODE);

    // Wait a bit.
    delay(20);

    // Buffer for the system info data.
    uint8_t _sysInfoArray[32];

    // Read the registers.
    if (!readI2CRegs(CYPRESS_TOUCH_BASE_ADDR, _sysInfoArray, sizeof(_sysInfoArray)))
    {
        return false;
    }

    // Copy into struct typedef.
    memcpy(_sysDataPtr, _sysInfoArray, sizeof(_sysInfoArray));

    // Do a handshake!
    handshake();

    // Check TTS version. If is zero, something went wrong.
    if (!_sysDataPtr->tts_verh && !_sysDataPtr->tts_verl)
    {
        return false;
    }

    // Everything went ok? Return true for success.
    return true;
}

/**
 * @brief       Set System info registers into their default state.
 * 
 * @param       struct cyttspSysinfoData *_sysDataPtr
 *              Defined in cypressTouchTypedefs.h, poinet to the struct for the system info registers.
 * 
 * @return      bool
 *              true - Registers are set successfully.
 *              false - Setting registers has failed.
 * 
 * @note        Stop the tuchscreen code from executing if this fails, touch data will be invalid.
 */
bool CypressTouch::setSysInfoRegs(struct cyttspSysinfoData *_sysDataPtr)
{
    // Modify registers to the default values.
    _sysDataPtr->act_intrvl = CYPRESS_TOUCH_ACT_INTRVL_DFLT;
    _sysDataPtr->tch_tmout = CYPRESS_TOUCH_TCH_TMOUT_DFLT;
    _sysDataPtr->lp_intrvl = CYPRESS_TOUCH_LP_INTRVL_DFLT;

    uint8_t _regs[] = {_sysDataPtr->act_intrvl, _sysDataPtr->tch_tmout, _sysDataPtr->lp_intrvl};

    // Send the registers to the I2C. Check if failed. If failed, return false.
    if (!writeI2CRegs(0x1D, _regs, 3)) return false;

    // Wait a little bit.
    delay(20);

    // Everything went ok? Return true for success.
    return true;
}

/**
 * @brief       Method does handshake for the Touchscreen/Touchscreen Controller to confirm successfull read
 *              new touch report data.
 * 
 * @note        Handshake must be done on every new touch event from the Interrupt.
 */
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
    // Delay between retires is 20ms (just a wildguess, don't have any documentation).
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
        delay(20);
    }

    // Got here? Not good, TSC not found, return error.
    return false;
}

// Needs to be removed.
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

/**
 * @brief       Prints out a debug message on the selected serial class.
 * 
 * @param       HardwareSerial *_serial
 *              Pointer to the Serial object.
 * @param       char *_msgPrefix
 *              Message prefix or header that will be printed before the message.
 * @param       char *_message
 *              Message that needs to be printed on the seleced serial.
 */
void CypressTouch::printMessage(HardwareSerial *_serial, char *_msgPrefix, char *_message)
{
    printTimestamp(_serial);
    _serial->print(" - [");
    _serial->print(_msgPrefix);
    _serial->print("]: ");
    _serial->print(_message);
    _serial->println();
}

/**
 * @brief       Print info message with the timestap.
 *              ex. 00:00:05;991 - [INFO]: Some info message
 * 
 * @param       HardwareSerial *_serial
 *              Pointer to the Serial object.
 * @param       char *_message
 *              Message that needs to be printed on the seleced serial.
 * 
 * @note        New line will be added at the end of each message.
 */
void CypressTouch::printInfo(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "INFO", _message);
}

/**
 * @brief       Print debug message with the timestap.
 *              ex. 00:00:01;280 - [DEBUG]: Some debug information.
 * 
 * @param       HardwareSerial *_serial
 *              Pointer to the Serial object.
 * @param       char *_message
 *              Message that needs to be printed on the seleced serial.
 * 
 * @note        New line will be added at the end of each message.
 */
void CypressTouch::printDebug(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "DEBUG", _message);
}

/**
 * @brief       Print error message with the timestap.
 *              ex. 00:00:01;280 - [ERROR]: System failure, halting!
 * 
 * @param       HardwareSerial *_serial
 *              Pointer to the Serial object.
 * @param       char *_message
 *              Message that needs to be printed on the seleced serial.
 * 
 * @note        New line will be added at the end of each message. It will also stop
 *              the code from executing.
 */
void CypressTouch::printError(HardwareSerial *_serial, char *_message)
{
    printMessage(_serial, "ERROR", _message);
    while (1)
    {
        delay(100);
    }
}

/**
 * @brief       Helper function for the printing timestamp on the serial.
 *              Timestamp source is Arduino millis() function.
 *              Format is: HH:MM:SS;MSS 
 * 
 * @param       HardwareSerial *_serial
 *              Pointer to the Serial object.
 */
void CypressTouch::printTimestamp(HardwareSerial *_serial)
{
    unsigned long _millisCapture = millis();
    int _h = _millisCapture / 3600000ULL;
    int _m = _millisCapture / 60000ULL % 60;
    int _s = _millisCapture / 1000ULL % 60;
    int _ms = _millisCapture % 1000ULL;
    _serial->printf("%02d:%02d:%02d;%03d", _h, _m, _s, _ms);
}

// -----------------------------LOW level I2C functions-----------------------------

/**
 * @brief       Method sends I2C command to the Touchscreen Controller IC.
 * 
 * @param       uint8_t _cmd
 *              I2C command for the Touchscreen Controller IC. 
 * 
 * @return      true - Command is succesfully send and executed.
 *              false - I2C command send failed.
 */
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

/**
 * @brief       Method reads multiple I2C registers at once from the touchscreen controller and save them into buffer.
 * 
 * @param       uint8_t _cmd
 *              I2C command for the Touchscreen Controller.
 * @param       uint8_t *_buffer
 *              Buffer for the bytes read from the Touchscreen Controller. 
 * @param       int _len
 *              How many bytes to read from the I2C (Touchscreen Controller).
 * 
 * @return      bool
 *              true - I2C register read was successfull.
 *              false - I2C register read failed.
 * 
 * @note        More than 32 bytes can be read at the same time.
 */
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

/**
 * @brief       Method writes multiple I2C registers at once to the touchscreen controller from buffer provided.
 * 
 * @param       uint8_t _cmd
 *              I2C command for the Touchscreen Controller.
 * @param       uint8_t *_buffer
 *              Buffer for the bytes that needs to be sent to the Touchscreen Controller. 
 * @param       int _len
 *              How many bytes to write to the I2C (Touchscreen Controller).
 * 
 * @return      bool
 *              true - I2C register write was successfull.
 *              false - I2C register write failed.
 * 
 * @note        More than 32 bytes can be written at the same time.
 */
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
