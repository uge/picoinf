# PicoInf - Pico Infrastructure Library

This library helps you write high-level event-driven embedded C++ code operating the [Raspberry Pi Pico and Pico W](https://www.raspberrypi.com/products/raspberry-pi-pico/).

This library builds on top of the [Pico SDK](https://www.raspberrypi.com/documentation/pico-sdk/), the [FreeRTOS](https://www.freertos.org/) RTOS, and more.

The primary features of this library are:
- Write C++ applications quickly and easily, don't worry about the fiddly details of the SDK and embedded systems
- All application code (your code) lives in user-space, no interrupt-context callbacks to worry about
- All callbacks are based on high-level C++ lambda functions (so, easy to implement)
- Full C++ STL library support
- Outputs include .uf2 file for easy sharing of application
- Convenience APIs for most common Pico SDK operations:
  - Event Manager core
  - Microsecond-precise timers
  - Pin manipulation (and interrupt support)
  - Hardware UART and USB UART streaming
  - Flash storage via LittleFS and native
  - I2C, PWM, Watchdog
  - Clock speed control
  - Bluetooth (BLE) Service and Listener
  - ... and more
- Higher-level functionality such as:
  - Hardware UART Shell command extensions
  - JSON message handling via USB serial
  - JavaScript execution
  - Microsecond-precise event timeline auditing 
  - Arduino shim interface for incorporating external libraries
  - Logging
  - ... and more
- Application-specific code which should probably live elsewhere, like:
  - GPS module operation
  - WSPR functionality


Note:
- This is not an Arduino library
- This is a C++ library, you build your app, you flash directly to the pico via SWD (eg via [JLink](https://www.segger.com/products/debug-probes/j-link/models/j-link-edu-mini/))
- There is no further documentation than this README (sorry -- see History)


History:
- I built this library to support a few other projects not intending to make it visible to others
- Then I decided to make it visible to others, that's why some things aren't quite perfect


Finally:
- I hope you find something useful or interesting here
- Unfortunately no support is available for your use of this library
- All code subject to change at any time, with no warning, including going private
- Please see the LICENSE file


Thanks.

