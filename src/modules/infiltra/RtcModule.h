#pragma once

#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>

#include <ctime>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

class RtcModule : public Module {
   public:
    const char* Name() const override { return "RTC"; }

   protected:
    void Init() override {
        PrepareDisplay();
        if (!ENABLE_INFILTRA_RTC || !HasRtc()) {
            RenderUnavailable("RTC unavailable");
            available_ = false;
            return;
        }
        available_ = true;
        sd_ready_ = SD.begin(kSdCsPin);
        auto probe = M5.Rtc.getDateTime();
        if (probe.year == 0) {
            RenderUnavailable("RTC offline");
            available_ = false;
            return;
        }
        LoadConfig();
        ReadRtc();
        Render();
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            SyncNtp();
        }
        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            AdjustMinutes(1);
        }
        if (DebouncedLongPress(M5.BtnB, 1200, last_long_press_b_)) {
            AdjustHours(1);
        }
        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            SaveRtc();
            ShowStatus("RTC saved", TFT_GREEN);
        }

        if (millis() - last_render_ms_ > 1000) {
            ReadRtc();
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
        M5.Display.println("RTC");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", &current_time_);
        M5.Display.println(buffer);
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &current_time_);
        M5.Display.println(buffer);
        M5.Display.printf("UTC%+d\n", timezone_offset_);
        M5.Display.println("A:NTP B:+min C:Save");
        last_render_ms_ = millis();
    }

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("RTC");
        M5.Display.println(reason);
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("RTC: ") + message);
        if (!HasDisplay()) {
            return;
        }
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", message.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void ReadRtc() {
        auto dt = M5.Rtc.getDateTime();
        current_time_.tm_year = dt.year - 1900;
        current_time_.tm_mon = dt.month - 1;
        current_time_.tm_mday = dt.day;
        current_time_.tm_hour = dt.hour;
        current_time_.tm_min = dt.minute;
        current_time_.tm_sec = dt.second;
    }

    void WriteRtc() {
        m5::rtc_datetime_t dt{};
        dt.year = current_time_.tm_year + 1900;
        dt.month = current_time_.tm_mon + 1;
        dt.day = current_time_.tm_mday;
        dt.hour = current_time_.tm_hour;
        dt.minute = current_time_.tm_min;
        dt.second = current_time_.tm_sec;
        M5.Rtc.setDateTime(dt);
    }

    void AdjustMinutes(int delta) {
        time_t time_value = mktime(&current_time_);
        time_value += delta * 60;
        localtime_r(&time_value, &current_time_);
        WriteRtc();
        Render();
    }

    void AdjustHours(int delta) {
        time_t time_value = mktime(&current_time_);
        time_value += delta * 3600;
        localtime_r(&time_value, &current_time_);
        WriteRtc();
        Render();
    }

    void SyncNtp() {
        if (WiFi.status() != WL_CONNECTED) {
            ShowStatus("WiFi not connected", TFT_RED);
            return;
        }
        ShowStatus("Syncing NTP", TFT_YELLOW);
        configTime(timezone_offset_ * 3600, 0, DEFAULT_NTP_SERVER, "time.nist.gov");
        struct tm ntp_time;
        if (!getLocalTime(&ntp_time, 5000)) {
            ShowStatus("NTP failed", TFT_RED);
            return;
        }
        current_time_ = ntp_time;
        WriteRtc();
        ShowStatus("Time synced", TFT_GREEN);
        Render();
    }

    void SaveRtc() {
        WriteRtc();
    }

    void LoadConfig() {
        if (!sd_ready_) {
            return;
        }
        File file = SD.open("/config/rtc.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("timezone=")) {
                timezone_offset_ = line.substring(String("timezone=").length()).toInt();
            }
        }
        file.close();
    }

    void SaveConfig() {
        if (!sd_ready_) {
            return;
        }
        File file = SD.open("/config/rtc.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist RTC config");
            return;
        }
        file.print("timezone=");
        file.println(timezone_offset_);
        file.close();
    }

    bool available_ = false;
    bool sd_ready_ = false;
    int timezone_offset_ = 0;
    struct tm current_time_ {};
    unsigned long last_render_ms_ = 0;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
    unsigned long last_long_press_b_ = 0;
};

}  // namespace bruce::modules::infiltra
