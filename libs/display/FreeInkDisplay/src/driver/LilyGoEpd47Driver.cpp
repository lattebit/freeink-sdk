#include "LilyGoEpd47Driver.h"

#include <BoardConfig.h>

#include <cstring>

#if FREEINK_DRIVER_LILYGO_EPD47
#include <ed047tc1.h>
#include <epd_driver.h>
#include <esp_heap_caps.h>
#endif

namespace freeink {

#if FREEINK_DRIVER_LILYGO_EPD47
namespace {

uint16_t darkLutByte(uint8_t bits) {
  uint16_t out = 0;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    if (bits & (1U << bit)) out |= static_cast<uint16_t>(1U << (bit * 2));
  }
  return out;
}

uint16_t encodeFreeInkByte(uint8_t fbByte) {
  // FreeInk framebuffers use 1 = white, 0 = black. ED047TC1 row data uses
  // 01 = dark drive and 10 = clear drive, matching LilyGo's 1bpp LUT order.
  return static_cast<uint16_t>(darkLutByte(static_cast<uint8_t>(~fbByte)) |
                               static_cast<uint16_t>(darkLutByte(fbByte) << 1));
}

uint8_t reverseBits(uint8_t value) {
  value = static_cast<uint8_t>((value >> 4) | (value << 4));
  value = static_cast<uint8_t>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
  return static_cast<uint8_t>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
}

uint32_t outputTimeFor(RefreshMode mode) {
  switch (mode) {
    case RefreshMode::Full: return 500;
    case RefreshMode::Half: return 300;
    default: return 160;
  }
}

}  // namespace
#endif

PanelGeometry LilyGoEpd47Driver::geometry() const {
  const uint16_t w = BoardConfig::ACTIVE.displayWidth;
  const uint16_t h = BoardConfig::ACTIVE.displayHeight;
  const uint16_t wb = w / 8;
  return {w, h, wb, static_cast<uint32_t>(wb) * h};
}

void LilyGoEpd47Driver::begin(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  _fbW = BoardConfig::ACTIVE.displayWidth;
  _fbH = BoardConfig::ACTIVE.displayHeight;
  _fbWb = _fbW / 8;
  _fbBytes = static_cast<uint32_t>(_fbWb) * _fbH;
  _mirrorX = BoardConfig::ACTIVE.orientation.mirrorX;
  _mirrorY = BoardConfig::ACTIVE.orientation.mirrorY;

  epd_base_init(_fbW);
  allocBaseSnapshot();
  _ready = true;
#endif
}

void LilyGoEpd47Driver::deepSleep(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (_ready) epd_poweroff_all();
#endif
}

void LilyGoEpd47Driver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  (void)bus;
  (void)prev;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (_base && fb) memcpy(_base, fb, _fbBytes);
  renderFrame(fb, mode, turnOff);
#else
  (void)fb;
  (void)mode;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::displayWindow(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h, bool turnOff) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  display(bus, fb, prev, RefreshMode::Fast, turnOff);
}

void LilyGoEpd47Driver::copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) {
  (void)bus;
  (void)lsb;
}

void LilyGoEpd47Driver::copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) {
  (void)bus;
  (void)msb;
}

void LilyGoEpd47Driver::writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                                 uint16_t numRows) {
  (void)bus;
  (void)plane;
  (void)rows;
  (void)yStart;
  (void)numRows;
}

void LilyGoEpd47Driver::displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut,
                                    bool factoryMode) {
  (void)bus;
  (void)lut;
  (void)factoryMode;
#if FREEINK_DRIVER_LILYGO_EPD47
  renderFrame(_base ? _base : fb, RefreshMode::Fast, turnOff);
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
  (void)bus;
  (void)bw;
}

void LilyGoEpd47Driver::renderFrame(const uint8_t* fb, RefreshMode mode, bool turnOff) {
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!_ready || !fb) return;

  const uint32_t outputTime = outputTimeFor(mode);
  epd_poweron();
  epd_start_frame();
  for (uint16_t y = 0; y < _fbH; ++y) {
    const uint16_t srcY = _mirrorY ? static_cast<uint16_t>(_fbH - 1 - y) : y;
    writeDriveRow(fb + static_cast<uint32_t>(srcY) * _fbWb, outputTime);
  }
  epd_output_row(outputTime);
  epd_end_frame();
  if (turnOff) epd_poweroff();
#else
  (void)fb;
  (void)mode;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::writeDriveRow(const uint8_t* srcRow, uint32_t outputTimeDus) {
#if FREEINK_DRIVER_LILYGO_EPD47
  uint8_t* line = epd_get_current_buffer();
  if (!line) return;

  uint32_t* wideLine = reinterpret_cast<uint32_t*>(line);
  for (uint16_t j = 0; j < _fbW / 16; ++j) {
    uint8_t v1 = srcRow[j * 2];
    uint8_t v2 = srcRow[j * 2 + 1];
    if (_mirrorX) {
      const uint16_t src = static_cast<uint16_t>(_fbWb - 2 - j * 2);
      v1 = reverseBits(srcRow[src + 1]);
      v2 = reverseBits(srcRow[src]);
    } else {
      v1 = reverseBits(v2);
      v2 = reverseBits(srcRow[j * 2]);
    }
    wideLine[j] = (static_cast<uint32_t>(encodeFreeInkByte(v1)) << 16) | encodeFreeInkByte(v2);
  }

  epd_output_row(outputTimeDus);
#else
  (void)srcRow;
  (void)outputTimeDus;
#endif
}

void LilyGoEpd47Driver::allocBaseSnapshot() {
#if FREEINK_DRIVER_LILYGO_EPD47
  if (_base || _fbBytes == 0) return;
  _base = static_cast<uint8_t*>(heap_caps_malloc(_fbBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!_base) _base = static_cast<uint8_t*>(heap_caps_malloc(_fbBytes, MALLOC_CAP_8BIT));
  if (!_base) {
    Serial.printf("[lilygo_epd47] failed to allocate %lu-byte base snapshot; grayscale falls back to live B/W buffer\n",
                  static_cast<unsigned long>(_fbBytes));
    return;
  }
  memset(_base, 0xFF, _fbBytes);
#endif
}

PanelDriver& lilyGoEpd47Driver() {
  static LilyGoEpd47Driver instance;
  return instance;
}

}  // namespace freeink
