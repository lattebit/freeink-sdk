#include <BoardT5EpaperS3Pins.h>
#include <LgfxEpdConfig.h>

#include <Arduino.h>

namespace {

struct LegacyConfigRegister {
  bool epLatchEnable : 1;
  bool powerDisable : 1;
  bool posPowerEnable : 1;
  bool negPowerEnable : 1;
  bool epStv : 1;
  bool epScanDirection : 1;
  bool epMode : 1;
  bool epOutputEnable : 1;
};

LegacyConfigRegister g_cfg = {};

void pushCfgBit(bool value) {
  digitalWrite(T5EPD47_CFG_CLK, LOW);
  digitalWrite(T5EPD47_CFG_DATA, value ? HIGH : LOW);
  digitalWrite(T5EPD47_CFG_CLK, HIGH);
}

void pushCfg() {
  digitalWrite(T5EPD47_CFG_STR, LOW);
  pushCfgBit(g_cfg.epOutputEnable);
  pushCfgBit(g_cfg.epMode);
  pushCfgBit(g_cfg.epScanDirection);
  pushCfgBit(g_cfg.epStv);
  pushCfgBit(g_cfg.negPowerEnable);
  pushCfgBit(g_cfg.posPowerEnable);
  pushCfgBit(g_cfg.powerDisable);
  pushCfgBit(g_cfg.epLatchEnable);
  digitalWrite(T5EPD47_CFG_STR, HIGH);
}

bool prepareEpdPower() {
  pinMode(T5EPD47_CFG_DATA, OUTPUT);
  pinMode(T5EPD47_CFG_CLK, OUTPUT);
  pinMode(T5EPD47_CFG_STR, OUTPUT);
  digitalWrite(T5EPD47_CFG_STR, LOW);

  g_cfg.epLatchEnable = false;
  g_cfg.powerDisable = true;
  g_cfg.posPowerEnable = false;
  g_cfg.negPowerEnable = false;
  g_cfg.epStv = true;
  g_cfg.epScanDirection = true;
  g_cfg.epMode = false;
  g_cfg.epOutputEnable = false;
  pushCfg();
  return true;
}

bool epdPowerOn() {
  g_cfg.epScanDirection = true;
  g_cfg.powerDisable = false;
  pushCfg();
  delayMicroseconds(100);
  g_cfg.negPowerEnable = true;
  pushCfg();
  delayMicroseconds(500);
  g_cfg.posPowerEnable = true;
  pushCfg();
  delayMicroseconds(100);
  g_cfg.epStv = true;
  g_cfg.epOutputEnable = true;
  pushCfg();
  digitalWrite(T5EPD47_EPD_STH, HIGH);
  return true;
}

void epdPowerOff() {
  g_cfg.epOutputEnable = false;
  g_cfg.posPowerEnable = false;
  pushCfg();
  delayMicroseconds(10);
  g_cfg.negPowerEnable = false;
  pushCfg();
  delayMicroseconds(100);
  g_cfg.powerDisable = true;
  g_cfg.epStv = false;
  g_cfg.epMode = false;
  g_cfg.epLatchEnable = false;
  pushCfg();
}

}  // namespace

namespace freeink {

const LgfxEpdConfig& lilygoEpd47S3LgfxConfig() {
  static const LgfxEpdConfig cfg = {
#if defined(FREEINK_EPD47_S3_LEGACY_I80_DATA_ORDER) && FREEINK_EPD47_S3_LEGACY_I80_DATA_ORDER
      {T5EPD47_EPD_D6, T5EPD47_EPD_D7, T5EPD47_EPD_D4, T5EPD47_EPD_D5, T5EPD47_EPD_D2, T5EPD47_EPD_D3,
       T5EPD47_EPD_D0, T5EPD47_EPD_D1},
#else
      {T5EPD47_EPD_D0, T5EPD47_EPD_D1, T5EPD47_EPD_D2, T5EPD47_EPD_D3, T5EPD47_EPD_D4, T5EPD47_EPD_D5,
       T5EPD47_EPD_D6, T5EPD47_EPD_D7},
#endif
      T5EPD47_EPD_STH,
      T5EPD47_CFG_STR,
      T5EPD47_EPD_DUMMY,
      T5EPD47_CFG_STR,
      T5EPD47_EPD_CKH,
      T5EPD47_EPD_CKV,
      T5EPD47_EPD_DUMMY,
      10000000,
      8,
      0,
      {&prepareEpdPower, &epdPowerOn, &epdPowerOff},
  };
  return cfg;
}

}  // namespace freeink
