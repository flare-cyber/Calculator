#pragma once
// ===========================================================================
//  calc/calculator.h -- expression state + history for the CALC screen.
//
//  Pure C++ (no Arduino dependency) so it can be exercised by the native test
//  harness alongside the evaluator.
// ===========================================================================
#include <stddef.h>

#include "calc/expr.h"
#include "config.h"
#include "util/ring_buffer.h"

struct CalcLine {
  char text[CALC_HISTORY_CHARS];
};

class Calculator {
 public:
  Calculator();

  // -- editing ----------------------------------------------------------
  void clearEntry();               // wipe the current expression only
  void clearAll();                 // wipe expression, result and history
  bool insert(char c);             // append a character if it is legal
  bool backspace();                // remove the last character
  bool evaluate();                 // evaluate, format and store the result

  // -- accessors --------------------------------------------------------
  const char* expression() const { return expr_; }
  const char* result() const { return result_; }
  bool hasResult() const { return hasResult_; }
  bool lastEvalOk() const { return lastOk_; }

  // -- history ----------------------------------------------------------
  size_t historyCount() const { return history_.size(); }
  // i == 0 is the oldest entry.
  const char* historyAt(size_t i) const { return history_[i].text; }
  // Replace the current expression with a history entry (used by Up/Down).
  bool recall(size_t i);

  // Format a value the way the calculator displays it (integer when exact).
  static void formatResult(double value, char* out, size_t outCap);

 private:
  static bool isAllowed(char c);

  char expr_[EXPR_CAP];
  size_t exprLen_;
  char result_[RESULT_CAP];
  bool hasResult_;
  bool lastOk_;
  RingBuffer<CalcLine, CALC_HISTORY_LEN> history_;
};
