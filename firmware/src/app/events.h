#pragma once
// ===========================================================================
//  events.h -- POD event used for ALL inter-task communication.
//
//  Every Event is trivially copyable and self-contained: the payload lives in
//  an inline char[EVENT_TEXT_LEN] so that pushing to a FreeRTOS queue performs
//  a single memcpy and never touches the heap. Keep it that way -- no String,
//  no pointers to stack data.
// ===========================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "config.h"

enum class EventType : uint8_t {
  None = 0,
  Key,          // code = keymap Action (uint8_t), value = KeyEventType
  Tick,         // periodic UI heartbeat (stamp carries millis())
  AiStart,      // text = prompt to send to DeepSeek
  AiToken,      // text = streamed delta content
  AiDone,       // value = HTTP status (0 == clean stream end), text = note
  AiError,      // value = error code (<0 transport, >0 HTTP), text = message
  AiCancel,     // request that the net task abort the active stream
  WifiState,    // value = WifiManager::State ordinal
  WifiSetSsid,  // text = SSID
  WifiSetPass,  // text = password
  WifiConnect,  // apply the stored credentials and connect immediately
  WifiForget,   // erase the stored credentials
  TimeSynced,   // value = rough epoch indicator
  Status,       // text = transient status/toast message for the UI
};

// Written to Event::code for EventType::Key. The numeric values are mirrored
// in hal/keypad.h (KeyEventType) because the HAL must not depend on app code.
enum class KeyPressKind : uint8_t {
  Press = 0,
  Repeat = 1,
  Long = 2,
  Release = 3,
};

struct Event {
  EventType type;
  uint8_t code;
  uint16_t value;
  uint32_t stamp;
  char text[EVENT_TEXT_LEN];
};

// -- small factory helpers (all inline, no allocation) ----------------------

inline void eventClear(Event& e) {
  e.type = EventType::None;
  e.code = 0;
  e.value = 0;
  e.stamp = 0;
  e.text[0] = '\0';
}

inline void eventSetText(Event& e, const char* s) {
  if (s == nullptr) {
    e.text[0] = '\0';
    return;
  }
  size_t n = strlen(s);
  if (n >= EVENT_TEXT_LEN) {
    n = EVENT_TEXT_LEN - 1;
  }
  memcpy(e.text, s, n);
  e.text[n] = '\0';
}

inline Event makeEvent(EventType t, uint32_t now = 0) {
  Event e;
  eventClear(e);
  e.type = t;
  e.stamp = now;
  return e;
}

inline Event makeValueEvent(EventType t, uint16_t v, uint32_t now = 0) {
  Event e = makeEvent(t, now);
  e.value = v;
  return e;
}

inline Event makeTextEvent(EventType t, const char* s, uint32_t now = 0) {
  Event e = makeEvent(t, now);
  eventSetText(e, s);
  return e;
}

inline Event makeKeyEvent(uint8_t actionCode, uint8_t pressKind, uint32_t now = 0) {
  Event e = makeEvent(EventType::Key, now);
  e.code = actionCode;
  e.value = pressKind;
  return e;
}
