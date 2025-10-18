#pragma once

#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

struct NetworkInfo {
    String ssid;
    int32_t rssi = 0;
    int32_t channel = 0;
    wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
    String encryption;
};

class WiFiScannerModule : public Module {
   public:
    const char* Name() const override { return "WiFi Scanner"; }

   protected:
    void Init() override {
        PrepareDisplay();
        sd_ready_ = PrepareSd();
        LoadConfig();

        if (!ENABLE_INFILTRA_WIFI_SCANNER || !HasWiFi()) {
            RenderUnavailable("WiFi unavailable");
            available_ = false;
            return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        available_ = true;
        RenderHeader();
        ShowStatus("A:scan B:sort C:save", TFT_GREEN);
        if (auto_hop_) {
            StartScan();
        } else {
            Render();
        }
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (scanning_) {
            int16_t result = WiFi.scanComplete();
            if (result >= 0) {
                CompleteScan(result);
                if (auto_hop_) {
                    StartScan();
                }
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

        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            sort_descending_ = !sort_descending_;
            config_dirty_ = true;
            SortNetworks();
            ShowStatus(sort_descending_ ? "Sorted by RSSI" : "Sorted alphabetically", TFT_YELLOW);
            Render();
        }

        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            if (sd_ready_ || PrepareSd()) {
                SaveResults();
            } else {
                ShowStatus("SD missing", TFT_RED);
            }
        }

        if (DebouncedLongPress(M5.BtnB, 1200, last_long_press_b_)) {
            auto_hop_ = !auto_hop_;
            config_dirty_ = true;
            ShowStatus(auto_hop_ ? "Auto hop ON" : "Auto hop OFF", TFT_YELLOW);
        }
    }

    void Cleanup() override {
        if (scanning_) {
            scanning_ = false;
            WiFi.scanDelete();
        }
        if (sd_ready_) {
            SaveConfig();
            SD.end();
            sd_ready_ = false;
        }
        networks_.clear();
        if (HasDisplay()) {
            M5.Display.clear();
        }
    }

   private:
    void PrepareDisplay() {
        if (HasDisplay()) {
            M5.Display.clear();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.setTextSize(1);
        }
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
        if (!available_) {
            return;
        }
        RenderHeader();
        M5.Display.printf("Scanning channel %d...\n", current_channel_);
        networks_.clear();
        scanning_ = true;
        WiFi.scanDelete();
        esp_wifi_set_channel(current_channel_, WIFI_SECOND_CHAN_NONE);
        WiFi.scanNetworks(true, true);
        ShowStatus(String("Scanning ch ") + current_channel_, TFT_YELLOW);
        current_channel_ = current_channel_ % 13 + 1;
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
            NetworkInfo info;
            info.ssid = WiFi.SSID(i);
            if (info.ssid.isEmpty()) {
                info.ssid = "(hidden)";
            }
            info.rssi = WiFi.RSSI(i);
            info.channel = WiFi.channel(i);
            info.auth = WiFi.encryptionType(i);
            info.encryption = DescribeAuth(info.auth);
            networks_.push_back(info);
            if ((i % 4) == 0) {
                yield();
            }
        }
        WiFi.scanDelete();
        SortNetworks();
        ShowStatus(String(count) + " APs", TFT_GREEN);
        Render();
    }

    void SortNetworks() {
        if (networks_.empty()) {
            return;
        }
        if (sort_descending_) {
            std::sort(networks_.begin(), networks_.end(), [](const NetworkInfo& a, const NetworkInfo& b) {
                return a.rssi > b.rssi;
            });
        } else {
            std::sort(networks_.begin(), networks_.end(), [](const NetworkInfo& a, const NetworkInfo& b) {
                return a.ssid < b.ssid;
            });
        }
    }

    String DescribeAuth(wifi_auth_mode_t mode) const {
        switch (mode) {
            case WIFI_AUTH_OPEN:
                return "OPEN";
            case WIFI_AUTH_WEP:
                return "WEP";
            case WIFI_AUTH_WPA_PSK:
                return "WPA";
            case WIFI_AUTH_WPA2_PSK:
                return "WPA2";
            case WIFI_AUTH_WPA_WPA2_PSK:
                return "WPA/WPA2";
            case WIFI_AUTH_WPA2_ENTERPRISE:
                return "802.1X";
            case WIFI_AUTH_WPA3_PSK:
                return "WPA3";
            case WIFI_AUTH_WPA2_WPA3_PSK:
                return "WPA2/3";
            default:
                return "UNKNOWN";
        }
    }

    void RenderHeader() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("WiFi Scanner");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void Render() {
        if (!HasDisplay()) {
            return;
        }
        RenderHeader();
        if (networks_.empty()) {
            M5.Display.println("Press A to scan");
        } else {
            const size_t max_rows = 5;
            for (size_t i = 0; i < max_rows && i < networks_.size(); ++i) {
                const auto& net = networks_[i];
                M5.Display.printf("%s (%ddBm)\n", net.ssid.c_str(), net.rssi);
                M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
                M5.Display.printf("  ch%02d %s\n", net.channel, net.encryption.c_str());
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }
        }
        M5.Display.println("------------------");
        M5.Display.println("A:Scan B:Sort C:Save");
        if (auto_hop_) {
            M5.Display.println("Auto hop enabled");
        }
    }

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("WiFi Scanner");
        M5.Display.println(reason);
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("WiFi scanner: ") + message);
        if (!HasDisplay()) {
            return;
        }
        info_message_active_ = true;
        info_message_until_ = millis() + 2000;
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", message.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void SaveResults() {
        if (!sd_ready_) {
            return;
        }
        char timestamp[20];
        snprintf(timestamp, sizeof(timestamp), "%lu", millis());
        String path = String("/logs/wifi_scan_") + timestamp + ".json";
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            ShowStatus("Save failed", TFT_RED);
            return;
        }
        file.println("{\"networks\":[");
        for (size_t i = 0; i < networks_.size(); ++i) {
            const auto& net = networks_[i];
            file.print("  {\"ssid\":\"");
            file.print(net.ssid);
            file.print("\",\"rssi\":");
            file.print(net.rssi);
            file.print(",\"channel\":");
            file.print(net.channel);
            file.print(",\"auth\":\"");
            file.print(net.encryption);
            file.print("\"}");
            if (i + 1 < networks_.size()) {
                file.println(",");
            } else {
                file.println();
            }
            if ((i % 4) == 0) {
                yield();
            }
        }
        file.println("]}");
        file.close();
        ShowStatus("Scan saved", TFT_GREEN);
    }

    void LoadConfig() {
        auto_hop_ = true;
        sort_descending_ = true;
        if (!sd_ready_ && !PrepareSd()) {
            return;
        }
        File file = SD.open("/config/wifi_scanner.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("auto_hop=")) {
                auto_hop_ = line.endsWith("1");
            } else if (line.startsWith("sort_desc=")) {
                sort_descending_ = line.endsWith("1");
            }
        }
        file.close();
    }

    void SaveConfig() {
        File file = SD.open("/config/wifi_scanner.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist scanner config");
            return;
        }
        file.print("auto_hop=");
        file.println(auto_hop_ ? "1" : "0");
        file.print("sort_desc=");
        file.println(sort_descending_ ? "1" : "0");
        file.close();
        config_dirty_ = false;
    }

    bool available_ = false;
    bool sd_ready_ = false;
    bool scanning_ = false;
    bool auto_hop_ = true;
    bool sort_descending_ = true;
    bool config_dirty_ = false;
    int current_channel_ = 1;
    bool info_message_active_ = false;
    unsigned long info_message_until_ = 0;
    std::vector<NetworkInfo> networks_;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
    unsigned long last_long_press_b_ = 0;
};

}  // namespace bruce::modules::infiltra
