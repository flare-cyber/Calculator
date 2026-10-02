#pragma once
// ===========================================================================
//  calc/expr.h -- shunting-yard expression evaluator.
//
//  Pure C++ with no Arduino dependency so the native unit test can link it
//  directly (pio test -e native).
//
//  Supported grammar:
//    - numbers:        12, 3.5, .5
//    - operators:      + - * / ^ and unary minus
//    - grouping:       ( )
//    - functions:      sin cos tan log ln sqrt abs exp
//    - constants:      pi, e
//
//  `evaluate` returns 0.0 and sets *ok=false on any lexical, syntactic or
//  domain error (division by zero, sqrt of a negative, log of a non-positive,
//  NaN/Inf results).
// ===========================================================================

double evaluate(const char* expr, bool* ok);
