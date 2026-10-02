#pragma once
// ===========================================================================
//  secrets.h -- LOCAL CREDENTIALS. DO NOT COMMIT THIS FILE.
//
//  This file is git-ignored and is NOT part of a fresh clone. Create it by
//  copying include/secrets.example.h, then replace the placeholders below with
//  your own values.
//
//  If this file is ever force-added to git, remove it again before pushing:
//  it must never contain real credentials in the repository.
// ===========================================================================

#define WIFI_SSID         "YourPhoneHotspotName"
#define WIFI_PASS         "simplepassword"
#define DEEPSEEK_API_KEY  "sk-REPLACE_ME"

// On first boot the firmware will prompt for WiFi credentials on the device,
// so the placeholders above only need to compile -- they do not have to work.
