#pragma once
// ===========================================================================
//  app/app.h -- composition root.
//
//  Owns every HAL/service/UI module and both FreeRTOS tasks:
//    * uiTask  (core 0) : keypad scan + UI FSM + rendering
//    * netTask (core 1) : WiFi / SNTP / HTTPS streaming
//
//  The two tasks never share mutable state directly. They exchange POD
//  Events through two FreeRTOS queues.
// ===========================================================================
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app/events.h"
#include "app/ui_fsm.h"
#include "calc/calculator.h"
#include "hal/display.h"
#include "hal/keypad.h"
#include "services/deepseek_client.h"
#include "services/time_service.h"
#include "services/wifi_manager.h"

class App {
 public:
  App();

  // Initialise all modules and create the IPC queues. Call from setup().
  void begin();

  // Create the two worker tasks. Call from setup() after begin().
  void startTasks();

 private:
  // -- task entry points -------------------------------------------------
  static void uiTaskEntry(void* arg);
  static void netTaskEntry(void* arg);
  void uiLoop();
  void netLoop();

  // -- UI-side -----------------------------------------------------------
  void processUiEvent(const Event& ev, uint32_t now);
  void updateBattery(uint32_t now);
  void postNet(const Event& ev);
  void postUi(const Event& ev);

  // -- net-side ----------------------------------------------------------
  void processNetEvent(const Event& ev, uint32_t now);
  void postWifiStateIfChanged(uint32_t now);
  void runAiTurn(const char* prompt);

  // -- static trampolines ------------------------------------------------
  static void uiSendNetTrampoline(const Event& ev, void* ctx);
  static void aiTokenTrampoline(const char* token, void* ctx);
  static void aiStatusTrampoline(int code, const char* message, void* ctx);

  // -- IPC ---------------------------------------------------------------
  QueueHandle_t uiQueue_;
  QueueHandle_t netQueue_;

  // -- UI-owned modules (only uiTask touches these) ----------------------
  Display display_;
  Keypad* keypad_;
  Calculator calc_;
  volatile bool aiCancel_;
  UiFsm ui_;

  // -- net-owned modules (only netTask touches these) --------------------
  WifiManager wifi_;
  TimeService time_;
  DeepSeekClient ai_;

  // -- net-owned bookkeeping --------------------------------------------
  WifiManager::State lastWifiState_;
  char pendingPrompt_[EVENT_TEXT_LEN];
  bool hasPendingPrompt_;
  char sessionSsid_[WIFI_MAX_SSID + 1];
  char sessionPass_[WIFI_MAX_PASS + 1];
  bool sessionCredsDirty_;
  uint32_t lastHeapLogMs_;

  // -- UI-side bookkeeping ----------------------------------------------
  int batteryPercent_;
  uint32_t lastBatteryMs_;
};
