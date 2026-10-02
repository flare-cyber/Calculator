#include "app/app.h"

#include <WiFi.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include <cstdio>
#include <cstring>

#include "config.h"
#include "hal/keymap.h"
#include "secrets.h"

#if ENABLE_SERIAL_DEBUG
namespace {
// Probe the shared I2C bus and log every responding address. Expected devices
// are the OLED (0x3C) and the TCA8418 keypad controller (0x34). This is the
// quickest way to tell a wiring problem from a firmware problem at bring-up.
void scanI2cBus() {
  Serial.print("[i2c] scan:");
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
      ++found;
    }
  }
  if (found == 0) {
    Serial.print(" none (check SDA/SCL, pull-ups, 3V3)");
  }
  Serial.println();
}
}  // namespace
#endif  // ENABLE_SERIAL_DEBUG

// ESP32-C3 is single core: pin netTask with tskNO_AFFINITY there. On a
// dual-core part it is pinned to core 1 as designed.
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
static const BaseType_t kNetTaskCore = tskNO_AFFINITY;
#else
static const BaseType_t kNetTaskCore = 1;
#endif
static const BaseType_t kUiTaskCore = 0;

App::App()
    : uiQueue_(nullptr),
      netQueue_(nullptr),
      keypad_(nullptr),
      aiCancel_(false),
      ui_(display_, calc_, UiFsm::Hooks{&App::uiSendNetTrampoline, this}, &aiCancel_),
      lastWifiState_(static_cast<WifiManager::State>(0xFF)),
      hasPendingPrompt_(false),
      sessionCredsDirty_(false),
      lastHeapLogMs_(0),
      batteryPercent_(-1),
      lastBatteryMs_(0) {
  pendingPrompt_[0] = '\0';
  sessionSsid_[0] = '\0';
  sessionPass_[0] = '\0';
}

// ===========================================================================
//  Boot
// ===========================================================================
void App::begin() {
#if ENABLE_SERIAL_DEBUG
  Serial.begin(115200);
  delay(50);
  Serial.printf("\n%s %s booting\n", FW_NAME, FW_VERSION);
#endif

  // Queue creation must precede task creation.
  uiQueue_ = xQueueCreate(UI_QUEUE_LEN, sizeof(Event));
  netQueue_ = xQueueCreate(NET_QUEUE_LEN, sizeof(Event));
#if ENABLE_SERIAL_DEBUG
  if (uiQueue_ == nullptr || netQueue_ == nullptr) {
    Serial.printf("[boot] FATAL: queue alloc failed ui=%p net=%p\n",
                  uiQueue_, netQueue_);
  }
#endif

  // Display first: it also brings up the shared I2C bus.
  if (!display_.begin()) {
    Serial.println("[display] init failed");
  }

#if ENABLE_SERIAL_DEBUG
  scanI2cBus();  // expect 0x3C (OLED) and 0x34 (TCA8418)
#endif

  keypad_ = createKeypad();
  if (keypad_ != nullptr) {
    keypad_->begin(millis());
  }

  // Services (net-owned).
  wifi_.begin();
  time_.begin();
  ai_.begin(DEEPSEEK_API_KEY);

  // Battery ADC.
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  ui_.begin(millis());

  Serial.printf("[boot] free heap=%u\n", ESP.getFreeHeap());
}

void App::startTasks() {
  xTaskCreatePinnedToCore(&App::uiTaskEntry, "uiTask", UI_TASK_STACK_BYTES, this,
                          UI_TASK_PRIORITY, nullptr, kUiTaskCore);
  xTaskCreatePinnedToCore(&App::netTaskEntry, "netTask", NET_TASK_STACK_BYTES, this,
                          NET_TASK_PRIORITY, nullptr, kNetTaskCore);
}

void App::uiTaskEntry(void* arg) {
  static_cast<App*>(arg)->uiLoop();
  vTaskDelete(nullptr);
}

void App::netTaskEntry(void* arg) {
  static_cast<App*>(arg)->netLoop();
  vTaskDelete(nullptr);
}

// ===========================================================================
//  UI task
// ===========================================================================
void App::uiLoop() {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(UI_TICK_MS);

  for (;;) {
    const uint32_t now = millis();

    // 1. Scan the keypad (bounded work per tick).
    if (keypad_ != nullptr) {
      RawKey rk;
      int budget = 8;
      while (budget-- > 0 && keypad_->poll(now, &rk)) {
        const Action a = keymap_action(rk.index);
        if (a == Action::None) {
          continue;
        }
#if ENABLE_SERIAL_DEBUG
        Serial.printf("[key] idx=%u action=%s\n", rk.index, keymap_actionName(a));
#endif
        Event e = makeKeyEvent(static_cast<uint8_t>(a), static_cast<uint8_t>(rk.type), now);
        processUiEvent(e, now);
      }
    }

    // 2. Drain events coming from netTask.
    Event e;
    int budget = 8;
    while (budget-- > 0 && xQueueReceive(uiQueue_, &e, 0) == pdTRUE) {
      processUiEvent(e, now);
    }

    // 3. Housekeeping and render.
    updateBattery(now);
    ui_.tick(now);
    if (ui_.dirty()) {
      ui_.render();
    }

    vTaskDelayUntil(&lastWake, period);
  }
}

void App::processUiEvent(const Event& ev, uint32_t now) {
  ui_.handleEvent(ev, now);
}

void App::updateBattery(uint32_t now) {
  if (lastBatteryMs_ != 0 && (now - lastBatteryMs_) < BATTERY_SAMPLE_MS) {
    return;
  }
  lastBatteryMs_ = now;

  const int pinMv = analogReadMilliVolts(BATTERY_ADC_PIN);
  const int packMv = static_cast<int>(pinMv * BATTERY_DIVIDER);
  int pct = (packMv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  batteryPercent_ = pct;
  ui_.setBattery(pct);
}

void App::postNet(const Event& ev) {
  if (netQueue_ != nullptr) {
    // Block briefly rather than drop: a lost WifiConnect/AiStart would leave
    // the device stuck with no user-visible error.
    if (xQueueSend(netQueue_, &ev, pdMS_TO_TICKS(50)) != pdTRUE) {
#if ENABLE_SERIAL_DEBUG
      Serial.printf("[app] netQueue full, dropped event type=%u\n",
                    static_cast<unsigned>(ev.type));
#endif
    }
  }
}

void App::postUi(const Event& ev) {
  if (uiQueue_ != nullptr) {
    // Small blocking timeout: token loss is worse than a brief stall.
    if (xQueueSend(uiQueue_, &ev, pdMS_TO_TICKS(50)) != pdTRUE) {
#if ENABLE_SERIAL_DEBUG
      Serial.printf("[app] uiQueue full, dropped event type=%u\n",
                    static_cast<unsigned>(ev.type));
#endif
    }
  }
}

void App::uiSendNetTrampoline(const Event& ev, void* ctx) {
  static_cast<App*>(ctx)->postNet(ev);
}

// ===========================================================================
//  Network task
// ===========================================================================
void App::netLoop() {
  for (;;) {
    const uint32_t now = millis();

    // 1. Consume work from the UI (bounded blocking so the loop stays live).
    Event e;
    if (xQueueReceive(netQueue_, &e, pdMS_TO_TICKS(20)) == pdTRUE) {
      processNetEvent(e, now);
    }

    // 2. Keep the WiFi state machine moving.
    wifi_.loop(millis());
    postWifiStateIfChanged(millis());

    // 3. Periodic heap telemetry.
    const uint32_t t = millis();
    if ((t - lastHeapLogMs_) > 30000) {
      lastHeapLogMs_ = t;
#if ENABLE_SERIAL_DEBUG
      Serial.printf("[heap] free=%u largest=%u\n", ESP.getFreeHeap(),
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void App::processNetEvent(const Event& ev, uint32_t now) {
  switch (ev.type) {
    case EventType::WifiSetSsid:
      snprintf(sessionSsid_, sizeof(sessionSsid_), "%s", ev.text);
      sessionCredsDirty_ = true;
      break;

    case EventType::WifiSetPass:
      snprintf(sessionPass_, sizeof(sessionPass_), "%s", ev.text);
      sessionCredsDirty_ = true;
      break;

    case EventType::WifiConnect:
      if (sessionCredsDirty_) {
        wifi_.setCredentials(sessionSsid_, sessionPass_);
        sessionCredsDirty_ = false;
      }
      wifi_.connectNow(now);
      break;

    case EventType::WifiForget:
      wifi_.forget();
      break;

    case EventType::AiCancel:
      aiCancel_ = true;
      break;

    case EventType::AiStart:
      snprintf(pendingPrompt_, sizeof(pendingPrompt_), "%s", ev.text);
      hasPendingPrompt_ = true;
      aiCancel_ = false;
      // If we are already online and past SNTP, run immediately; otherwise the
      // connection callback in postWifiStateIfChanged() will pick it up.
      if (wifi_.connected() && time_.synced()) {
        char prompt[EVENT_TEXT_LEN];
        snprintf(prompt, sizeof(prompt), "%s", pendingPrompt_);
        hasPendingPrompt_ = false;
        runAiTurn(prompt);
      } else if (!wifi_.connected()) {
        wifi_.connectNow(now);
        postUi(makeTextEvent(EventType::Status, "connecting WiFi...", now));
      }
      break;

    default:
      break;
  }
}

void App::postWifiStateIfChanged(uint32_t now) {
  const WifiManager::State st = wifi_.state();
  if (st != lastWifiState_) {
    lastWifiState_ = st;
    postUi(makeValueEvent(EventType::WifiState, static_cast<uint16_t>(st), now));
    if (st == WifiManager::State::Connected) {
      // Restart SNTP now that we have connectivity.
      time_.begin();
    }
  }

  // Retry a queued AI prompt once we are online and time is valid.
  if (hasPendingPrompt_ && wifi_.connected()) {
    if (time_.synced()) {
      char prompt[EVENT_TEXT_LEN];
      snprintf(prompt, sizeof(prompt), "%s", pendingPrompt_);
      hasPendingPrompt_ = false;
      runAiTurn(prompt);
    } else {
      // Best effort SYNC; runAiTurn performs the final blocking wait.
      char prompt[EVENT_TEXT_LEN];
      snprintf(prompt, sizeof(prompt), "%s", pendingPrompt_);
      hasPendingPrompt_ = false;
      runAiTurn(prompt);
    }
  }
}

void App::runAiTurn(const char* prompt) {
  aiCancel_ = false;
  postUi(makeTextEvent(EventType::Status, "asking AI...", millis()));

  // SNTP MUST be valid before TLS or certificate validation fails.
  if (!time_.ensureSynced(SNTP_TIMEOUT_MS)) {
    postUi(makeTextEvent(EventType::AiError, "SNTP sync failed", millis()));
    // Deliver a terminal event so the UI leaves the streaming state.
    Event done = makeEvent(EventType::AiDone, millis());
    done.value = 0;
    postUi(done);
    return;
  }

  AiSink sink;
  sink.onToken = &App::aiTokenTrampoline;
  sink.onStatus = &App::aiStatusTrampoline;
  sink.ctx = this;

  ai_.chat(prompt, sink, &aiCancel_);

#if ENABLE_SERIAL_DEBUG
  Serial.printf("[ai] http=%d bytes=%u free=%u largest=%u\n", ai_.lastHttpStatus(),
                static_cast<unsigned>(ai_.lastReplyBytes()), ESP.getFreeHeap(),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}

// ---------------------------------------------------------------------------
//  AiSink trampolines (netTask -> uiQueue)
// ---------------------------------------------------------------------------
void App::aiTokenTrampoline(const char* token, void* ctx) {
  App* self = static_cast<App*>(ctx);
  self->postUi(makeTextEvent(EventType::AiToken, token, millis()));
}

void App::aiStatusTrampoline(int code, const char* message, void* ctx) {
  App* self = static_cast<App*>(ctx);
  const uint32_t now = millis();

  if (code == 0) {
    Event e = makeEvent(EventType::AiDone, now);
    e.value = 0;
    self->postUi(e);
    return;
  }

  Event e = makeTextEvent(EventType::AiError, message, now);
  if (code > 0) {
    // HTTP-level error (401/402/429/5xx ...): keep the status code.
    e.code = 1;
    e.value = static_cast<uint16_t>(code > 0xFFFF ? 0xFFFF : code);
  } else {
    // Transport / client-side error.
    e.code = 2;
    e.value = 0;
  }
  self->postUi(e);
}
