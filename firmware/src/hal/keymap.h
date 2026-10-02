#pragma once
// ===========================================================================
//  hal/keymap.h -- physical key index -> logical Action, plus multi-tap text
//  mapping used by the TEXT_INPUT and WIFI_CONFIG screens.
//
//  The matrix table below is a *starting point* for the Casio FX-300MS
//  membrane. The real membrane pinout must be confirmed by buzzing out the
//  ribbon; adjust kMatrix[] in keymap.cpp until "Action" matches the key
//  legends. Nothing else in the firmware needs to change.
// ===========================================================================
#include <stdint.h>

#include "config.h"

enum class Action : uint8_t {
  None = 0,
  // digits and arithmetic
  D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
  Dot, Add, Sub, Mul, Div, Pow, Eq,
  ParenOpen, ParenClose,
  // editing / modes
  Del, Clear, Shift, Mode, Ans,
  Up, Down, Left, Right,
  // scientific functions
  Sin, Cos, Tan, Log, Ln, Sqrt,
  // AI / power
  Ai, On,
  Count
};

// Translate a raw 0..MATRIX_KEYS-1 index into an Action.
Action keymap_action(uint8_t index);

// Human-readable name (serial debug only).
const char* keymap_actionName(Action a);

// Multi-tap character sequence for a digit key, e.g. '2' -> "abc2".
// The trailing digit reproduces phone-style wrapping. Returns "" for
// non-digit input.
const char* keymap_multitapChars(char digit);

// Apply the current SHIFT/ALPHA case to a character.
inline char keymap_applyCase(char c, bool upper) {
  if (c >= 'a' && c <= 'z') {
    return upper ? static_cast<char>(c - 'a' + 'A') : c;
  }
  return c;
}

// If `a` is a digit action, write the ASCII digit into *outDigit and return
// true. Used to drive multi-tap entry from the numeric keys.
bool keymap_isDigitAction(Action a, char* outDigit);
