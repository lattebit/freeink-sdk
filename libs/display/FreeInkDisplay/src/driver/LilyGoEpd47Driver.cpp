#include "LilyGoEpd47Driver.h"

#include <BoardConfig.h>

#include <cstring>

#if FREEINK_DRIVER_LILYGO_EPD47
#include <BoardT5EpaperS3Pins.h>
#include <FastEPD.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

int bbepIOInit(FASTEPDSTATE* state);
#endif

namespace freeink {

#if FREEINK_DRIVER_LILYGO_EPD47
namespace {

constexpr uint32_t kFastEpdBusHz = 20'000'000;
constexpr uint8_t kPartialPasses = 4;
constexpr uint8_t kFullPasses = 5;

// FastEPD custom panels require a gray matrix even when the driver only uses
// BB_MODE_1BPP. Keep this tiny placeholder out of DRAM-heavy paths.
static const uint8_t kUnusedGrayMatrix[16] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

class FastEpdPanel final : public FASTEPD {
 public:
  FASTEPDSTATE* state() { return &_state; }
};

FastEpdPanel gPanel;

uint8_t reverseBits(uint8_t value) {
  value = static_cast<uint8_t>((value >> 4) | (value << 4));
  value = static_cast<uint8_t>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
  return static_cast<uint8_t>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
}

const char* refreshModeName(RefreshMode mode) {
  switch (mode) {
    case RefreshMode::Full: return "full";
    case RefreshMode::Half: return "half";
    case RefreshMode::Fast: return "fast";
  }
  return "unknown";
}

const char* fastEpdErrorName(int rc) {
  switch (rc) {
    case BBEP_SUCCESS: return "success";
    case BBEP_ERROR_BAD_PARAMETER: return "bad_parameter";
    case BBEP_ERROR_BAD_DATA: return "bad_data";
    case BBEP_ERROR_NOT_SUPPORTED: return "not_supported";
    case BBEP_ERROR_NO_MEMORY: return "no_memory";
    case BBEP_ERROR_OUT_OF_BOUNDS: return "out_of_bounds";
    case BBEP_IO_ERROR: return "io_error";
    default: return "unknown";
  }
}

void sendShiftData(FASTEPDSTATE* state) {
  uint8_t value = state->shift_data;
  gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioShiftSTR), 0);
  for (uint8_t i = 0; i < 8; ++i) {
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioSCL), 0);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioSDA), (value & 0x80) ? 1 : 0);
    value <<= 1;
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioSCL), 1);
  }
  gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioShiftSTR), 1);
}

int legacyEinkPower(void* pBBEP, int on) {
  auto* state = static_cast<FASTEPDSTATE*>(pBBEP);
  if (on == state->pwr_on) return BBEP_SUCCESS;

  if (on) {
    state->shift_data |= 0x80;  // ep_output_enable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data &= static_cast<uint8_t>(~0x02);  // !power_disable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data |= 0x08;  // neg_power_enable
    sendShiftData(state);
    delayMicroseconds(500);
    state->shift_data |= 0x04;  // pos_power_enable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data |= 0x10;  // ep_stv
    sendShiftData(state);
    delayMicroseconds(100);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioSPH), 1);
    state->pwr_on = 1;
  } else {
    state->shift_data &= static_cast<uint8_t>(~0x04);  // pos_power_enable
    sendShiftData(state);
    delayMicroseconds(10);
    state->shift_data &= static_cast<uint8_t>(~0x08);  // neg_power_enable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data |= 0x02;  // power_disable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data &= static_cast<uint8_t>(~0x80);  // !ep_output_enable
    sendShiftData(state);
    delayMicroseconds(100);
    state->shift_data &= static_cast<uint8_t>(~0x10);  // !ep_stv
    sendShiftData(state);
    state->pwr_on = 0;
  }
  return BBEP_SUCCESS;
}

int legacyIoInit(void* pBBEP) {
  auto* state = static_cast<FASTEPDSTATE*>(pBBEP);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioSDA), GPIO_MODE_OUTPUT);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioSCL), GPIO_MODE_OUTPUT);
  gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioSCL), 0);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioShiftSTR), GPIO_MODE_OUTPUT);
  gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioShiftSTR), 0);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioCKV), GPIO_MODE_OUTPUT);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioSPH), GPIO_MODE_OUTPUT);
  gpio_set_direction(static_cast<gpio_num_t>(state->panelDef.ioCL), GPIO_MODE_OUTPUT);

  state->shift_data = 0x02;  // power_disable
  state->shift_data |= 0x20;  // ep_scan_direction
  state->shift_data |= 0x10;  // ep_stv
  sendShiftData(state);
  return BBEP_SUCCESS;
}

void legacyRowControl(void* pBBEP, int mode) {
  auto* state = static_cast<FASTEPDSTATE*>(pBBEP);
  if (mode == ROW_START) {
    state->shift_data |= 0x40;  // ep_mode
    sendShiftData(state);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 1);
    delayMicroseconds(7);
    state->shift_data &= static_cast<uint8_t>(~0x10);  // !ep_stv
    sendShiftData(state);
    delayMicroseconds(10);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 0);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 1);
    delayMicroseconds(8);
    state->shift_data |= 0x10;  // ep_stv
    sendShiftData(state);
    delayMicroseconds(10);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 0);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 1);
    delayMicroseconds(18);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 0);
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 1);
  } else if (mode == ROW_STEP) {
    gpio_set_level(static_cast<gpio_num_t>(state->panelDef.ioCKV), 0);
    state->shift_data |= 0x01;  // ep_latch_enable
    sendShiftData(state);
    state->shift_data &= static_cast<uint8_t>(~0x01);
    sendShiftData(state);
  }
}

BBPANELDEF legacyPanelDef() {
  BBPANELDEF def = {};
  def.width = 960;
  def.height = 540;
  def.bus_speed = kFastEpdBusHz;
  def.flags = BB_PANEL_FLAG_SLOW_SPH;
  def.data[0] = T5EPD47_EPD_D0;
  def.data[1] = T5EPD47_EPD_D1;
  def.data[2] = T5EPD47_EPD_D2;
  def.data[3] = T5EPD47_EPD_D3;
  def.data[4] = T5EPD47_EPD_D4;
  def.data[5] = T5EPD47_EPD_D5;
  def.data[6] = T5EPD47_EPD_D6;
  def.data[7] = T5EPD47_EPD_D7;
  def.bus_width = 8;
  def.ioPWR = BB_NOT_USED;
  def.ioSPV = BB_NOT_USED;
  def.ioCKV = T5EPD47_EPD_CKV;
  def.ioSPH = T5EPD47_EPD_STH;
  def.ioOE = BB_NOT_USED;
  def.ioLE = BB_NOT_USED;
  def.ioCL = T5EPD47_EPD_CKH;
  def.ioPWR_Good = BB_NOT_USED;
  def.ioSDA = T5EPD47_CFG_DATA;
  def.ioSCL = T5EPD47_CFG_CLK;
  def.ioShiftSTR = T5EPD47_CFG_STR;
  def.ioShiftMask = 0;
  def.ioDCDummy = T5EPD47_EPD_DUMMY;
  def.pGrayMatrix = kUnusedGrayMatrix;
  def.iMatrixSize = sizeof(kUnusedGrayMatrix);
  def.iLinePadding = 0;
  return def;
}

BBPANELPROCS legacyPanelProcs() {
  BBPANELPROCS procs = {};
  procs.pfnEinkPower = legacyEinkPower;
  procs.pfnIOInit = legacyIoInit;
  procs.pfnRowControl = legacyRowControl;
  return procs;
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

  BBPANELDEF panel = legacyPanelDef();
  BBPANELPROCS procs = legacyPanelProcs();
  int rc = gPanel.initCustomPanel(&panel, &procs);
  if (rc != BBEP_SUCCESS) {
    Serial.printf("[lilygo_epd47] FastEPD custom panel init failed rc=%d (%s)\n", rc, fastEpdErrorName(rc));
    _ready = false;
    return;
  }
  rc = ::bbepIOInit(gPanel.state());
  if (rc != BBEP_SUCCESS) {
    Serial.printf("[lilygo_epd47] FastEPD i80 bus init failed rc=%d (%s)\n", rc, fastEpdErrorName(rc));
    _ready = false;
    return;
  }
  rc = gPanel.setPanelSize(_fbW, _fbH, panel.flags);
  if (rc != BBEP_SUCCESS) {
    Serial.printf("[lilygo_epd47] FastEPD buffer allocation failed rc=%d (%s) heap=%lu psram=%lu\n", rc,
                  fastEpdErrorName(rc), static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    _ready = false;
    return;
  }
  rc = gPanel.setMode(BB_MODE_1BPP);
  if (rc != BBEP_SUCCESS) {
    Serial.printf("[lilygo_epd47] FastEPD 1bpp mode failed rc=%d (%s)\n", rc, fastEpdErrorName(rc));
    _ready = false;
    return;
  }
  gPanel.setPasses(kPartialPasses, kFullPasses);
  memset(gPanel.currentBuffer(), 0xFF, _fbBytes);
  if (gPanel.previousBuffer()) memset(gPanel.previousBuffer(), 0xFF, _fbBytes);
  _ready = true;
  _hasPresented = false;
  Serial.printf("[lilygo_epd47] FastEPD ready %ux%u fb=%lu partialPasses=%u fullPasses=%u heap=%lu psram=%lu\n", _fbW,
                _fbH, static_cast<unsigned long>(_fbBytes), kPartialPasses, kFullPasses,
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
#endif
}

void LilyGoEpd47Driver::deepSleep(EpdBus& bus) {
  (void)bus;
#if FREEINK_DRIVER_LILYGO_EPD47
  if (_ready) gPanel.einkPower(0);
#endif
}

void LilyGoEpd47Driver::display(EpdBus& bus, const uint8_t* fb, const uint8_t* prev, RefreshMode mode, bool turnOff) {
  (void)bus;
  (void)prev;
#if FREEINK_DRIVER_LILYGO_EPD47
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
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!fb || w == 0 || h == 0 || y >= _fbH) {
    renderFrame(fb, RefreshMode::Fast, turnOff);
    return;
  }
  const uint16_t clampedEnd = static_cast<uint16_t>((static_cast<uint32_t>(y) + h > _fbH) ? (_fbH - 1) : (y + h - 1));
  renderFrame(fb, RefreshMode::Fast, turnOff, y, clampedEnd);
#else
  display(bus, fb, prev, RefreshMode::Fast, turnOff);
#endif
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
  renderFrame(fb, RefreshMode::Fast, turnOff);
#else
  (void)fb;
  (void)turnOff;
#endif
}

void LilyGoEpd47Driver::cleanupGrayscaleBuffers(EpdBus& bus, const uint8_t* bw) {
  (void)bus;
  (void)bw;
}

void LilyGoEpd47Driver::renderFrame(const uint8_t* fb, RefreshMode mode, bool turnOff, int16_t startRow, int16_t endRow) {
#if FREEINK_DRIVER_LILYGO_EPD47
  if (!_ready || !fb) return;

  if (!copyFrameToFastEpd(fb)) return;

  RefreshMode effectiveMode = mode;
  if (!_hasPresented && effectiveMode == RefreshMode::Fast) effectiveMode = RefreshMode::Half;

  const int64_t startUs = esp_timer_get_time();
  int rc = BBEP_SUCCESS;
  const bool keepOn = !turnOff;
  if (effectiveMode == RefreshMode::Full) {
    rc = gPanel.fullUpdate(CLEAR_SLOW, keepOn, nullptr);
  } else if (effectiveMode == RefreshMode::Half) {
    rc = gPanel.fullUpdate(CLEAR_FAST, keepOn, nullptr);
  } else {
    if (startRow < 0 || endRow < startRow || endRow >= static_cast<int16_t>(_fbH)) {
      startRow = 0;
      endRow = static_cast<int16_t>(_fbH - 1);
    }
    rc = gPanel.partialUpdate(keepOn, startRow, endRow);
  }

  const int64_t elapsedMs = (esp_timer_get_time() - startUs) / 1000;
  if (rc == BBEP_SUCCESS) {
    _hasPresented = true;
    Serial.printf("[lilygo_epd47] FastEPD refresh requested=%s effective=%s rows=%d..%d keepOn=%d elapsed=%lldms heap=%lu "
                  "psram=%lu\n",
                  refreshModeName(mode), refreshModeName(effectiveMode), static_cast<int>(startRow), static_cast<int>(endRow),
                  keepOn ? 1 : 0, static_cast<long long>(elapsedMs), static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  } else {
    Serial.printf("[lilygo_epd47] FastEPD refresh failed requested=%s effective=%s rc=%d (%s) elapsed=%lldms\n",
                  refreshModeName(mode), refreshModeName(effectiveMode), rc, fastEpdErrorName(rc),
                  static_cast<long long>(elapsedMs));
  }
#else
  (void)fb;
  (void)mode;
  (void)turnOff;
  (void)startRow;
  (void)endRow;
#endif
}

bool LilyGoEpd47Driver::copyFrameToFastEpd(const uint8_t* fb) {
#if FREEINK_DRIVER_LILYGO_EPD47
  uint8_t* current = gPanel.currentBuffer();
  if (!current) {
    Serial.printf("[lilygo_epd47] FastEPD current buffer is null\n");
    return false;
  }

  if (!_mirrorX && !_mirrorY) {
    memcpy(current, fb, _fbBytes);
    return true;
  }

  for (uint16_t y = 0; y < _fbH; ++y) {
    const uint16_t srcY = _mirrorY ? static_cast<uint16_t>(_fbH - 1 - y) : y;
    const uint8_t* src = fb + static_cast<uint32_t>(srcY) * _fbWb;
    uint8_t* dst = current + static_cast<uint32_t>(y) * _fbWb;
    if (_mirrorX) {
      for (uint16_t x = 0; x < _fbWb; ++x) dst[x] = reverseBits(src[_fbWb - 1 - x]);
    } else {
      memcpy(dst, src, _fbWb);
    }
  }
  return true;
#else
  (void)fb;
  return false;
#endif
}

PanelDriver& lilyGoEpd47Driver() {
  static LilyGoEpd47Driver instance;
  return instance;
}

}  // namespace freeink
