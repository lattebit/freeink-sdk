#pragma once

#include <Arduino.h>

// LilyGo-EPD47 legacy T5-ePaper-S3 pinout.
//
// Sources:
// - LilyGo-EPD47/src/utilities.h (T5-ePaper-S3 section)
// - LilyGo-EPD47/src/ed047tc1.h (CONFIG_IDF_TARGET_ESP32S3 section)

constexpr int T5EPD47_BTN_USER = 21;
constexpr int T5EPD47_BATTERY_ADC = 14;

constexpr int T5EPD47_SD_MISO = 16;
constexpr int T5EPD47_SD_MOSI = 15;
constexpr int T5EPD47_SD_SCLK = 11;
constexpr int T5EPD47_SD_CS = 42;

constexpr int T5EPD47_TOUCH_SCL = 17;
constexpr int T5EPD47_TOUCH_SDA = 18;
constexpr int T5EPD47_TOUCH_INT = 47;

constexpr int T5EPD47_CFG_DATA = 13;
constexpr int T5EPD47_CFG_CLK = 12;
constexpr int T5EPD47_CFG_STR = 0;

constexpr int T5EPD47_EPD_CKV = 38;
constexpr int T5EPD47_EPD_STH = 40;
constexpr int T5EPD47_EPD_CKH = 41;

constexpr int T5EPD47_EPD_D0 = 8;
constexpr int T5EPD47_EPD_D1 = 1;
constexpr int T5EPD47_EPD_D2 = 2;
constexpr int T5EPD47_EPD_D3 = 3;
constexpr int T5EPD47_EPD_D4 = 4;
constexpr int T5EPD47_EPD_D5 = 5;
constexpr int T5EPD47_EPD_D6 = 6;
constexpr int T5EPD47_EPD_D7 = 7;

// GPIO39 is exposed as the legacy demo's generic SPI CS and is not part of the
// panel's ED047TC1 timing pins. LovyanGFX's Bus_EPD needs direct GPIOs for a few
// slots even when board-specific hooks own real power; keep this as the dummy.
constexpr int T5EPD47_EPD_DUMMY = 39;
