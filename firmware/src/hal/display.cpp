#include "hal/display.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kLineHeight = 16;          // 4 rows * 16 px = 64 px
constexpr uint8_t kSmallBaseline = 10;       // baseline of row 0 (small font)
constexpr uint8_t kLargeBaseline = 14;
constexpr uint8_t kLargeLineHeight = 20;
}  // namespace

Display::Display()
#if DISPLAY_CONTROLLER == DISPLAY_CTRL_SH1106
    : u8g2_(U8G2_R0, U8X8_PIN_NONE) {
#else
    : u8g2_(U8G2_R0, U8X8_PIN_NONE) {
#endif
}

bool Display::begin() {
  // Bring up the shared I2C bus first so the keypad can use it too.
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

  // U8g2 expects the 8-bit I2C address (7-bit << 1).
  u8g2_.setI2CAddress(static_cast<uint8_t>(DISPLAY_I2C_ADDR << 1));
  u8g2_.setBusClock(I2C_FREQ_HZ);

  if (!u8g2_.begin()) {
    return false;
  }

  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.clearBuffer();
  u8g2_.sendBuffer();
  return true;
}

void Display::clear() { u8g2_.clearBuffer(); }

void Display::flush() { u8g2_.sendBuffer(); }

void Display::useSmallFont() { u8g2_.setFont(u8g2_font_6x10_tf); }

void Display::useLargeFont() { u8g2_.setFont(u8g2_font_9x15_tf); }

void Display::drawStr(uint8_t x, uint8_t baseline, const char* text) {
  if (text == nullptr) {
    return;
  }
  u8g2_.drawStr(x, baseline, text);
}

void Display::drawLine(uint8_t row, const char* text, bool large) {
  if (text == nullptr || row >= DISPLAY_TEXT_ROWS) {
    return;
  }
  if (large) {
    useLargeFont();
    u8g2_.drawStr(0, static_cast<uint8_t>(kLargeBaseline + row * kLargeLineHeight), text);
    useSmallFont();
  } else {
    useSmallFont();
    u8g2_.drawStr(0, static_cast<uint8_t>(kSmallBaseline + row * kLineHeight), text);
  }
}

void Display::drawStatusBar(const char* left, const char* right) {
  // Inverted strip across the top of the panel.
  u8g2_.setDrawColor(1);
  u8g2_.drawBox(0, 0, DISPLAY_WIDTH, UI_STATUS_BAR_H);
  u8g2_.setDrawColor(0);
  useSmallFont();
  if (left != nullptr) {
    u8g2_.drawStr(2, UI_STATUS_BAR_H - 2, left);
  }
  if (right != nullptr) {
    const uint8_t w = static_cast<uint8_t>(u8g2_.getStrWidth(right));
    const int x = static_cast<int>(DISPLAY_WIDTH) - 2 - w;
    u8g2_.drawStr(x < 0 ? 0 : static_cast<uint8_t>(x), UI_STATUS_BAR_H - 2, right);
  }
  u8g2_.setDrawColor(1);
}
