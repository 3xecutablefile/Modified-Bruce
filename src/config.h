#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#define VERSION "1.0.0-HYBRID"
#define DEVICE_NAME "BruceInfiltra"
#define DEFAULT_AP_SSID "BruceInfiltra_AP"
#define DEFAULT_AP_PASSWORD "bruceinfiltra"
#define DEFAULT_NTP_SERVER "pool.ntp.org"

// Bruce core feature toggles
constexpr bool kEnableBruceOffensiveSuite = true;
constexpr bool kEnableBruceBleAttacks = true;
constexpr bool kEnableBruceWebExploit = true;

constexpr int kSdCsPin = 4;

// Infiltra-derived utility toggles
#ifndef ENABLE_INFILTRA_WIFI_SCANNER
#define ENABLE_INFILTRA_WIFI_SCANNER 1
#endif
#ifndef ENABLE_INFILTRA_IR_MODULE
#define ENABLE_INFILTRA_IR_MODULE 1
#endif
#ifndef ENABLE_INFILTRA_FILE_EXPLORER
#define ENABLE_INFILTRA_FILE_EXPLORER 1
#endif
#ifndef ENABLE_INFILTRA_FILE_SENDER
#define ENABLE_INFILTRA_FILE_SENDER 1
#endif
#ifndef ENABLE_INFILTRA_DEVICE_CONTROLS
#define ENABLE_INFILTRA_DEVICE_CONTROLS 1
#endif
#ifndef ENABLE_INFILTRA_RTC
#define ENABLE_INFILTRA_RTC 1
#endif

#if ENABLE_INFILTRA_WIFI_SCANNER
#define USE_WIFI_SCANNER
#endif
#if ENABLE_INFILTRA_IR_MODULE
#define USE_IR_MODULE
#endif
#if ENABLE_INFILTRA_FILE_EXPLORER
#define USE_FILE_EXPLORER
#endif
#if ENABLE_INFILTRA_FILE_SENDER
#define USE_FILE_SENDER
#endif
#if ENABLE_INFILTRA_DEVICE_CONTROLS
#define USE_DEVICE_CONTROLS
#endif
#if ENABLE_INFILTRA_RTC
#define USE_RTC_MODULE
#endif

// Hardware availability guard macros
inline bool HasDisplay() {
    return true;  // M5StickC PLUS2 always has a TFT display
}

inline bool HasIRTransceiver() {
#if defined(ARDUINO_M5Stick_C_PLUS2)
    return true;
#else
    return false;
#endif
}

inline bool HasWiFi() {
    return WiFi.getMode() != WIFI_MODE_NULL;
}

inline bool HasBatterySensor() {
    return true;
}

inline bool HasRtc() {
    return true;
}
