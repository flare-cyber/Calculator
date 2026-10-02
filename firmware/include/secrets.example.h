#pragma once
// ===========================================================================
//  secrets.example.h -- template for secrets.h
//
//  Copy this file to `secrets.h` (same folder) and fill in your own values.
//  `secrets.h` is intentionally NOT committed (see .gitignore note in README).
// ===========================================================================

#define WIFI_SSID      "YourPhoneHotspotName"
#define WIFI_PASS      "simplepassword"
#define DEEPSEEK_API_KEY "sk-REPLACE_ME"

// ---------------------------------------------------------------------------
//  Optional: override the pinned TLS root CA.
//  By default the firmware uses the built-in Amazon Root CA 1 PEM from
//  include/deepseek_roots.h (the CA that signs api.deepseek.com). Define
//  DEEPSEEK_ROOT_CA_PEM here only if you need to pin a different CA.
// ---------------------------------------------------------------------------
// #define DEEPSEEK_ROOT_CA_PEM "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"
