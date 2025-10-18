#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include <memory>

#include "config.h"
// LEGAL NOTICE: This firmware is for authorized security testing and education only.
// Unauthorized use against networks/devices you don't own is illegal.
// Users assume all legal responsibility.

#include "core/Logger.h"
#include "modules/Module.h"
#include "modules/bruce/BleAttackModule.h"
#include "modules/bruce/OffensiveSuiteModule.h"
#include "modules/bruce/WebExploitModule.h"
#ifdef USE_DEVICE_CONTROLS
#include "modules/infiltra/BrightnessBatteryModule.h"
#endif
#ifdef USE_FILE_EXPLORER
#include "modules/infiltra/FileExplorerModule.h"
#endif
#ifdef USE_FILE_SENDER
#include "modules/infiltra/FileSenderModule.h"
#endif
#ifdef USE_IR_MODULE
#include "modules/infiltra/IrModule.h"
#endif
#ifdef USE_RTC_MODULE
#include "modules/infiltra/RtcModule.h"
#endif
#ifdef USE_WIFI_SCANNER
#include "modules/infiltra/WiFiScannerModule.h"
#endif
#include "services/OtaService.h"
#include "ui/MenuSystem.h"

using bruce::modules::BleAttackModule;
using bruce::modules::OffensiveSuiteModule;
using bruce::modules::WebExploitModule;
using bruce::services::OtaService;
using bruce::MenuSystem;

namespace {
AsyncWebServer g_web_server(80);
MenuSystem g_menu;
OtaService g_ota;
unsigned long g_last_activity = 0;
bool g_low_battery_warned = false;
constexpr unsigned long kAutoSleepMillis = 5UL * 60UL * 1000UL;
}

void TouchActivity() { g_last_activity = millis(); }

void SetupNetworking() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin();
    WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);
    BRUCE_LOG_INFO("WiFi STA/AP configured");
}

void RegisterModules() {
    if (kEnableBruceOffensiveSuite) {
        g_menu.RegisterModule([]() { return std::make_shared<OffensiveSuiteModule>(); });
    }
    if (kEnableBruceBleAttacks) {
        g_menu.RegisterModule([]() { return std::make_shared<BleAttackModule>(); });
    }
    if (kEnableBruceWebExploit) {
        g_menu.RegisterModule([]() { return std::make_shared<WebExploitModule>(g_web_server); });
    }

#ifdef USE_WIFI_SCANNER
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::WiFiScannerModule>(); });
#endif
#ifdef USE_IR_MODULE
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::IrModule>(); });
#endif
#ifdef USE_FILE_EXPLORER
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::FileExplorerModule>(); });
#endif
#ifdef USE_FILE_SENDER
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::FileSenderModule>(g_web_server); });
#endif
#ifdef USE_DEVICE_CONTROLS
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::BrightnessBatteryModule>(); });
#endif
#ifdef USE_RTC_MODULE
    g_menu.RegisterModule([]() { return std::make_shared<bruce::modules::infiltra::RtcModule>(); });
#endif
}

void setup() {
    bruce::Logger::Init();
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(3);
    TouchActivity();

    if (!M5.Power.begin()) {
        BRUCE_LOG_WARN("Power subsystem init failed at boot");
    }

    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("BRUCE + INFILTRA");
    M5.Display.setTextSize(1);
    M5.Display.println("Hybrid Firmware");
    M5.Display.println(VERSION);
    delay(2000);

    BRUCE_LOG_INFO("Bruce+Infiltra hybrid booting");

    SetupNetworking();
    g_ota.Begin();

    g_menu.SetActivityCallback(TouchActivity);
    RegisterModules();
    g_web_server.begin();
    g_menu.Begin();
    g_menu.ShowSystemMessage(String("Modules: ") + g_menu.ModuleCount());
}

void CheckBattery() {
    int level = M5.Power.getBatteryLevel();
    if (level >= 0 && level < 10 && !g_low_battery_warned) {
        BRUCE_LOG_WARN("Battery below 10%");
        if (!g_menu.IsModuleActive()) {
            g_menu.ShowSystemMessage("Battery <10%", TFT_RED);
        }
        g_low_battery_warned = true;
    }
}

void loop() {
    g_ota.Loop();
    g_menu.Loop();
    CheckBattery();
    if (!g_menu.IsModuleActive() && millis() - g_last_activity > kAutoSleepMillis) {
        BRUCE_LOG_INFO("Auto-sleep engaged after inactivity");
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.println("Sleeping...");
        delay(250);
        M5.Power.powerOff();
        TouchActivity();
    }
    delay(10);
}
