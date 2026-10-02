#pragma once
// ===========================================================================
//  ui/text_buffer.h -- fixed-capacity line editor with multi-tap input.
//
//  No heap allocation. The multi-tap engine mirrors old phone keypads:
//    * pressing the same digit again within MULTITAP_TIMEOUT_MS cycles the
//      candidate character for that position,
//    * pressing a different key (or waiting) commits it.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "config.h"

class TextBuffer {
 public:
  TextBuffer();

  void reset();                       // clear text and multi-tap state
  void clear() { reset(); }

  bool insert(char c);                // commit pending glyph, then append
  bool backspace();
  bool multitapDigit(char digit, uint32_t nowMs);
  void multitapCommit();
  bool multitapPending() const { return slot_ >= 0; }
  void tick(uint32_t nowMs);          // commit after inactivity

  const char* c_str() const { return buf_; }
  size_t length() const { return len_; }
  bool full() const { return len_ >= capacity_; }
  size_t capacity() const { return capacity_; }

  void setCaps(bool on) { caps_ = on; }
  bool caps() const { return caps_; }
  void toggleCaps() { caps_ = !caps_; }

 private:
  char buf_[TEXT_INPUT_CAP + 1];
  size_t len_;
  size_t capacity_;
  int slot_;              // index of the in-progress multi-tap glyph, or -1
  char lastDigit_;
  uint32_t lastPressMs_;
  uint8_t cycle_;
  bool caps_;
};
