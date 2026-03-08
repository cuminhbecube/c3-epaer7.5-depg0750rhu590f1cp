#ifndef DRIVER_CONFIG_H
#define DRIVER_CONFIG_H

// --- HARDWARE PINOUT (ESP32-C3) ---
// E-Paper SPI
#define EPD_SCK     4
#define EPD_MOSI    6
#define EPD_CS      7
#define EPD_DC      5
#define EPD_RST     1
#define EPD_BUSY    0

// Alias for GxEPD2 constructor
#define PIN_CS      EPD_CS
#define PIN_DC      EPD_DC
#define PIN_RST     EPD_RST
#define PIN_BUSY    EPD_BUSY

// Button (GPIO 9 = BOOT button on ESP32-C3 devkit, has internal pull-up)
#define PIN_SWITCH_1  9

#endif
