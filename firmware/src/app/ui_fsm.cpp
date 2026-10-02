#include "app/ui_fsm.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "ui/renderer.h"

namespace {
// Layout constants (see renderer::drawWrapped for the baseline semantics).
constexpr uint8_t kCalcExprBaseline = 24;
constexpr uint8_t kCalcExprLineHeight = 13;
constexpr uint8_t kCalcExprRows = 2;
constexpr uint8_t kCalcResultBaseline = 60;

constexpr uint8_t kTextBaseline = 26;
constexpr uint8_t kTextLineHeight = 13;
constexpr uint8_t kTextRows = 3;

constexpr uint8_t kAiBaseline = 22;
constexpr uint8_t kAiLineHeight = 13;
constexpr uint8_t kAiRows = 4;

constexpr uint8_t kWifiSsidBaseline = 28;
constexpr uint8_t kWifiPassBaseline = 44;
constexpr uint8_t kWifiHintBaseline = 60;
}  // namespace

UiFsm::UiFsm(Display& display, Calculator& calc, const Hooks& hooks,
             volatile bool* cancelFlag)
    : display_(display),
      calc_(calc),
      hooks_(hooks),
      cancelFlag_(cancelFlag),
      mode_(Mode::Boot),
      dirty_(true),
      bootUntilMs_(0),
      replyLen_(0),
      replyScroll_(0),
      aiStreaming_(false),
      wifiField_(0),
      wifiState_(static_cast<uint8_t>(WifiManager::State::Idle)),
      toastUntilMs_(0),
      batteryPercent_(-1),
      historyCursor_(0) {
  reply_[0] = '\0';
  errorText_[0] = '\0';
  toast_[0] = '\0';
  statusRight_[0] = '\0';
}

void UiFsm::begin(uint32_t now) {
  mode_ = Mode::Boot;
  bootUntilMs_ = now + 900;
  dirty_ = true;
}

void UiFsm::setMode(Mode m, uint32_t now) {
  if (mode_ == m) {
    return;
  }
  mode_ = m;
  dirty_ = true;
  if (m == Mode::Calc) {
    historyCursor_ = 0;
  }
  if (m == Mode::WifiConfig) {
    beginWifiForm();
  }
  (void)now;
}

void UiFsm::sendNet(const Event& ev) {
  if (hooks_.sendNet != nullptr) {
    hooks_.sendNet(ev, hooks_.ctx);
  }
}

// ---------------------------------------------------------------------------
//  Event handling
// ---------------------------------------------------------------------------
void UiFsm::handleEvent(const Event& ev, uint32_t now) {
  switch (ev.type) {
    case EventType::Key:
      handleKey(ev, now);
      break;

    case EventType::AiToken:
      if (mode_ != Mode::AiResponse) {
        setMode(Mode::AiResponse, now);
      }
      aiStreaming_ = true;
      appendReply(ev.text);
      dirty_ = true;
      break;

    case EventType::AiDone:
      aiStreaming_ = false;
      dirty_ = true;
      break;

    case EventType::AiError:
      aiStreaming_ = false;
      // Keep the partial reply and append a compact error marker.
      appendReply("\n[");
      appendReply(ev.text[0] != '\0' ? ev.text : "error");
      appendReply("]");
      dirty_ = true;
      break;

    case EventType::WifiState: {
      wifiState_ = static_cast<uint8_t>(ev.value);
      if (mode_ == Mode::WifiConfig &&
          wifiState_ == static_cast<uint8_t>(WifiManager::State::Connected)) {
        setToast("WiFi connected", now);
        setMode(Mode::Calc, now);
      }
      dirty_ = true;
      break;
    }

    case EventType::TimeSynced:
      dirty_ = true;
      break;

    case EventType::Status:
      setToast(ev.text, now);
      break;

    default:
      break;
  }
}

void UiFsm::handleKey(const Event& ev, uint32_t now) {
  if (mode_ == Mode::Boot) {
    return;
  }

  const Action action = static_cast<Action>(ev.code);
  const uint8_t kind = ev.value;

  // ERROR mode dismisses on any key.
  if (mode_ == Mode::Error) {
    setMode(Mode::Calc, now);
    return;
  }

  switch (mode_) {
    case Mode::Calc:
      handleCalcKey(action, kind, now);
      break;
    case Mode::TextInput:
      handleTextKey(action, kind, now);
      break;
    case Mode::AiResponse:
      handleAiKey(action, kind, now);
      break;
    case Mode::WifiConfig:
      handleWifiKey(action, kind, now);
      break;
    default:
      break;
  }
}

void UiFsm::insertInto(Calculator& c, const char* s) {
  for (const char* p = s; p != nullptr && *p != '\0'; ++p) {
    c.insert(*p);
  }
}

void UiFsm::handleCalcKey(Action a, uint8_t kind, uint32_t now) {
  const bool isPress = (kind == static_cast<uint8_t>(KeyPressKind::Press));
  const bool isRepeat = (kind == static_cast<uint8_t>(KeyPressKind::Repeat));
  const bool isLong = (kind == static_cast<uint8_t>(KeyPressKind::Long));

  char digit = '\0';
  if (keymap_isDigitAction(a, &digit)) {
    if (isPress) {
      calc_.insert(digit);
      dirty_ = true;
    }
    return;
  }

  switch (a) {
    case Action::Dot: if (isPress) { calc_.insert('.'); dirty_ = true; } break;
    case Action::Add: if (isPress) { calc_.insert('+'); dirty_ = true; } break;
    case Action::Sub: if (isPress) { calc_.insert('-'); dirty_ = true; } break;
    case Action::Mul: if (isPress) { calc_.insert('*'); dirty_ = true; } break;
    case Action::Div: if (isPress) { calc_.insert('/'); dirty_ = true; } break;
    case Action::Pow: if (isPress) { calc_.insert('^'); dirty_ = true; } break;
    case Action::ParenOpen: if (isPress) { calc_.insert('('); dirty_ = true; } break;
    case Action::ParenClose: if (isPress) { calc_.insert(')'); dirty_ = true; } break;
    case Action::Sin: if (isPress) { insertInto(calc_, "sin("); dirty_ = true; } break;
    case Action::Cos: if (isPress) { insertInto(calc_, "cos("); dirty_ = true; } break;
    case Action::Tan: if (isPress) { insertInto(calc_, "tan("); dirty_ = true; } break;
    case Action::Log: if (isPress) { insertInto(calc_, "log("); dirty_ = true; } break;
    case Action::Ln: if (isPress) { insertInto(calc_, "ln("); dirty_ = true; } break;
    case Action::Sqrt: if (isPress) { insertInto(calc_, "sqrt("); dirty_ = true; } break;

    case Action::Del:
      if (isPress || isRepeat) {
        calc_.backspace();
        dirty_ = true;
      }
      break;

    case Action::Clear:
      if (isLong) {
        calc_.clearAll();
        historyCursor_ = 0;
        dirty_ = true;
      } else if (isPress) {
        calc_.clearEntry();
        historyCursor_ = 0;
        dirty_ = true;
      }
      break;

    case Action::Eq:
      if (isPress) {
        if (calc_.evaluate()) {
          historyCursor_ = 0;
          dirty_ = true;
        } else if (calc_.expression()[0] != '\0') {
          setToast("ERROR", now);
        }
      }
      break;

    case Action::Up:
      if (isPress || isRepeat) {
        // Walk backwards through the history ring (0 == newest).
        if (historyCursor_ + 1 < calc_.historyCount()) {
          ++historyCursor_;
        }
        calc_.recall(calc_.historyCount() - 1 - historyCursor_);
        dirty_ = true;
      }
      break;

    case Action::Down:
      if (isPress || isRepeat) {
        if (historyCursor_ > 0) {
          --historyCursor_;
        }
        calc_.recall(calc_.historyCount() - 1 - historyCursor_);
        dirty_ = true;
      }
      break;

    case Action::Shift:
      if (isPress) {
        text_.toggleCaps();
        setToast(text_.caps() ? "ALPHA" : "alpha", now);
      }
      break;

    case Action::Mode:
      // Context-sensitive: while offline MODE opens WIFI_CONFIG (that is the
      // only thing a disconnected user can usefully do next); once connected
      // MODE opens TEXT_INPUT. This also makes first-run setup reachable.
      if (isPress) {
        if (wifiConnected()) {
          setMode(Mode::TextInput, now);
        } else {
          setMode(Mode::WifiConfig, now);
        }
      }
      break;

    case Action::Ai:
      if (isPress) {
        if (calc_.expression()[0] != '\0') {
          if (wifiConnected()) {
            startAi(calc_.expression(), now);
          } else {
            setToast("connect WiFi first", now);
            setMode(Mode::WifiConfig, now);
          }
        } else {
          setToast("type something first", now);
        }
      }
      break;

    default:
      break;
  }
}

void UiFsm::handleTextKey(Action a, uint8_t kind, uint32_t now) {
  const bool isPress = (kind == static_cast<uint8_t>(KeyPressKind::Press));
  const bool isRepeat = (kind == static_cast<uint8_t>(KeyPressKind::Repeat));

  char digit = '\0';
  if (keymap_isDigitAction(a, &digit)) {
    if (isPress) {
      text_.multitapDigit(digit, now);
      dirty_ = true;
    }
    return;
  }

  switch (a) {
    case Action::Del:
      if (isPress || isRepeat) {
        text_.backspace();
        dirty_ = true;
      }
      break;

    case Action::Clear:
      if (isPress) {
        text_.clear();
        dirty_ = true;
      }
      break;

    case Action::Dot:
      if (isPress) { text_.insert('.'); dirty_ = true; }
      break;

    case Action::Shift:
      if (isPress) {
        text_.toggleCaps();
        dirty_ = true;
      }
      break;

    case Action::Eq:      // ENTER
    case Action::Ai:
      if (isPress) {
        text_.multitapCommit();
        if (text_.length() > 0) {
          if (wifiConnected()) {
            startAi(text_.c_str(), now);
          } else {
            setToast("connect WiFi first", now);
            setMode(Mode::WifiConfig, now);
          }
        } else {
          setToast("nothing to send", now);
        }
      }
      break;

    case Action::Mode:
      if (isPress) {
        text_.multitapCommit();
        setMode(Mode::Calc, now);
      }
      break;

    default:
      break;
  }
}

void UiFsm::handleAiKey(Action a, uint8_t kind, uint32_t now) {
  const bool isPress = (kind == static_cast<uint8_t>(KeyPressKind::Press));
  const bool isRepeat = (kind == static_cast<uint8_t>(KeyPressKind::Repeat));

  const int total = renderer::wrapCount(reply_, DISPLAY_TEXT_COLS);
  const int maxScroll = (total > kAiRows) ? (total - kAiRows) : 0;

  switch (a) {
    case Action::Up:
      if (isPress || isRepeat) {
        if (replyScroll_ > 0) {
          --replyScroll_;
          dirty_ = true;
        }
      }
      break;

    case Action::Down:
      if (isPress || isRepeat) {
        if (replyScroll_ < maxScroll) {
          ++replyScroll_;
          dirty_ = true;
        }
      }
      break;

    case Action::Clear:  // AC cancels the stream and returns to CALC
      if (isPress) {
        if (cancelFlag_ != nullptr) {
          *cancelFlag_ = true;
        }
        setMode(Mode::Calc, now);
      }
      break;

    case Action::Ai:  // follow-up question
      if (isPress) {
        if (wifiConnected()) {
          text_.clear();
          setMode(Mode::TextInput, now);
        } else {
          setToast("connect WiFi first", now);
          setMode(Mode::WifiConfig, now);
        }
      }
      break;

    case Action::Mode:
      if (isPress) {
        setMode(Mode::Calc, now);
      }
      break;

    default:
      break;
  }
}

void UiFsm::handleWifiKey(Action a, uint8_t kind, uint32_t now) {
  const bool isPress = (kind == static_cast<uint8_t>(KeyPressKind::Press));
  const bool isRepeat = (kind == static_cast<uint8_t>(KeyPressKind::Repeat));

  TextBuffer& field = (wifiField_ == 0) ? ssidField_ : passField_;

  char digit = '\0';
  if (keymap_isDigitAction(a, &digit)) {
    if (isPress) {
      field.multitapDigit(digit, now);
      dirty_ = true;
    }
    return;
  }

  switch (a) {
    case Action::Del:
      if (isPress || isRepeat) { field.backspace(); dirty_ = true; }
      break;

    case Action::Clear:
      if (isPress) { field.clear(); dirty_ = true; }
      break;

    case Action::Shift:
      if (isPress) { field.toggleCaps(); dirty_ = true; }
      break;

    case Action::Up:
      if (isPress) { wifiField_ = 0; dirty_ = true; }
      break;

    case Action::Down:
      if (isPress) { wifiField_ = 1; dirty_ = true; }
      break;

    case Action::Dot:
      if (isPress) { field.insert('.'); dirty_ = true; }
      break;

    case Action::Eq:  // ENTER: next field, then apply
      if (isPress) {
        ssidField_.multitapCommit();
        passField_.multitapCommit();
        if (wifiField_ == 0) {
          wifiField_ = 1;
          dirty_ = true;
        } else {
          applyWifiForm(now);
        }
      }
      break;

    case Action::Mode:
      if (isPress) { setMode(Mode::Calc, now); }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
//  AI helpers
// ---------------------------------------------------------------------------
void UiFsm::clearReply() {
  reply_[0] = '\0';
  replyLen_ = 0;
  replyScroll_ = 0;
}

void UiFsm::appendReply(const char* text) {
  if (text == nullptr) {
    return;
  }
  while (*text != '\0' && replyLen_ + 1 < sizeof(reply_)) {
    reply_[replyLen_++] = *text++;
  }
  reply_[replyLen_] = '\0';
}

bool UiFsm::wifiConnected() const {
  return wifiState_ == static_cast<uint8_t>(WifiManager::State::Connected);
}

void UiFsm::startAi(const char* prompt, uint32_t now) {
  clearReply();
  aiStreaming_ = true;
  if (cancelFlag_ != nullptr) {
    *cancelFlag_ = false;
  }
  sendNet(makeTextEvent(EventType::AiStart, prompt, now));
  setMode(Mode::AiResponse, now);
}

// ---------------------------------------------------------------------------
//  WiFi form helpers
// ---------------------------------------------------------------------------
void UiFsm::beginWifiForm() {
  // Prefill from any previous attempt (App pushes the stored values via the
  // Status event path is overkill; the fields simply start empty).
  wifiField_ = 0;
  ssidField_.clear();
  passField_.clear();
}

void UiFsm::applyWifiForm(uint32_t now) {
  if (ssidField_.length() == 0) {
    setToast("SSID required", now);
    wifiField_ = 0;
    return;
  }
  sendNet(makeTextEvent(EventType::WifiSetSsid, ssidField_.c_str(), now));
  sendNet(makeTextEvent(EventType::WifiSetPass, passField_.c_str(), now));
  sendNet(makeEvent(EventType::WifiConnect, now));
  setToast("connecting...", now);
}

// ---------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------
void UiFsm::setToast(const char* msg, uint32_t now) {
  if (msg == nullptr) {
    return;
  }
  snprintf(toast_, sizeof(toast_), "%s", msg);
  toastUntilMs_ = now + UI_MESSAGE_MS;
  dirty_ = true;
}

void UiFsm::tick(uint32_t now) {
  text_.tick(now);
  ssidField_.tick(now);
  passField_.tick(now);

  if (mode_ == Mode::Boot && static_cast<int32_t>(now - bootUntilMs_) >= 0) {
    setMode(Mode::Calc, now);
  }

  if (toast_[0] != '\0' && static_cast<int32_t>(now - toastUntilMs_) >= 0) {
    toast_[0] = '\0';
    dirty_ = true;
  }
}

// ---------------------------------------------------------------------------
//  Rendering
// ---------------------------------------------------------------------------
void UiFsm::render() {
  display_.clear();

  switch (mode_) {
    case Mode::Boot: renderBoot(); break;
    case Mode::Calc: renderCalc(); break;
    case Mode::TextInput: renderTextInput(); break;
    case Mode::AiResponse: renderAiResponse(); break;
    case Mode::WifiConfig: renderWifiConfig(); break;
    case Mode::Error: renderError(); break;
  }

  display_.flush();
  dirty_ = false;
}

void UiFsm::renderStatusBar() {
  const char* modeLabel = "CALC";
  switch (mode_) {
    case Mode::Boot: modeLabel = "BOOT"; break;
    case Mode::Calc: modeLabel = "CALC"; break;
    case Mode::TextInput: modeLabel = "ASK"; break;
    case Mode::AiResponse: modeLabel = "AI"; break;
    case Mode::WifiConfig: modeLabel = "WIFI"; break;
    case Mode::Error: modeLabel = "ERR"; break;
  }

  const char* wifiTag = "-";
  switch (wifiState_) {
    case static_cast<uint8_t>(WifiManager::State::Connected): wifiTag = "W"; break;
    case static_cast<uint8_t>(WifiManager::State::Connecting): wifiTag = "."; break;
    case static_cast<uint8_t>(WifiManager::State::Failed): wifiTag = "!"; break;
    case static_cast<uint8_t>(WifiManager::State::NoCredentials): wifiTag = "?"; break;
    default: wifiTag = "-"; break;
  }

  if (batteryPercent_ >= 0) {
    snprintf(statusRight_, sizeof(statusRight_), "%s %d%%", wifiTag, batteryPercent_);
  } else {
    snprintf(statusRight_, sizeof(statusRight_), "%s", wifiTag);
  }
  display_.drawStatusBar(modeLabel, statusRight_);
}

void UiFsm::renderBoot() {
  display_.useLargeFont();
  display_.drawStr(6, 26, FW_NAME);
  display_.useSmallFont();
  display_.drawStr(6, 44, "v" FW_VERSION);
  display_.drawStr(6, 58, "starting...");
}

void UiFsm::renderCalc() {
  renderStatusBar();

  renderer::drawWrapped(display_, calc_.expression(), 0, kCalcExprBaseline,
                        kCalcExprLineHeight, kCalcExprRows, DISPLAY_TEXT_COLS, 0);

  if (calc_.hasResult()) {
    if (calc_.lastEvalOk()) {
      display_.useLargeFont();
      // Right-align the result on the bottom line.
      const int w = display_.g().getStrWidth(calc_.result());
      int x = DISPLAY_WIDTH - w;
      if (x < 0) x = 0;
      display_.drawStr(static_cast<uint8_t>(x), kCalcResultBaseline, calc_.result());
      display_.useSmallFont();
    } else {
      display_.useSmallFont();
      display_.drawStr(0, kCalcResultBaseline, "ERROR");
    }
  }

  if (calc_.expression()[0] == '\0') {
    display_.useSmallFont();
    display_.drawStr(0, 40, "-> type or press MODE");
    display_.drawStr(0, 52, "-> AI to ask DeepSeek");
  }

  renderToastIfAny(millis());
}

void UiFsm::renderTextInput() {
  renderStatusBar();
  display_.useSmallFont();
  display_.drawStr(0, 18, "ASK (ENTER=send):");

  renderer::drawScrolledField(display_, 0, kTextBaseline, text_.c_str(),
                              DISPLAY_TEXT_COLS, true);

  display_.drawStr(0, kWifiHintBaseline, text_.caps() ? "[ALPHA]" : "[alpha]");
  renderToastIfAny(millis());
}

void UiFsm::renderAiResponse() {
  renderStatusBar();

  const int total = renderer::wrapCount(reply_, DISPLAY_TEXT_COLS);
  const int maxScroll = (total > kAiRows) ? (total - kAiRows) : 0;
  if (replyScroll_ > maxScroll) {
    replyScroll_ = maxScroll;
  }

  if (replyLen_ == 0) {
    renderer::drawWrapped(display_, aiStreaming_ ? "thinking..." : "(no reply)",
                          0, kAiBaseline, kAiLineHeight, kAiRows,
                          DISPLAY_TEXT_COLS, 0);
  } else {
    renderer::drawWrapped(display_, reply_, 0, kAiBaseline, kAiLineHeight, kAiRows,
                          DISPLAY_TEXT_COLS, replyScroll_);
  }

  // Scroll indicator in the bottom-right corner.
  if (maxScroll > 0) {
    char ind[12];
    snprintf(ind, sizeof(ind), "%d/%d", replyScroll_ + 1, maxScroll + 1);
    display_.useSmallFont();
    const int w = display_.g().getStrWidth(ind);
    display_.drawStr(static_cast<uint8_t>(DISPLAY_WIDTH - w - 1), 62, ind);
  }

  renderToastIfAny(millis());
}

void UiFsm::renderWifiConfig() {
  renderStatusBar();
  display_.useSmallFont();

  display_.drawStr(0, 16, "WiFi setup (UP/DOWN):");
  renderer::drawLabelledInput(display_, kWifiSsidBaseline, "SSID:",
                              ssidField_.c_str(), wifiField_ == 0, DISPLAY_TEXT_COLS);
  renderer::drawLabelledInput(display_, kWifiPassBaseline, "PASS:",
                              passField_.c_str(), wifiField_ == 1, DISPLAY_TEXT_COLS);

  const char* hint = "ENTER=next/connect  MODE=back";
  display_.drawStr(0, kWifiHintBaseline, hint);

  renderToastIfAny(millis());
}

void UiFsm::renderError() {
  renderStatusBar();
  renderer::drawWrapped(display_, errorText_, 0, 22, 12, 4, DISPLAY_TEXT_COLS, 0);
}

void UiFsm::renderToastIfAny(uint32_t now) {
  if (toast_[0] == '\0' || static_cast<int32_t>(now - toastUntilMs_) >= 0) {
    return;
  }
  // Inverted strip at the very bottom of the panel.
  display_.useSmallFont();
  const uint8_t y = 52;
  display_.g().setDrawColor(1);
  display_.g().drawBox(0, y, DISPLAY_WIDTH, 12);
  display_.g().setDrawColor(0);
  display_.g().drawStr(2, y + 10, toast_);
  display_.g().setDrawColor(1);
}
