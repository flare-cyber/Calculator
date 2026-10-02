#include "calc/calculator.h"

#include <cmath>
#include <cstdio>
#include <cstring>

Calculator::Calculator()
    : exprLen_(0), hasResult_(false), lastOk_(false) {
  expr_[0] = '\0';
  result_[0] = '\0';
}

bool Calculator::isAllowed(char c) {
  if (c >= '0' && c <= '9') return true;
  if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') return true;
  if (c == '.' || c == '(' || c == ')') return true;
  // Lower-case letters allow sin/cos/tan/log/ln/sqrt/abs/exp/pi/e to be typed.
  if (c >= 'a' && c <= 'z') return true;
  return false;
}

void Calculator::clearEntry() {
  exprLen_ = 0;
  expr_[0] = '\0';
  lastOk_ = false;
}

void Calculator::clearAll() {
  clearEntry();
  hasResult_ = false;
  result_[0] = '\0';
  history_.clear();
}

bool Calculator::insert(char c) {
  if (!isAllowed(c)) {
    return false;
  }
  if (exprLen_ + 1 >= EXPR_CAP) {
    return false;  // full
  }
  expr_[exprLen_++] = c;
  expr_[exprLen_] = '\0';
  hasResult_ = false;  // editing invalidates the previous answer
  lastOk_ = false;
  return true;
}

bool Calculator::backspace() {
  if (exprLen_ == 0) {
    return false;
  }
  expr_[--exprLen_] = '\0';
  hasResult_ = false;
  lastOk_ = false;
  return true;
}

void Calculator::formatResult(double value, char* out, size_t outCap) {
  if (out == nullptr || outCap == 0) {
    return;
  }
  // Prefer a compact integer representation when the value is (very nearly)
  // whole, otherwise use %g with enough significant digits.
  const double rounded = std::round(value);
  if (std::fabs(value - rounded) < 1e-9 && std::fabs(value) < 1e15) {
    snprintf(out, outCap, "%.0f", rounded);
  } else {
    snprintf(out, outCap, "%.10g", value);
  }
}

bool Calculator::evaluate() {
  if (exprLen_ == 0) {
    return false;
  }

  bool ok = false;
  const double v = ::evaluate(expr_, &ok);
  lastOk_ = ok;

  if (ok) {
    formatResult(v, result_, sizeof(result_));
  } else {
    snprintf(result_, sizeof(result_), "ERROR");
  }
  hasResult_ = true;

  // Record the attempt in the history ring (expression only).
  CalcLine line;
  size_t n = exprLen_;
  if (n >= CALC_HISTORY_CHARS) {
    n = CALC_HISTORY_CHARS - 1;
  }
  memcpy(line.text, expr_, n);
  line.text[n] = '\0';
  history_.pushOverwrite(line);

  return ok;
}

bool Calculator::recall(size_t i) {
  if (i >= history_.size()) {
    return false;
  }
  const char* src = history_[i].text;
  size_t n = strlen(src);
  if (n >= EXPR_CAP) {
    n = EXPR_CAP - 1;
  }
  memcpy(expr_, src, n);
  expr_[n] = '\0';
  exprLen_ = n;
  hasResult_ = false;
  return true;
}
