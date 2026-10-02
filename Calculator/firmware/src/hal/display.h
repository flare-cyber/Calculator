#pragma once
// ===========================================================================
//  hal/display.h -- U8g2 wrapper for the 1.54" 128x64 I2C OLED.
//
//  Full-buffer mode: U8g2 keeps a 128x64/8 = 1024 byte framebuffer, so all
//  drawing is done locally and pushed to the panel with a single I2C burst.
// ===========================================================================
#include <stdint.h>

#include <U8g2lib.h>

#include "config.h"

class Display {
 public:
  Display();

  // Initialise the panel (also starts the shared I2C bus).
  bool begin();

  void clear();
  void flush();

  // Raw primitives.
  void drawStr(uint8_t x, uint8_t baseline, const char* text);
  U8G2& g() { return u8g2_; }

  // Fonts used by the renderer.
  void useSmallFont();
  void useLargeFont();

  // 4-line text helper: row 0..3 with a 16 px line pitch.
  void drawLine(uint8_t row, const char* text, bool large = false);

  // Draw the top status bar (inverted strip with left/right labels).
  void drawStatusBar(const char* left, const char* right);

  uint8_t width() const { return DISPLAY_WIDTH; }
  uint8_t height() const { return DISPLAY_HEIGHT; }

 private:
#if DISPLAY_CONTROLLER == DISPLAY_CTRL_SH1106
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2_;
#else
  U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2_;
#endif
};
