#include "hal/keymap.h"

// ---------------------------------------------------------------------------
//  8 rows x 7 columns physical layout.
//
//  Index = row * MATRIX_COLS + col. Actions left-to-right, top-to-bottom.
//  Replace the entries below once the real membrane is mapped.
// ---------------------------------------------------------------------------
static const uint8_t kMatrix[MATRIX_KEYS] = {
    // row 0
    (uint8_t)Action::D7, (uint8_t)Action::D8, (uint8_t)Action::D9,
    (uint8_t)Action::Del, (uint8_t)Action::Clear,
    (uint8_t)Action::ParenOpen, (uint8_t)Action::ParenClose,
    // row 1
    (uint8_t)Action::D4, (uint8_t)Action::D5, (uint8_t)Action::D6,
    (uint8_t)Action::Add, (uint8_t)Action::Sub,
    (uint8_t)Action::Mul, (uint8_t)Action::Div,
    // row 2
    (uint8_t)Action::D1, (uint8_t)Action::D2, (uint8_t)Action::D3,
    (uint8_t)Action::Pow, (uint8_t)Action::Sin,
    (uint8_t)Action::Cos, (uint8_t)Action::Tan,
    // row 3
    (uint8_t)Action::D0, (uint8_t)Action::Dot, (uint8_t)Action::Eq,
    (uint8_t)Action::Ans, (uint8_t)Action::Log,
    (uint8_t)Action::Ln, (uint8_t)Action::Sqrt,
    // row 4
    (uint8_t)Action::Shift, (uint8_t)Action::Mode, (uint8_t)Action::Up,
    (uint8_t)Action::Down, (uint8_t)Action::Left,
    (uint8_t)Action::Right, (uint8_t)Action::Ai,
    // row 5
    (uint8_t)Action::On, (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    // row 6 (spare)
    (uint8_t)Action::None, (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    // row 7 (spare)
    (uint8_t)Action::None, (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
    (uint8_t)Action::None, (uint8_t)Action::None,
};

Action keymap_action(uint8_t index) {
  if (index >= MATRIX_KEYS) {
    return Action::None;
  }
  return static_cast<Action>(kMatrix[index]);
}

const char* keymap_actionName(Action a) {
  switch (a) {
    case Action::D0: return "0";
    case Action::D1: return "1";
    case Action::D2: return "2";
    case Action::D3: return "3";
    case Action::D4: return "4";
    case Action::D5: return "5";
    case Action::D6: return "6";
    case Action::D7: return "7";
    case Action::D8: return "8";
    case Action::D9: return "9";
    case Action::Dot: return ".";
    case Action::Add: return "+";
    case Action::Sub: return "-";
    case Action::Mul: return "*";
    case Action::Div: return "/";
    case Action::Pow: return "^";
    case Action::Eq: return "=";
    case Action::ParenOpen: return "(";
    case Action::ParenClose: return ")";
    case Action::Del: return "DEL";
    case Action::Clear: return "AC";
    case Action::Shift: return "SHIFT";
    case Action::Mode: return "MODE";
    case Action::Ans: return "ANS";
    case Action::Up: return "UP";
    case Action::Down: return "DOWN";
    case Action::Left: return "LEFT";
    case Action::Right: return "RIGHT";
    case Action::Sin: return "SIN";
    case Action::Cos: return "COS";
    case Action::Tan: return "TAN";
    case Action::Log: return "LOG";
    case Action::Ln: return "LN";
    case Action::Sqrt: return "SQRT";
    case Action::Ai: return "AI";
    case Action::On: return "ON";
    default: return "-";
  }
}

const char* keymap_multitapChars(char digit) {
  switch (digit) {
    case '1': return ".,?!1";
    case '2': return "abc2";
    case '3': return "def3";
    case '4': return "ghi4";
    case '5': return "jkl5";
    case '6': return "mno6";
    case '7': return "pqrs7";
    case '8': return "tuv8";
    case '9': return "wxyz9";
    case '0': return " 0";
    default: return "";
  }
}

bool keymap_isDigitAction(Action a, char* outDigit) {
  if (a < Action::D0 || a > Action::D9) {
    return false;
  }
  if (outDigit != nullptr) {
    *outDigit = static_cast<char>('0' + (static_cast<uint8_t>(a) - static_cast<uint8_t>(Action::D0)));
  }
  return true;
}
