#pragma once
// ===========================================================================
//  services/time_service.h -- SNTP synchronisation.
//
//  TLS certificate validation compares NotBefore/NotAfter against the system
//  clock. An ESP32 boots at 1970-01-01, so SNTP MUST succeed before the first
//  HTTPS request or every handshake fails with "certificate not valid yet".
// ===========================================================================
#include <stdint.h>

class TimeService {
 public:
  TimeService();

  // Configure SNTP (idempotent; call after WiFi is up).
  void begin();

  // True once the clock is plausibly valid (past SNTP_MIN_VALID_EPOCH).
  bool synced() const;

  // Block until the clock is valid or the timeout elapses. Only ever called
  // from netTask, never from the UI task.
  bool ensureSynced(uint32_t timeoutMs);

 private:
  bool configured_;
};
