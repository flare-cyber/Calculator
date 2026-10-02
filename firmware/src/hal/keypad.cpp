#include "hal/keypad.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/keymap.h"

// ===========================================================================
//  Common debounce / repeat / long-press engine
// ===========================================================================

Keypad::Keypad() : lastScanMs_(0), initialised_(false) {
  for (uint8_t i = 0; i < MATRIX_KEYS; ++i) {
    lastRaw_[i] = 0;
    stable_[i] = 0;
    changeAt_[i] = 0;
    pressAt_[i] = 0;
    repeatAt_[i] = 0;
    longSent_[i] = 0;
  }
}

bool Keypad::begin(uint32_t now) {
  lastScanMs_ = now;
  initialised_ = beginImpl();
  return initialised_;
}

void Keypad::scan(uint32_t now) {
  if (!initialised_) {
    return;
  }
  // Rate-limit the physical scan; debounce timing is still exact because it
  // is measured in milliseconds, not scan counts.
  if (now - lastScanMs_ < KEYPAD_SCAN_INTERVAL_MS) {
    return;
  }
  lastScanMs_ = now;

  uint64_t mask = 0;
  if (!readMatrix(&mask)) {
    return;
  }

  for (uint8_t i = 0; i < MATRIX_KEYS; ++i) {
    const uint8_t raw = (mask >> i) & 0x1u;

    if (raw != lastRaw_[i]) {
      lastRaw_[i] = raw;
      changeAt_[i] = now;
    }

    // Debounce: only accept a level that has been stable long enough.
    if ((now - changeAt_[i]) >= KEYPAD_DEBOUNCE_MS && stable_[i] != raw) {
      stable_[i] = raw;
      if (raw) {
        pressAt_[i] = now;
        repeatAt_[i] = now + KEYPAD_REPEAT_MS;
        longSent_[i] = 0;
        RawKey ev{i, KeyEventType::Press, now};
        queue_.pushOverwrite(ev);
      } else {
        RawKey ev{i, KeyEventType::Release, now};
        queue_.pushOverwrite(ev);
      }
    }

    // Held-key behaviour: one long-press event, then periodic repeats.
    if (stable_[i] && raw) {
      if (!longSent_[i] && (now - pressAt_[i]) >= KEYPAD_LONG_MS) {
        longSent_[i] = 1;
        RawKey ev{i, KeyEventType::Long, now};
        queue_.pushOverwrite(ev);
      }
      if (static_cast<int32_t>(now - repeatAt_[i]) >= 0) {
        repeatAt_[i] = now + KEYPAD_REPEAT_MS;
        RawKey ev{i, KeyEventType::Repeat, now};
        queue_.pushOverwrite(ev);
      }
    }
  }
}

bool Keypad::poll(uint32_t now, RawKey* out) {
  scan(now);

  RawKey ev;
  if (!queue_.pop(&ev)) {
    return false;
  }
  // Drop events for unmapped keys so the UI never sees Action::None.
  while (keymap_action(ev.index) == Action::None) {
    if (!queue_.pop(&ev)) {
      return false;
    }
  }
  if (out != nullptr) {
    *out = ev;
  }
  return true;
}

// ===========================================================================
//  TCA8418 I2C keypad controller
// ===========================================================================

namespace {

// TCA8418 register map.
constexpr uint8_t TCA_REG_CFG = 0x01;
constexpr uint8_t TCA_REG_INT_STAT = 0x02;
constexpr uint8_t TCA_REG_KEY_LCK_EC = 0x03;
constexpr uint8_t TCA_REG_KEY_EVENT_A = 0x04;
constexpr uint8_t TCA_REG_KP_GPIO1 = 0x1D;
constexpr uint8_t TCA_REG_KP_GPIO2 = 0x1E;
constexpr uint8_t TCA_REG_KP_GPIO3 = 0x1F;

class Tca8418Keypad : public Keypad {
 protected:
  bool beginImpl() override {
    // Probe the device first.
    Wire.beginTransmission(TCA8418_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
      return false;
    }

    // ROW0..ROW7 belong to the keypad.
    if (!writeReg(TCA_REG_KP_GPIO1, 0xFF)) return false;
    // COL0..COL6 belong to the keypad (COL7..COL9 unused).
    uint8_t colMask = 0;
    for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
      colMask |= static_cast<uint8_t>(1u << c);
    }
    if (!writeReg(TCA_REG_KP_GPIO2, colMask)) return false;
    if (!writeReg(TCA_REG_KP_GPIO3, 0x00)) return false;

    // Enable key-event interrupts/FIFO.
    if (!writeReg(TCA_REG_CFG, 0x01)) return false;

    // Clear any stale interrupt status and drain the FIFO.
    uint8_t dummy = 0;
    readReg(TCA_REG_INT_STAT, &dummy);
    return true;
  }

  bool readMatrix(uint64_t* mask) override {
    uint8_t ec = 0;
    if (!readReg(TCA_REG_KEY_LCK_EC, &ec)) {
      return false;
    }
    const uint8_t count = ec & 0x0F;  // number of events in the FIFO

    for (uint8_t i = 0; i < count; ++i) {
      uint8_t ev = 0;
      if (!readReg(TCA_REG_KEY_EVENT_A, &ev)) {
        break;
      }
      if (ev == 0) {
        continue;
      }
      const bool pressed = (ev & 0x80) != 0;
      const uint8_t code = ev & 0x7F;
      if (code == 0) {
        continue;
      }
      const uint8_t idx = code - 1;                 // TCA8418 codes are 1-based
      const uint8_t row = idx / TCA8418_STRIDE;     // stride is 10 internally
      const uint8_t col = idx % TCA8418_STRIDE;
      if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        continue;
      }
      const uint8_t mi = row * MATRIX_COLS + col;
      if (pressed) {
        mask_ |= (1ull << mi);
      } else {
        mask_ &= ~(1ull << mi);
      }
    }

    *mask = mask_;
    return true;
  }

 private:
  bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(TCA8418_I2C_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }

  bool readReg(uint8_t reg, uint8_t* value) {
    Wire.beginTransmission(TCA8418_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }
    if (Wire.requestFrom(static_cast<int>(TCA8418_I2C_ADDR), 1) != 1) {
      return false;
    }
    *value = static_cast<uint8_t>(Wire.read());
    return true;
  }

  uint64_t mask_ = 0;
};

// ===========================================================================
//  Direct GPIO row/column scanner
// ===========================================================================

class GpioKeypad : public Keypad {
 public:
  GpioKeypad() {
    const uint8_t rows[MATRIX_ROWS] = KEYPAD_ROW_PINS;
    const uint8_t cols[MATRIX_COLS] = KEYPAD_COL_PINS;
    for (uint8_t i = 0; i < MATRIX_ROWS; ++i) rows_[i] = rows[i];
    for (uint8_t i = 0; i < MATRIX_COLS; ++i) cols_[i] = cols[i];
  }

 protected:
  bool beginImpl() override {
    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
      pinMode(rows_[r], OUTPUT);
      digitalWrite(rows_[r], HIGH);  // rows idle high
    }
    for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
      pinMode(cols_[c], INPUT_PULLUP);  // columns pulled up, read low
    }
    return true;
  }

  bool readMatrix(uint64_t* mask) override {
    uint64_t m = 0;
    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
      digitalWrite(rows_[r], LOW);
      delayMicroseconds(5);  // let the line settle
      for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
        if (digitalRead(cols_[c]) == LOW) {
          const uint8_t mi = r * MATRIX_COLS + c;
          m |= (1ull << mi);
        }
      }
      digitalWrite(rows_[r], HIGH);
    }
    *mask = m;
    return true;
  }

 private:
  uint8_t rows_[MATRIX_ROWS];
  uint8_t cols_[MATRIX_COLS];
};

}  // namespace

// ===========================================================================
//  Factory
// ===========================================================================

Keypad* createKeypad() {
#if KEYPAD_DRIVER == KEYPAD_DRIVER_GPIO
  static GpioKeypad instance;
#else
  static Tca8418Keypad instance;
#endif
  return &instance;
}
