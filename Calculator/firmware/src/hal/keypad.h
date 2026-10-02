#pragma once
// ===========================================================================
//  hal/keypad.h -- abstract keypad with debounce / auto-repeat / long-press.
//
//  Two concrete drivers are provided and selected by KEYPAD_DRIVER in
//  config.h:
//    * Tca8418Keypad -- I2C keypad controller (prototype default)
//    * GpioKeypad    -- direct row/column GPIO scan
//
//  Both only implement `beginImpl()` and `readMatrix()` (a live 56-bit
//  pressed mask). The shared base class turns that mask into clean RawKey
//  events using the timing constants from config.h.
// ===========================================================================
#include <stdint.h>

#include "config.h"
#include "util/ring_buffer.h"

enum class KeyEventType : uint8_t {
  Press = 0,
  Repeat = 1,
  Long = 2,
  Release = 3,
};

struct RawKey {
  uint8_t index;          // physical matrix index 0..MATRIX_KEYS-1
  KeyEventType type;
  uint32_t stamp;         // millis() when the event was generated
};

class Keypad {
 public:
  Keypad();
  virtual ~Keypad() {}

  // Initialise the driver. `now` is millis() at call time.
  bool begin(uint32_t now);

  // Advance the scanner and return at most one event. Non-blocking; call it
  // every UI tick. Returns false when no event is pending.
  bool poll(uint32_t now, RawKey* out);

  uint8_t keyCount() const { return MATRIX_KEYS; }

 protected:
  virtual bool beginImpl() = 0;
  // Fill *mask with one bit per physical key: 1 == currently pressed.
  virtual bool readMatrix(uint64_t* mask) = 0;

 private:
  void scan(uint32_t now);

  uint8_t lastRaw_[MATRIX_KEYS];
  uint8_t stable_[MATRIX_KEYS];
  uint32_t changeAt_[MATRIX_KEYS];
  uint32_t pressAt_[MATRIX_KEYS];
  uint32_t repeatAt_[MATRIX_KEYS];
  uint8_t longSent_[MATRIX_KEYS];

  RingBuffer<RawKey, KEYPAD_QUEUE_LEN> queue_;
  uint32_t lastScanMs_;
  bool initialised_;
};

// Build the driver selected by KEYPAD_DRIVER. Returns a pointer to a static
// instance (no heap allocation).
Keypad* createKeypad();
