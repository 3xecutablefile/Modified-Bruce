#pragma once

#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules {

class OffensiveSuiteModule : public Module {
   public:
    const char* Name() const override { return "WiFi Research"; }

   protected:
    void Init() override {
        EnsureDisplay();
        sd_ready_ = PrepareSd();
        LoadConfig();

        wifi_ready_ = PrepareWifi();
        if (!wifi_ready_) {
            RenderUnavailable("WiFi radio unavailable");
            return;
        }

        RenderHeader();
        ShowStatus("A:scan B:next C:save", TFT_GREEN);
        if (auto_scan_) {
            StartScan();
        } else {
            Render();
        }
    }

    void Update() override {
        if (!wifi_ready_) {
            return;
        }

        if (scanning_) {
            int16_t result = WiFi.scanComplete();
            if (result >= 0) {
                CompleteScan(result);
            } else if (result == -2) {
                scanning_ = false;
                ShowStatus("Scan failed", TFT_RED);
            }
        }

        if (info_message_active_ && millis() > info_message_until_) {
            info_message_active_ = false;
            Render();
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            StartScan();
        }

        if (!networks_.empty() && DebouncedPress(M5.BtnB, last_press_b_)) {
            selected_index_ = (selected_index_ + 1) % networks_.size();
            config_dirty_ = true;
            Render();
        }

        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            if (sd_ready_ || PrepareSd()) {
                SaveInventory();
            } else {
                ShowStatus("SD missing", TFT_RED);
            }
        }

        if (DebouncedLongPress(M5.BtnB, 1200, last_long_press_b_)) {
            auto_scan_ = !auto_scan_;
            config_dirty_ = true;
            ShowStatus(auto_scan_ ? "Auto scan ON" : "Auto scan OFF", TFT_YELLOW);
        }
    }

    void Cleanup() override {
        if (scanning_) {
            scanning_ = false;
            WiFi.scanDelete();
        }
        networks_.clear();
        if (sd_ready_) {
            SaveConfig();
            SD.end();
            sd_ready_ = false;
        }
        if (HasDisplay()) {
            M5.Display.clear();
        }
    }

   private:
    struct NetworkRecord {
        String ssid;
        String bssid;
        int32_t rssi = 0;
        int32_t channel = 0;
        wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
        bool hidden = false;
        String posture;
    };

    void EnsureDisplay() {
        if (HasDisplay()) {
            M5.Display.clear();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.setTextSize(1);
        }
    }

    bool PrepareWifi() {
        WiFi.disconnect();
        if (!WiFi.mode(WIFI_STA)) {
            BRUCE_LOG_ERROR("Unable to set STA mode");
            return false;
        }
        return true;
    }

    bool PrepareSd() {
        if (sd_ready_) {
            return true;
        }
        if (SD.begin(kSdCsPin)) {
            if (!SD.exists("/config")) {
                SD.mkdir("/config");
            }
            if (!SD.exists("/logs")) {
                SD.mkdir("/logs");
            }
            sd_ready_ = true;
            return true;
        }
        return false;
    }

    void StartScan() {
        RenderHeader();
        M5.Display.println("Scanning WiFi channels...");
        networks_.clear();
        selected_index_ = 0;
        scanning_ = true;
        WiFi.scanDelete();
        WiFi.scanNetworks(true, true);
        ShowStatus("Scanning...", TFT_YELLOW);
    }

    void CompleteScan(int16_t count) {
        scanning_ = false;
        networks_.clear();
        if (count <= 0) {
            ShowStatus("No networks", TFT_YELLOW);
            Render();
            return;
        }
        networks_.reserve(count);
        for (int i = 0; i < count; ++i) {
            NetworkRecord record;
            record.ssid = WiFi.SSID(i);
            if (record.ssid.isEmpty()) {
                record.ssid = "(hidden)";
                record.hidden = true;
            }
            record.bssid = WiFi.BSSIDstr(i);
            record.rssi = WiFi.RSSI(i);
            record.channel = WiFi.channel(i);
            record.auth = WiFi.encryptionType(i);
            record.posture = Assess(record);
            networks_.push_back(record);
            if ((i % 4) == 0) {
                yield();
            }
        }
        WiFi.scanDelete();
        std::sort(networks_.begin(), networks_.end(), [](const NetworkRecord& a, const NetworkRecord& b) {
            return a.rssi > b.rssi;
        });
        ShowStatus(String(count) + " networks", TFT_GREEN);
        Render();
    }

    String Assess(const NetworkRecord& record) const {
        String result;
        switch (record.auth) {
            case WIFI_AUTH_OPEN:
                result = "Open network";
                break;
            case WIFI_AUTH_WEP:
                result = "WEP (legacy)";
                break;
            case WIFI_AUTH_WPA_PSK:
            case WIFI_AUTH_WPA2_PSK:
            case WIFI_AUTH_WPA_WPA2_PSK:
                result = "WPA/WPA2 PSK";
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                result = "802.1X enterprise";
                break;
            case WIFI_AUTH_WPA3_PSK:
            case WIFI_AUTH_WPA2_WPA3_PSK:
                result = "WPA3 capable";
                break;
            default:
                result = "Unknown auth";
                break;
        }
        result += ", ch" + String(record.channel);
        if (record.rssi > -55) {
            result += ", strong";
        } else if (record.rssi < -80) {
            result += ", weak";
        }
        if (record.hidden) {
            result += ", hidden SSID";
        }
        if (record.auth == WIFI_AUTH_OPEN || record.auth == WIFI_AUTH_WEP) {
            result += ", review security";
        }
        return result;
    }

    void RenderHeader() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("WiFi Research");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void Render() {
        if (!HasDisplay()) {
            return;
        }
        RenderHeader();
        if (scanning_) {
            M5.Display.println("Scanning...");
            return;
        }
        if (networks_.empty()) {
            M5.Display.println("Press A to scan.");
        } else {
            const size_t max_rows = 4;
            size_t start = 0;
            if (selected_index_ >= max_rows) {
                start = selected_index_ - max_rows + 1;
            }
            for (size_t i = 0; i < max_rows && (start + i) < networks_.size(); ++i) {
                const auto& record = networks_[start + i];
                if (start + i == selected_index_) {
                    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                } else {
                    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
                }
                M5.Display.printf("%s (%ddBm)\n", record.ssid.c_str(), record.rssi);
                M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
                M5.Display.println("  " + record.posture);
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }
            M5.Display.println("------------------");
            const auto& selected = networks_[selected_index_];
            M5.Display.printf("BSSID: %s\n", selected.bssid.c_str());
            M5.Display.printf("Auth: %d\n", static_cast<int>(selected.auth));
        }
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.println("A:Scan B:Next C:Save");
        if (auto_scan_) {
            M5.Display.println("Auto-scan enabled");
        }
    }

    void ShowStatus(const String& text, uint16_t color) {
        BRUCE_LOG_INFO(String("WiFi module: ") + text);
        if (!HasDisplay()) {
            return;
        }
        info_message_active_ = true;
        info_message_until_ = millis() + 2000;
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", text.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void RenderUnavailable(const char* message) {
        if (!HasDisplay()) {
            return;
        }
        RenderHeader();
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println(message);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void SaveInventory() {
        if (!sd_ready_) {
            return;
        }
        char timestamp[20];
        snprintf(timestamp, sizeof(timestamp), "%lu", millis());
        String path = String("/logs/wifi_inventory_") + timestamp + ".csv";
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            ShowStatus("Log write failed", TFT_RED);
            return;
        }
        file.println("SSID,BSSID,RSSI,Channel,Auth,Notes");
        for (const auto& record : networks_) {
            file.print('"');
            file.print(record.ssid);
            file.print('"');
            file.print(',');
            file.print(record.bssid);
            file.print(',');
            file.print(record.rssi);
            file.print(',');
            file.print(record.channel);
            file.print(',');
            file.print(static_cast<int>(record.auth));
            file.print(',');
            file.println(record.posture);
            yield();
        }
        file.close();
        ShowStatus("Inventory saved", TFT_GREEN);
    }

    void LoadConfig() {
        auto_scan_ = false;
        selected_index_ = 0;
        if (!sd_ready_ && !PrepareSd()) {
            return;
        }
        File file = SD.open("/config/offensive_suite.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("auto_scan=")) {
                auto_scan_ = line.endsWith("1");
            } else if (line.startsWith("last_index=")) {
                selected_index_ = line.substring(String("last_index=").length()).toInt();
            }
        }
        file.close();
    }

    void SaveConfig() {
        File file = SD.open("/config/offensive_suite.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist WiFi config");
            return;
        }
        file.print("auto_scan=");
        file.println(auto_scan_ ? "1" : "0");
        file.print("last_index=");
        file.println(static_cast<int>(selected_index_));
        file.close();
        config_dirty_ = false;
    }

    bool wifi_ready_ = false;
    bool sd_ready_ = false;
    bool scanning_ = false;
    bool auto_scan_ = false;
    bool config_dirty_ = false;
    std::vector<NetworkRecord> networks_;
    size_t selected_index_ = 0;
    bool info_message_active_ = false;
    unsigned long info_message_until_ = 0;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
    unsigned long last_long_press_b_ = 0;
};

}  // namespace bruce::modules
