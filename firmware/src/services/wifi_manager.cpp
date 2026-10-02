#include "services/wifi_manager.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "secrets.h"

namespace {
// Treat the example placeholder (any variant beginning "YourPhoneHotspot") as
// "not configured" so a fresh clone does not try to join a non-existent network.
bool compiledCredsPresent() {
  return WIFI_SSID[0] != '\0' &&
         strncmp(WIFI_SSID, "YourPhoneHotspot", 16) != 0;
}
}  // namespace

WifiManager::WifiManager()
    : state_(State::NoCredentials),
      hasCreds_(false),
      attemptStartMs_(0),
      backoffMs_(WIFI_RETRY_MIN_MS),
      nextAttemptMs_(0) {
  ssid_[0] = '\0';
  pass_[0] = '\0';
  snprintf(ip_, sizeof(ip_), "-");
}

void WifiManager::begin() {
  WiFi.persistent(false);          // we own NVS persistence ourselves
  WiFi.setAutoReconnect(false);    // the state machine handles retries
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);

  loadFromNvs();

  backoffMs_ = WIFI_RETRY_MIN_MS;
  nextAttemptMs_ = 0;
  state_ = hasCreds_ ? State::Idle : State::NoCredentials;
}

void WifiManager::loadFromNvs() {
  bool haveNvs = false;
  Preferences prefs;
  if (prefs.begin(WIFI_NVS_NAMESPACE, /*readOnly=*/true)) {
    const size_t nSsid = prefs.getString("ssid", ssid_, sizeof(ssid_));
    prefs.getString("pass", pass_, sizeof(pass_));
    prefs.end();
    haveNvs = (nSsid > 0 && ssid_[0] != '\0');
  }
  if (haveNvs) {
    hasCreds_ = true;
    return;
  }

  // No stored credentials: fall back to the values compiled into secrets.h
  // (unless they are the example placeholder). This lets a builder bake in a
  // hotspot rather than typing a password with the multi-tap keypad.
  if (compiledCredsPresent()) {
    snprintf(ssid_, sizeof(ssid_), "%s", WIFI_SSID);
    snprintf(pass_, sizeof(pass_), "%s", WIFI_PASS);
    hasCreds_ = true;
    return;
  }

  ssid_[0] = '\0';
  pass_[0] = '\0';
  hasCreds_ = false;
}

bool WifiManager::setCredentials(const char* ssid, const char* pass) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  const size_t ssidLen = strlen(ssid);
  const size_t passLen = (pass != nullptr) ? strlen(pass) : 0;
  if (ssidLen > WIFI_MAX_SSID || passLen > WIFI_MAX_PASS) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_NVS_NAMESPACE, /*readOnly=*/false)) {
    return false;
  }
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass != nullptr ? pass : "");
  prefs.end();

  snprintf(ssid_, sizeof(ssid_), "%s", ssid);
  snprintf(pass_, sizeof(pass_), "%s", pass != nullptr ? pass : "");
  hasCreds_ = true;
  backoffMs_ = WIFI_RETRY_MIN_MS;
  nextAttemptMs_ = 0;
  state_ = State::Idle;
  return true;
}

bool WifiManager::loadCredentials(char* ssid, size_t ssidCap, char* pass,
                                  size_t passCap) const {
  if (ssid == nullptr || pass == nullptr || ssidCap == 0 || passCap == 0) {
    return false;
  }
  snprintf(ssid, ssidCap, "%s", ssid_);
  snprintf(pass, passCap, "%s", pass_);
  return hasCreds_;
}

bool WifiManager::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::startConnect(uint32_t now) {
  if (!hasCreds_) {
    state_ = State::NoCredentials;
    return;
  }
  WiFi.begin(ssid_, pass_);
  state_ = State::Connecting;
  attemptStartMs_ = now;
}

void WifiManager::connectNow(uint32_t now) {
  backoffMs_ = WIFI_RETRY_MIN_MS;
  nextAttemptMs_ = 0;
  if (hasCreds_) {
    startConnect(now);
  }
}

void WifiManager::forget() {
  Preferences prefs;
  if (prefs.begin(WIFI_NVS_NAMESPACE, /*readOnly=*/false)) {
    prefs.clear();
    prefs.end();
  }
  WiFi.disconnect(true /*wifioff*/, false /*eraseap*/);
  ssid_[0] = '\0';
  pass_[0] = '\0';
  hasCreds_ = false;
  snprintf(ip_, sizeof(ip_), "-");
  state_ = State::NoCredentials;
}

void WifiManager::updateIp() {
  const IPAddress ip = WiFi.localIP();
  snprintf(ip_, sizeof(ip_), "%u.%u.%u.%u", static_cast<unsigned>(ip[0]),
           static_cast<unsigned>(ip[1]), static_cast<unsigned>(ip[2]),
           static_cast<unsigned>(ip[3]));
}

int32_t WifiManager::rssi() const {
  return connected() ? WiFi.RSSI() : 0;
}

void WifiManager::loop(uint32_t now) {
  switch (state_) {
    case State::Connected:
      if (!connected()) {
        // Link dropped: fall back to connecting so the timeout logic applies.
        state_ = State::Connecting;
        attemptStartMs_ = now;
      }
      break;

    case State::Connecting:
      if (connected()) {
        state_ = State::Connected;
        backoffMs_ = WIFI_RETRY_MIN_MS;
        updateIp();
      } else if ((now - attemptStartMs_) >= WIFI_CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(false, false);
        state_ = hasCreds_ ? State::Failed : State::NoCredentials;
        nextAttemptMs_ = now + backoffMs_;
        backoffMs_ = (backoffMs_ * 2u > WIFI_RETRY_MAX_MS) ? WIFI_RETRY_MAX_MS
                                                           : backoffMs_ * 2u;
      }
      break;

    case State::Idle:
    case State::Failed:
      if (hasCreds_ && static_cast<int32_t>(now - nextAttemptMs_) >= 0) {
        startConnect(now);
      }
      break;

    case State::NoCredentials:
    default:
      break;
  }
}
