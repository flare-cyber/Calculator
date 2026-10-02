#pragma once
// ===========================================================================
//  services/wifi_manager.h -- non-blocking WiFi station manager.
//
//  Never calls delay() and never blocks: `loop()` is a small state machine
//  with exponential backoff. Credentials persist in NVS via Preferences.
// ===========================================================================
#include <stddef.h>
#include <stdint.h>

#include "config.h"

class WifiManager {
 public:
  enum class State : uint8_t {
    Idle = 0,           // credentials present, not currently connecting
    Connecting = 1,     // association in progress
    Connected = 2,      // got an IP
    Failed = 3,         // attempt timed out, retry scheduled
    NoCredentials = 4,  // nothing stored yet -> show WIFI_CONFIG
  };

  WifiManager();

  // Configure the radio and load any saved credentials. Does not connect.
  void begin();

  // Drive the state machine. Call regularly with millis().
  void loop(uint32_t now);

  // Persist credentials to NVS (does not connect).
  bool setCredentials(const char* ssid, const char* pass);

  // Read the stored credentials (for prefilling the WIFI_CONFIG screen).
  bool loadCredentials(char* ssid, size_t ssidCap, char* pass, size_t passCap) const;

  bool hasCredentials() const { return hasCreds_; }

  // Restart the association immediately, resetting the backoff.
  void connectNow(uint32_t now);

  // Erase stored credentials and drop the link.
  void forget();

  State state() const { return state_; }
  bool connected() const;
  const char* ipString() const { return ip_; }
  int32_t rssi() const;
  const char* ssid() const { return ssid_; }

 private:
  void loadFromNvs();
  void startConnect(uint32_t now);
  void updateIp();

  State state_;
  char ssid_[WIFI_MAX_SSID + 1];
  char pass_[WIFI_MAX_PASS + 1];
  char ip_[16];
  bool hasCreds_;
  uint32_t attemptStartMs_;
  uint32_t backoffMs_;
  uint32_t nextAttemptMs_;
};
