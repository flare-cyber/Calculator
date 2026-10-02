#include "ui/text_buffer.h"

#include <string.h>

#include "hal/keymap.h"

TextBuffer::TextBuffer()
    : len_(0),
      capacity_(TEXT_INPUT_CAP),
      slot_(-1),
      lastDigit_('\0'),
      lastPressMs_(0),
      cycle_(0),
      caps_(false) {
  buf_[0] = '\0';
}

void TextBuffer::reset() {
  len_ = 0;
  buf_[0] = '\0';
  slot_ = -1;
  lastDigit_ = '\0';
  lastPressMs_ = 0;
  cycle_ = 0;
}

bool TextBuffer::insert(char c) {
  multitapCommit();
  if (len_ + 1 > capacity_) {
    return false;
  }
  buf_[len_++] = c;
  buf_[len_] = '\0';
  return true;
}

bool TextBuffer::backspace() {
  if (len_ == 0) {
    return false;
  }
  if (slot_ == static_cast<int>(len_) - 1) {
    slot_ = -1;  // remove the pending glyph as well
  }
  buf_[--len_] = '\0';
  return true;
}

void TextBuffer::multitapCommit() {
  if (slot_ >= 0) {
    slot_ = -1;
    lastDigit_ = '\0';
    cycle_ = 0;
  }
}

bool TextBuffer::multitapDigit(char digit, uint32_t nowMs) {
  const char* chars = keymap_multitapChars(digit);
  if (chars == nullptr || chars[0] == '\0') {
    return false;
  }
  const size_t n = strlen(chars);

  // Cycling the same key: rewrite the pending character in place.
  if (slot_ >= 0 && digit == lastDigit_ &&
      (nowMs - lastPressMs_) <= MULTITAP_TIMEOUT_MS) {
    cycle_ = static_cast<uint8_t>((cycle_ + 1) % n);
    buf_[slot_] = keymap_applyCase(chars[cycle_], caps_);
    lastPressMs_ = nowMs;
    return true;
  }

  // New glyph: commit whatever was pending and start a fresh slot.
  multitapCommit();
  if (len_ + 1 > capacity_) {
    return false;
  }
  cycle_ = 0;
  buf_[len_] = keymap_applyCase(chars[0], caps_);
  slot_ = static_cast<int>(len_);
  ++len_;
  buf_[len_] = '\0';
  lastDigit_ = digit;
  lastPressMs_ = nowMs;
  return true;
}

void TextBuffer::tick(uint32_t nowMs) {
  if (slot_ >= 0 && (nowMs - lastPressMs_) > MULTITAP_TIMEOUT_MS) {
    multitapCommit();
  }
}
