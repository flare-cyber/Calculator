#pragma once
// ===========================================================================
//  app/ui_fsm.h -- the screen state machine.
//
//  Owned by uiTask. It consumes Events, drives the Calculator / TextBuffer,
//  renders frames through the Display and, when it needs the network, hands
//  an Event to the App through the `sendNet` hook (which posts to netQueue).
// ===========================================================================
#include <stdint.h>

#include "app/events.h"
#include "calc/calculator.h"
#include "hal/display.h"
#include "hal/keymap.h"
#include "services/wifi_manager.h"
#include "ui/text_buffer.h"

class UiFsm {
 public:
  enum class Mode : uint8_t {
    Boot = 0,
    Calc,
    TextInput,
    AiResponse,
    WifiConfig,
    Error,
  };

  struct Hooks {
    void (*sendNet)(const Event& ev, void* ctx);
    void* ctx;
  };

  UiFsm(Display& display, Calculator& calc, const Hooks& hooks, volatile bool* cancelFlag);

  void begin(uint32_t now);
  void handleEvent(const Event& ev, uint32_t now);
  void tick(uint32_t now);
  void render();

  // App feeds the battery estimate in (kept out of the render path).
  void setBattery(int percent) { batteryPercent_ = percent; }

  Mode mode() const { return mode_; }
  bool dirty() const { return dirty_; }
  void markDirty() { dirty_ = true; }

 private:
  // -- transitions / dispatch ------------------------------------------
  void setMode(Mode m, uint32_t now);
  void handleKey(const Event& ev, uint32_t now);
  void handleCalcKey(Action a, uint8_t kind, uint32_t now);
  void handleTextKey(Action a, uint8_t kind, uint32_t now);
  void handleAiKey(Action a, uint8_t kind, uint32_t now);
  void handleWifiKey(Action a, uint8_t kind, uint32_t now);

  // -- helpers ----------------------------------------------------------
  void sendNet(const Event& ev);
  bool wifiConnected() const;
  void startAi(const char* prompt, uint32_t now);
  void appendReply(const char* text);
  void clearReply();
  void insertInto(Calculator& c, const char* s);
  void setToast(const char* msg, uint32_t now);
  void beginWifiForm();
  void applyWifiForm(uint32_t now);

  // -- rendering --------------------------------------------------------
  void renderBoot();
  void renderCalc();
  void renderTextInput();
  void renderAiResponse();
  void renderWifiConfig();
  void renderError();
  void renderStatusBar();
  void renderToastIfAny(uint32_t now);

  Display& display_;
  Calculator& calc_;
  Hooks hooks_;
  volatile bool* cancelFlag_;
  Mode mode_;
  bool dirty_;
  uint32_t bootUntilMs_;

  // TEXT_INPUT state
  TextBuffer text_;

  // AI_RESPONSE state
  char reply_[REPLY_CAP];
  size_t replyLen_;
  int replyScroll_;
  bool aiStreaming_;

  // WIFI_CONFIG state
  TextBuffer ssidField_;
  TextBuffer passField_;
  uint8_t wifiField_;    // 0 = SSID, 1 = password
  uint8_t wifiState_;    // last WifiManager::State reported by netTask

  // ERROR / toast state
  char errorText_[EVENT_TEXT_LEN];
  char toast_[EVENT_TEXT_LEN];
  uint32_t toastUntilMs_;

  int batteryPercent_;
  char statusRight_[24];
  size_t historyCursor_;
};
