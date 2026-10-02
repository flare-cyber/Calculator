// ===========================================================================
//  test/test_expr.cpp -- native unit tests for the calculator engine.
//
//  Run with:  pio test -e native
//
//  The test only touches the pure-C++ code under src/calc/, so no Arduino
//  headers are required (see [env:native] in platformio.ini).
// ===========================================================================
#include <unity.h>

#include <cmath>
#include <cstring>

#include "calc/calculator.h"
#include "calc/expr.h"

namespace {

void assertEval(const char* expr, double expected) {
  bool ok = false;
  const double got = evaluate(expr, &ok);
  TEST_ASSERT_TRUE_MESSAGE(ok, expr);
  TEST_ASSERT_EQUAL_DOUBLE(expected, got);
}

void assertEvalError(const char* expr) {
  bool ok = true;
  evaluate(expr, &ok);
  TEST_ASSERT_FALSE_MESSAGE(ok, expr);
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
//  Evaluator: arithmetic and precedence
// ---------------------------------------------------------------------------
void test_addition(void) { assertEval("3+4", 7.0); }
void test_subtraction(void) { assertEval("10-3", 7.0); }
void test_multiplication(void) { assertEval("6*7", 42.0); }
void test_division(void) { assertEval("9/2", 4.5); }
void test_precedence(void) { assertEval("3+4*2", 11.0); }
void test_parentheses(void) { assertEval("(3+4)*2", 14.0); }
void test_nested_parentheses(void) { assertEval("((1+2)*(3+4))", 21.0); }
void test_decimal_leading_dot(void) { assertEval(".5+1", 1.5); }
void test_decimal_plain(void) { assertEval("1.5*2", 3.0); }

// ---------------------------------------------------------------------------
//  Power and unary minus
// ---------------------------------------------------------------------------
void test_power(void) { assertEval("2^10", 1024.0); }
void test_power_right_assoc(void) { assertEval("2^3^2", 512.0); }
void test_unary_minus_binds_below_power(void) { assertEval("-2^2", -4.0); }
void test_unary_minus_after_power(void) { assertEval("2^-2", 0.25); }
void test_double_unary_minus(void) { assertEval("--5", 5.0); }
void test_unary_plus(void) { assertEval("+5", 5.0); }

// ---------------------------------------------------------------------------
//  Functions and constants
// ---------------------------------------------------------------------------
void test_sin_zero(void) { assertEval("sin(0)", 0.0); }
void test_cos_zero(void) { assertEval("cos(0)", 1.0); }
void test_sqrt(void) { assertEval("sqrt(9)", 3.0); }
void test_log_base10(void) { assertEval("log(100)", 2.0); }
void test_ln(void) { assertEval("ln(1)", 0.0); }
void test_function_nesting(void) { assertEval("sqrt(sqrt(16))", 2.0); }
void test_pi_constant(void) { assertEval("pi", 3.14159265358979323846); }
void test_e_constant(void) { assertEval("e", 2.71828182845904523536); }
void test_abs(void) { assertEval("abs(-3)", 3.0); }

// ---------------------------------------------------------------------------
//  Error handling
// ---------------------------------------------------------------------------
void test_empty_is_error(void) { assertEvalError(""); }
void test_null_is_error(void) {
  bool ok = true;
  evaluate(nullptr, &ok);
  TEST_ASSERT_FALSE(ok);
}
void test_divide_by_zero(void) { assertEvalError("1/0"); }
void test_sqrt_negative(void) { assertEvalError("sqrt(-1)"); }
void test_log_non_positive(void) { assertEvalError("log(0)"); }
void test_unbalanced_open(void) { assertEvalError("(1+2"); }
void test_unbalanced_close(void) { assertEvalError("1+2)"); }
void test_dangling_operator(void) { assertEvalError("1+"); }
void test_unknown_identifier(void) { assertEvalError("foo(1)"); }
void test_garbage(void) { assertEvalError("1$2"); }

// ---------------------------------------------------------------------------
//  Calculator state machine
// ---------------------------------------------------------------------------
void test_calculator_build_and_evaluate(void) {
  Calculator c;
  const char* keys = "12+30";
  for (const char* p = keys; *p; ++p) {
    TEST_ASSERT_TRUE(c.insert(*p));
  }
  TEST_ASSERT_EQUAL_STRING("12+30", c.expression());
  TEST_ASSERT_TRUE(c.evaluate());
  TEST_ASSERT_EQUAL_STRING("42", c.result());
  TEST_ASSERT_TRUE(c.hasResult());
}

void test_calculator_backspace(void) {
  Calculator c;
  for (const char* p = "123"; *p; ++p) {
    c.insert(*p);
  }
  TEST_ASSERT_TRUE(c.backspace());
  TEST_ASSERT_EQUAL_STRING("12", c.expression());
}

void test_calculator_rejects_illegal(void) {
  Calculator c;
  TEST_ASSERT_FALSE(c.insert('@'));
  TEST_ASSERT_EQUAL_STRING("", c.expression());
}

void test_calculator_error_result(void) {
  Calculator c;
  for (const char* p = "1/0"; *p; ++p) {
    c.insert(*p);
  }
  TEST_ASSERT_FALSE(c.evaluate());
  TEST_ASSERT_EQUAL_STRING("ERROR", c.result());
}

void test_calculator_history(void) {
  Calculator c;
  for (const char* p = "1+1"; *p; ++p) {
    c.insert(*p);
  }
  c.evaluate();
  c.clearEntry();
  for (const char* p = "2+2"; *p; ++p) {
    c.insert(*p);
  }
  c.evaluate();

  TEST_ASSERT_EQUAL_UINT(2, c.historyCount());
  TEST_ASSERT_EQUAL_STRING("1+1", c.historyAt(0));
  TEST_ASSERT_EQUAL_STRING("2+2", c.historyAt(1));
  TEST_ASSERT_TRUE(c.recall(0));
  TEST_ASSERT_EQUAL_STRING("1+1", c.expression());
}

void test_calculator_format_result(void) {
  char buf[32];
  Calculator::formatResult(4.0, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("4", buf);
  Calculator::formatResult(3.5, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("3.5", buf);
}

// ---------------------------------------------------------------------------
//  Runner
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_addition);
  RUN_TEST(test_subtraction);
  RUN_TEST(test_multiplication);
  RUN_TEST(test_division);
  RUN_TEST(test_precedence);
  RUN_TEST(test_parentheses);
  RUN_TEST(test_nested_parentheses);
  RUN_TEST(test_decimal_leading_dot);
  RUN_TEST(test_decimal_plain);
  RUN_TEST(test_power);
  RUN_TEST(test_power_right_assoc);
  RUN_TEST(test_unary_minus_binds_below_power);
  RUN_TEST(test_unary_minus_after_power);
  RUN_TEST(test_double_unary_minus);
  RUN_TEST(test_unary_plus);
  RUN_TEST(test_sin_zero);
  RUN_TEST(test_cos_zero);
  RUN_TEST(test_sqrt);
  RUN_TEST(test_log_base10);
  RUN_TEST(test_ln);
  RUN_TEST(test_function_nesting);
  RUN_TEST(test_pi_constant);
  RUN_TEST(test_e_constant);
  RUN_TEST(test_abs);
  RUN_TEST(test_empty_is_error);
  RUN_TEST(test_null_is_error);
  RUN_TEST(test_divide_by_zero);
  RUN_TEST(test_sqrt_negative);
  RUN_TEST(test_log_non_positive);
  RUN_TEST(test_unbalanced_open);
  RUN_TEST(test_unbalanced_close);
  RUN_TEST(test_dangling_operator);
  RUN_TEST(test_unknown_identifier);
  RUN_TEST(test_garbage);
  RUN_TEST(test_calculator_build_and_evaluate);
  RUN_TEST(test_calculator_backspace);
  RUN_TEST(test_calculator_rejects_illegal);
  RUN_TEST(test_calculator_error_result);
  RUN_TEST(test_calculator_history);
  RUN_TEST(test_calculator_format_result);
  return UNITY_END();
}
