#pragma once

// LilyGo-EPD47 ESP32-S3 board driver.
//
// The board uses the ED047TC1 raw parallel panel behind LilyGo's CFG shift
// register. This driver keeps the FreeInk PanelDriver API and delegates the
// waveform/update engine to FastEPD so partial updates use previous-frame diffs
// instead of a single blind scanline pass.

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

  bool supportsStripGrayscale() const override { return false; }
  void copyGrayscaleLsb(EpdBus& bus, const uint8_t* lsb) override;
  void copyGrayscaleMsb(EpdBus& bus, const uint8_t* msb) override;
  void writeGrayscalePlaneStrip(EpdBus& bus, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override;
  void displayGray(EpdBus& bus, const uint8_t* fb, bool turnOff, const unsigned char* lut, bool factoryMode) override;
  void cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) override;

 private:
  void renderFrame(const uint8_t* fb, RefreshMode mode, bool turnOff, int16_t startRow = -1, int16_t endRow = -1);
  bool copyFrameToFastEpd(const uint8_t* fb);

  uint16_t _fbW = 0;
  uint16_t _fbH = 0;
  uint16_t _fbWb = 0;
  uint32_t _fbBytes = 0;
  bool _mirrorX = false;
  bool _mirrorY = false;
  bool _ready = false;
  bool _hasPresented = false;
  uint8_t _largeFastClearCount = 0;
};

PanelDriver& lilyGoEpd47Driver();

}  // namespace freeink
