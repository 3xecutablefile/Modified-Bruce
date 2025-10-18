#pragma once

#include <M5Unified.h>
#include <SD.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

class BrightnessBatteryModule : public Module {
   public:
    const char* Name() const override { return "Device Control"; }

   protected:
    void Init() override {
        PrepareDisplay();
        if (!ENABLE_INFILTRA_DEVICE_CONTROLS) {
            RenderUnavailable("Module disabled");
            available_ = false;
            return;
        }
        available_ = true;
        sd_ready_ = SD.begin(kSdCsPin);
        power_ready_ = M5.Power.begin();
        if (!power_ready_) {
            BRUCE_LOG_WARN("Power subsystem init failed");
        }
        LoadConfig();
        ApplyBrightness();
        M5.Power.setPowerBoostKeepOn(!allow_sleep_);
        Render();
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            brightness_ = std::max(0, brightness_ - 10);
            ApplyBrightness();
            ShowStatus("Brightness-", TFT_YELLOW);
            Render();
        }
        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            brightness_ = std::min(100, brightness_ + 10);
            ApplyBrightness();
            ShowStatus("Brightness+", TFT_YELLOW);
            Render();
        }
        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            allow_sleep_ = !allow_sleep_;
            M5.Power.setPowerBoostKeepOn(!allow_sleep_);
            ShowStatus(allow_sleep_ ? "Sleep allowed" : "Boost on", TFT_GREEN);
            Render();
        }

        UpdateMetrics();
        if (millis() - last_render_ > 1500) {
            Render();
        }
    }

    void Cleanup() override {
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
    void PrepareDisplay() {
        if (HasDisplay()) {
            M5.Display.clear();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.setTextSize(1);
        }
    }

    void Render() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("Device Control");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.printf("Brightness: %d%%\n", brightness_);
        M5.Display.printf("Battery: %.2fV (%d%%)\n", voltage_, battery_percent_);
        M5.Display.printf("Charging: %s\n", charging_ ? "Yes" : "No");
        M5.Display.printf("Sleep: %s\n", allow_sleep_ ? "Enabled" : "Disabled");
        DrawPowerGraph();
        M5.Display.println("A:- B:+ C:Sleep");
        last_render_ = millis();
    }

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("Device Control");
        M5.Display.println(reason);
    }

    void ApplyBrightness() {
        int level = map(brightness_, 0, 100, 16, 255);
        M5.Display.setBrightness(level);
    }

    void UpdateMetrics() {
        float current = 0.0f;
        if (power_ready_) {
            voltage_ = M5.Power.getBatteryVoltage() / 1000.0f;
            battery_percent_ = M5.Power.getBatteryLevel();
            charging_ = M5.Power.isCharging();
            current = M5.Power.getBatteryCurrent() / 1000.0f;
        } else {
            voltage_ = 0.0f;
            battery_percent_ = 0;
            charging_ = false;
        }
        if (current_history_.size() >= 20) {
            current_history_.erase(current_history_.begin());
        }
        current_history_.push_back(current);
    }

    void DrawPowerGraph() {
        if (!HasDisplay()) {
            return;
        }
        int origin_x = 0;
        int origin_y = 80;
        int width = 60;
        int height = 30;
        M5.Display.drawRect(origin_x, origin_y, width, height, TFT_WHITE);
        if (current_history_.empty()) {
            return;
        }
        float max_current = 0.1f;
        for (float value : current_history_) {
            if (abs(value) > max_current) {
                max_current = abs(value);
            }
        }
        int count = current_history_.size();
        for (int i = 0; i < count; ++i) {
            float value = current_history_[i];
            int x = origin_x + (i * width) / count;
            int y = origin_y + height / 2 - static_cast<int>((value / max_current) * (height / 2));
            M5.Display.drawPixel(x, y, value >= 0 ? TFT_GREEN : TFT_RED);
            if ((i % 4) == 0) {
                yield();
            }
        }
        M5.Display.setCursor(origin_x + width + 4, origin_y);
        M5.Display.printf("Current %.2fmA", current_history_.back() * 1000.0f);
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("Device control: ") + message);
        if (!HasDisplay()) {
            return;
        }
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", message.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void LoadConfig() {
        if (!sd_ready_) {
            return;
        }
        File file = SD.open("/config/device_control.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("brightness=")) {
                brightness_ = line.substring(String("brightness=").length()).toInt();
            } else if (line.startsWith("sleep=")) {
                allow_sleep_ = line.endsWith("1");
            }
        }
        file.close();
    }

    void SaveConfig() {
        if (!sd_ready_) {
            return;
        }
        File file = SD.open("/config/device_control.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist device control config");
            return;
        }
        file.print("brightness=");
        file.println(brightness_);
        file.print("sleep=");
        file.println(allow_sleep_ ? "1" : "0");
        file.close();
    }

    bool available_ = false;
    bool sd_ready_ = false;
    bool allow_sleep_ = true;
    int brightness_ = 80;
    float voltage_ = 0.0f;
    int battery_percent_ = 0;
    bool charging_ = false;
    unsigned long last_render_ = 0;
    std::vector<float> current_history_;
    bool power_ready_ = false;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
};

}  // namespace bruce::modules::infiltra
