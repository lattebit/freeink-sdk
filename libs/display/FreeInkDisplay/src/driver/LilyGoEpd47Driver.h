#pragma once

// LilyGo-EPD47 official ESP32-S3 board driver.
//
// The board uses LilyGo's ED047TC1 low-level parallel driver (CFG shift register
// + i80/RMT row output), so this PanelDriver owns the display bus end-to-end and
// reports usesExternalBus(). The FreeInk framebuffer remains 1bpp
// (bit set = white), expanded row-by-row into the panel's two-bit drive codes.

#include "PanelDriver.h"

namespace freeink {

class LilyGoEpd47Driver : public PanelDriver {
 public:
  uint32_t spiHz() const override { return 0; }
  BusyPolarity busyPolarity() const override { return BusyPolarity::ActiveLow; }
  bool usesExternalBus() const override { return true; }
  PanelGeometry geometry() const override;

  void begin(EpdBus& bus) override;
  void deepSleep(EpdBus& bus) override;
  void display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) override;
  void displayWindow(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, uint16_t x, uint16_t y, uint16_t w,
                     uint16_t h, bool turnOff) override;

  bool supportsStripGrayscale() const override { return true; }
  void copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) override;
  void copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) override;
  void writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override;
  void displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) override;
  void cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) override;

 private:
  void renderFrame(const uint8_t* fb, RefreshMode mode, bool turnOff);
  void writeDriveRow(const uint8_t* srcRow, uint32_t outputTimeDus);
  void allocBaseSnapshot();

  uint16_t _fbW = 0;
  uint16_t _fbH = 0;
  uint16_t _fbWb = 0;
  uint32_t _fbBytes = 0;
  uint8_t* _base = nullptr;
  bool _mirrorX = false;
  bool _mirrorY = false;
  bool _ready = false;
};

PanelDriver& lilyGoEpd47Driver();

}  // namespace freeink
