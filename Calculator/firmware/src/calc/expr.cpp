#include "calc/expr.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kMaxTokens = 128;
constexpr int kMaxStack = 128;

enum TokType : uint8_t { T_NUM, T_OP, T_LP, T_RP, T_FUNC };

enum FuncId : int {
  F_SIN = 0,
  F_COS,
  F_TAN,
  F_LOG,   // base 10
  F_LN,    // natural
  F_SQRT,
  F_ABS,
  F_EXP,
};

struct Token {
  TokType type;
  double num;
  char op;
  int func;
};

// Stack entry used by the shunting-yard algorithm: either a binary/unary
// operator, a '(' marker, or a pending function application.
struct OpEntry {
  bool isFunc;
  char op;
  int func;
};

int opPrec(char op) {
  switch (op) {
    case '^': return 4;
    case 'u': return 3;  // unary minus
    case '*':
    case '/': return 2;
    case '+':
    case '-': return 1;
    default: return 0;
  }
}

bool opRightAssoc(char op) { return op == '^' || op == 'u'; }

bool lookupFunc(const char* id, int* out) {
  struct Entry {
    const char* name;
    int id;
  };
  static const Entry kFuncs[] = {
      {"sin", F_SIN}, {"cos", F_COS}, {"tan", F_TAN}, {"log", F_LOG},
      {"ln", F_LN},   {"sqrt", F_SQRT}, {"abs", F_ABS}, {"exp", F_EXP},
  };
  for (const auto& e : kFuncs) {
    if (strcmp(id, e.name) == 0) {
      *out = e.id;
      return true;
    }
  }
  return false;
}

bool lookupConst(const char* id, double* out) {
  if (strcmp(id, "pi") == 0) {
    *out = 3.14159265358979323846;
    return true;
  }
  if (strcmp(id, "e") == 0) {
    *out = 2.71828182845904523536;
    return true;
  }
  return false;
}

}  // namespace

double evaluate(const char* expr, bool* ok) {
  if (ok != nullptr) {
    *ok = false;
  }
  if (expr == nullptr) {
    return 0.0;
  }

  // ---------------------------------------------------------------------
  //  Working storage is declared `static` so it lives in .bss rather than
  //  on the caller's stack. `evaluate()` is only ever invoked from the UI
  //  task (never re-entered), and the arrays total ~8.7 KB -- far too much
  //  for the 8 KB uiTask stack. Keeping them static eliminates that risk.
  // ---------------------------------------------------------------------
  static Token tokens[kMaxTokens];
  static Token out[kMaxTokens];
  static OpEntry ops[kMaxStack];
  static double st[kMaxStack];

  // ---------------------------------------------------------------------
  //  1. Tokenize, inserting unary-minus tokens where appropriate.
  // ---------------------------------------------------------------------
  int nt = 0;

  const char* p = expr;
  bool expectOperand = true;  // true at start, after '(' and after operators

  while (*p != '\0') {
    while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    if (*p == '\0') {
      break;
    }

    const char c = *p;

    // -- number ---------------------------------------------------------
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && std::isdigit(static_cast<unsigned char>(p[1])))) {
      char* end = nullptr;
      const double v = std::strtod(p, &end);
      if (end == p) {
        return 0.0;
      }
      if (nt >= kMaxTokens) {
        return 0.0;
      }
      tokens[nt++] = Token{T_NUM, v, 0, 0};
      p = end;
      expectOperand = false;
      continue;
    }

    // -- identifier (function or constant) ------------------------------
    if (std::isalpha(static_cast<unsigned char>(c))) {
      char id[16];
      size_t n = 0;
      while ((std::isalpha(static_cast<unsigned char>(*p)) ||
              std::isdigit(static_cast<unsigned char>(*p))) &&
             n < sizeof(id) - 1) {
        id[n++] = *p++;
      }
      id[n] = '\0';

      double cv = 0.0;
      int fid = 0;
      if (lookupConst(id, &cv)) {
        if (nt >= kMaxTokens) {
          return 0.0;
        }
        tokens[nt++] = Token{T_NUM, cv, 0, 0};
        expectOperand = false;
      } else if (lookupFunc(id, &fid)) {
        if (nt >= kMaxTokens) {
          return 0.0;
        }
        tokens[nt++] = Token{T_FUNC, 0, 0, fid};
        expectOperand = true;
      } else {
        return 0.0;  // unknown identifier
      }
      continue;
    }

    // -- parentheses ----------------------------------------------------
    if (c == '(') {
      if (nt >= kMaxTokens) {
        return 0.0;
      }
      tokens[nt++] = Token{T_LP, 0, 0, 0};
      ++p;
      expectOperand = true;
      continue;
    }
    if (c == ')') {
      if (nt >= kMaxTokens) {
        return 0.0;
      }
      tokens[nt++] = Token{T_RP, 0, 0, 0};
      ++p;
      expectOperand = false;
      continue;
    }

    // -- operators ------------------------------------------------------
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
      char op = c;
      if (expectOperand) {
        if (op == '-') {
          op = 'u';  // unary minus
        } else if (op == '+') {
          ++p;  // unary plus is a no-op
          continue;
        } else {
          return 0.0;  // binary operator with a missing left operand
        }
      }
      if (nt >= kMaxTokens) {
        return 0.0;
      }
      tokens[nt++] = Token{T_OP, 0, op, 0};
      ++p;
      expectOperand = true;
      continue;
    }

    return 0.0;  // unexpected character
  }

  if (nt == 0 || expectOperand) {
    return 0.0;  // empty expression or dangling operator
  }

  // ---------------------------------------------------------------------
  //  2. Shunting-yard: infix tokens -> RPN.
  // ---------------------------------------------------------------------
  int no = 0;
  int ns = 0;

  auto popOp = [&]() -> bool {
    if (no >= kMaxTokens || ns <= 0) {
      return false;
    }
    const OpEntry e = ops[--ns];
    if (e.isFunc) {
      out[no++] = Token{T_FUNC, 0, 0, e.func};
    } else {
      out[no++] = Token{T_OP, 0, e.op, 0};
    }
    return true;
  };

  for (int i = 0; i < nt; ++i) {
    const Token& t = tokens[i];
    switch (t.type) {
      case T_NUM:
        if (no >= kMaxTokens) {
          return 0.0;
        }
        out[no++] = t;
        break;

      case T_FUNC:
        if (ns >= kMaxStack) {
          return 0.0;
        }
        ops[ns++] = OpEntry{true, 0, t.func};
        break;

      case T_OP: {
        // A prefix unary operator never pops: its operand has not been read
        // yet. This keeps `-2^2 == -(2^2)` and `2^-3 == 2^(-3)` correct.
        if (t.op == 'u') {
          if (ns >= kMaxStack) {
            return 0.0;
          }
          ops[ns++] = OpEntry{false, t.op, 0};
          break;
        }
        while (ns > 0) {
          const OpEntry& top = ops[ns - 1];
          if (top.isFunc || top.op == '(') {
            break;
          }
          const int tp = opPrec(top.op);
          const int cp = opPrec(t.op);
          if (tp > cp || (tp == cp && !opRightAssoc(t.op))) {
            if (!popOp()) {
              return 0.0;
            }
          } else {
            break;
          }
        }
        if (ns >= kMaxStack) {
          return 0.0;
        }
        ops[ns++] = OpEntry{false, t.op, 0};
        break;
      }

      case T_LP:
        if (ns >= kMaxStack) {
          return 0.0;
        }
        ops[ns++] = OpEntry{false, '(', 0};
        break;

      case T_RP: {
        bool foundLp = false;
        while (ns > 0) {
          if (!ops[ns - 1].isFunc && ops[ns - 1].op == '(') {
            foundLp = true;
            break;
          }
          if (!popOp()) {
            return 0.0;
          }
        }
        if (!foundLp) {
          return 0.0;  // unbalanced ')'
        }
        --ns;  // discard the '('
        if (ns > 0 && ops[ns - 1].isFunc) {
          if (!popOp()) {
            return 0.0;
          }
        }
        break;
      }
    }
  }

  while (ns > 0) {
    if (!ops[ns - 1].isFunc && ops[ns - 1].op == '(') {
      return 0.0;  // unbalanced '('
    }
    if (!popOp()) {
      return 0.0;
    }
  }

  // ---------------------------------------------------------------------
  //  3. Evaluate the RPN program.
  // ---------------------------------------------------------------------
  int sp = 0;

  for (int i = 0; i < no; ++i) {
    const Token& t = out[i];

    if (t.type == T_NUM) {
      if (sp >= kMaxStack) {
        return 0.0;
      }
      st[sp++] = t.num;
      continue;
    }

    if (t.type == T_OP) {
      if (t.op == 'u') {
        if (sp < 1) {
          return 0.0;
        }
        st[sp - 1] = -st[sp - 1];
        continue;
      }
      if (sp < 2) {
        return 0.0;
      }
      const double b = st[--sp];
      const double a = st[--sp];
      double r = 0.0;
      switch (t.op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/':
          if (b == 0.0) {
            return 0.0;  // division by zero
          }
          r = a / b;
          break;
        case '^': r = std::pow(a, b); break;
        default: return 0.0;
      }
      st[sp++] = r;
      continue;
    }

    if (t.type == T_FUNC) {
      if (sp < 1) {
        return 0.0;
      }
      const double a = st[sp - 1];
      double r = 0.0;
      switch (t.func) {
        case F_SIN: r = std::sin(a); break;
        case F_COS: r = std::cos(a); break;
        case F_TAN: r = std::tan(a); break;
        case F_LOG:
          if (a <= 0.0) {
            return 0.0;
          }
          r = std::log10(a);
          break;
        case F_LN:
          if (a <= 0.0) {
            return 0.0;
          }
          r = std::log(a);
          break;
        case F_SQRT:
          if (a < 0.0) {
            return 0.0;
          }
          r = std::sqrt(a);
          break;
        case F_ABS: r = std::fabs(a); break;
        case F_EXP: r = std::exp(a); break;
        default: return 0.0;
      }
      if (std::isnan(r) || std::isinf(r)) {
        return 0.0;
      }
      st[sp - 1] = r;
      continue;
    }
  }

  if (sp != 1) {
    return 0.0;
  }
  const double result = st[0];
  if (std::isnan(result) || std::isinf(result)) {
    return 0.0;
  }

  if (ok != nullptr) {
    *ok = true;
  }
  return result;
}
