#include "services/time_service.h"

#include <Arduino.h>
#include <time.h>

#include "config.h"

TimeService::TimeService() : configured_(false) {}

void TimeService::begin() {
  // configTzTime starts SNTP and sets the TZ in one call.
  configTzTime(TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
  configured_ = true;
}

bool TimeService::synced() const {
  const time_t now = time(nullptr);
  // Reject both "not yet synced" (1970 epoch) and error values (-1). Casting
  // to uint64_t first would turn -1 into a huge number and falsely pass.
  if (now < static_cast<time_t>(SNTP_MIN_VALID_EPOCH)) {
    return false;
  }
  return true;
}

bool TimeService::ensureSynced(uint32_t timeoutMs) {
  if (!configured_) {
    begin();
  }
  if (synced()) {
    return true;
  }

  const uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    if (synced()) {
      return true;
    }
    delay(100);
  }
  return synced();
}
