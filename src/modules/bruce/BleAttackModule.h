#pragma once

#include <M5Unified.h>
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include <SD.h>
#include <esp_bt_main.h>
#include <esp_bt.h>

#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules {

class BleAttackModule : public Module {
   public:
    const char* Name() const override { return "BLE Research"; }

   protected:
    void Init() override {
        EnsureDisplay();
        EnsureLogDirectories();
        LoadConfig();

        ble_available_ = PrepareBle();
        sd_ready_ = PrepareSd();

        if (!ble_available_) {
            RenderUnavailable("BLE controller offline");
            return;
        }

        RenderHeader();
        ShowStatus("Ready: A scan, B next, C save", TFT_GREEN);

        if (auto_scan_) {
            StartScan();
        } else {
            Render();
        }
    }

    void Update() override {
        if (!ble_available_) {
            return;
        }

        if (scanning_ && scanner_ && !scanner_->isScanning()) {
            CompleteScan();
        }

        if (info_message_active_ && millis() > info_message_until_) {
            info_message_active_ = false;
            Render();
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            StartScan();
        }

        if (!devices_.empty() && DebouncedPress(M5.BtnB, last_press_b_)) {
            selected_index_ = (selected_index_ + 1) % devices_.size();
            Render();
        }

        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            if (sd_ready_) {
                SaveLog();
            } else if (PrepareSd()) {
                SaveLog();
            } else {
                ShowStatus("SD missing", TFT_RED);
            }
        }

        if (DebouncedLongPress(M5.BtnB, 1200, last_long_press_b_)) {
            auto_scan_ = !auto_scan_;
            config_dirty_ = true;
            ShowStatus(auto_scan_ ? "Auto-scan enabled" : "Auto-scan disabled", TFT_YELLOW);
        }
    }

    void Cleanup() override {
        if (scanning_ && scanner_) {
            scanner_->stop();
        }
        scanning_ = false;
        devices_.clear();

        if (ble_initialized_here_) {
            NimBLEDevice::deinit(true);
            ble_initialized_here_ = false;
        }

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
    struct DeviceRecord {
        String address;
        String name;
        int rssi = 0;
        bool connectable = false;
        bool private_address = false;
        bool has_manufacturer = false;
        bool has_services = false;
        String posture;
    };

    void EnsureDisplay() {
        if (!HasDisplay()) {
            BRUCE_LOG_WARN("Display unavailable; BLE module running headless");
        } else {
            M5.Display.clear();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.setTextSize(1);
        }
    }

    void EnsureLogDirectories() {
        if (!PrepareSd()) {
            return;
        }
        if (!SD.exists("/config")) {
            SD.mkdir("/config");
        }
        if (!SD.exists("/logs")) {
            SD.mkdir("/logs");
        }
    }

    bool PrepareSd() {
        if (sd_ready_) {
            return true;
        }
        if (SD.begin(kSdCsPin)) {
            sd_ready_ = true;
            return true;
        }
        return false;
    }

    bool PrepareBle() {
        if (ble_available_) {
            return true;
        }
        esp_bt_controller_status_t status = esp_bt_controller_get_status();
        BRUCE_LOG_INFO(String("BLE controller status: ") + status);
        try {
            if (!NimBLEDevice::getInitialized()) {
                NimBLEDevice::init("BruceHybridBLE");
                ble_initialized_here_ = true;
            }
            NimBLEDevice::setPower(ESP_PWR_LVL_P7);
            scanner_ = NimBLEDevice::getScan();
            scanner_->setActiveScan(true);
            scanner_->setInterval(45);
            scanner_->setWindow(30);
            scanner_->setDuplicateFilter(true);
        } catch (...) {
            BRUCE_LOG_ERROR("Failed to initialize NimBLE stack");
            ShowStatus("BLE init failed", TFT_RED);
            return false;
        }
        return true;
    }

    void StartScan() {
        if (!scanner_) {
            return;
        }
        if (scanning_) {
            scanner_->stop();
            scanning_ = false;
        }
        devices_.clear();
        selected_index_ = 0;
        RenderHeader();
        M5.Display.println("Scanning for BLE devices...");
        ShowStatus("Scanning...", TFT_YELLOW);
        if (!scanner_->start(5, false)) {
            ShowStatus("Scan start failed", TFT_RED);
            return;
        }
        scanning_ = true;
        scan_start_ = millis();
        BRUCE_LOG_INFO("BLE scan started");
    }

    void CompleteScan() {
        if (!scanner_) {
            return;
        }
        NimBLEScanResults results = scanner_->getResults();
        devices_.clear();
        devices_.reserve(results.getCount());
        for (int i = 0; i < results.getCount(); ++i) {
            NimBLEAdvertisedDevice device = results.getDevice(i);
            DeviceRecord record;
            record.address = device.getAddress().toString().c_str();
            record.name = device.getName().c_str();
            if (record.name.isEmpty()) {
                record.name = "(unknown)";
            }
            record.rssi = device.getRSSI();
            record.connectable = device.isConnectable();
            record.private_address = device.getAddressType() == BLE_ADDR_RANDOM;
            record.has_manufacturer = device.haveManufacturerData();
            record.has_services = device.haveServiceUUID();
            record.posture = AnalyzeDevice(record, device);
            devices_.push_back(record);
            if ((i % 4) == 0) {
                yield();
            }
        }
        scanner_->clearResults();
        scanning_ = false;
        ShowStatus(String("Scan complete: ") + devices_.size() + " devices", TFT_GREEN);
        Render();
    }

    String AnalyzeDevice(const DeviceRecord& record, const NimBLEAdvertisedDevice& device) {
        String assessment;
        assessment += record.connectable ? "Connectable" : "Broadcast only";
        assessment += record.private_address ? ", private MAC" : ", public MAC";
        assessment += record.has_manufacturer ? ", vendor data" : ", no vendor data";
        assessment += record.has_services ? ", UUIDs present" : ", limited UUIDs";
        if (device.getServiceDataCount() > 0 || device.haveServiceData()) {
            assessment += ", data insight";
        } else {
            assessment += ", metadata only";
        }
        return assessment;
    }

    void RenderHeader() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("BLE Research");
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
        if (devices_.empty()) {
            M5.Display.println("No devices. Press A to scan.");
        } else {
            const size_t max_rows = 6;
            size_t start = 0;
            if (selected_index_ >= max_rows) {
                start = selected_index_ - max_rows + 1;
            }
            for (size_t i = 0; i < max_rows && (start + i) < devices_.size(); ++i) {
                const auto& record = devices_[start + i];
                if (start + i == selected_index_) {
                    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                } else {
                    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
                }
                String line = record.name + " (" + record.rssi + "dBm)";
                M5.Display.println(line);  // line 1
                M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
                M5.Display.println("  " + record.posture);
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }
        }
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("------------------");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.println("A:Scan B:Next C:Save");
        if (auto_scan_) {
            M5.Display.println("Auto-scan ON");
        } else {
            M5.Display.println("Hold B: toggle auto");
        }
    }

    void RenderUnavailable(const String& reason) {
        if (!HasDisplay()) {
            return;
        }
        RenderHeader();
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println(reason);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("BLE module: ") + message);
        if (!HasDisplay()) {
            return;
        }
        info_message_active_ = true;
        info_message_until_ = millis() + 2000;
        info_message_ = message;
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", message.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    void SaveLog() {
        if (!sd_ready_) {
            return;
        }
        char timestamp[20];
        snprintf(timestamp, sizeof(timestamp), "%lu", millis());
        String path = String("/logs/ble_research_") + timestamp + ".log";
        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            ShowStatus("Log open failed", TFT_RED);
            return;
        }
        file.println("# BLE inventory log");
        for (const auto& record : devices_) {
            file.print(record.address);
            file.print(",");
            file.print(record.name);
            file.print(",");
            file.print(record.rssi);
            file.print(",");
            file.println(record.posture);
        }
        file.close();
        ShowStatus("Log saved", TFT_GREEN);
    }

    void LoadConfig() {
        auto_scan_ = false;
        if (!PrepareSd()) {
            return;
        }
        File file = SD.open("/config/ble_attack.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("auto_scan=")) {
                auto_scan_ = line.endsWith("1");
            }
        }
        file.close();
    }

    void SaveConfig() {
        File file = SD.open("/config/ble_attack.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist BLE config");
            return;
        }
        file.print("auto_scan=");
        file.println(auto_scan_ ? "1" : "0");
        file.close();
        config_dirty_ = false;
    }

    bool ble_available_ = false;
    bool ble_initialized_here_ = false;
    bool sd_ready_ = false;
    bool scanning_ = false;
    bool auto_scan_ = false;
    bool config_dirty_ = false;
    NimBLEScan* scanner_ = nullptr;
    unsigned long scan_start_ = 0;
    std::vector<DeviceRecord> devices_;
    size_t selected_index_ = 0;
    bool info_message_active_ = false;
    unsigned long info_message_until_ = 0;
    String info_message_;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
    unsigned long last_long_press_b_ = 0;
};

}  // namespace bruce::modules
