#pragma once

#include <M5Unified.h>
#include <SD.h>

#include <vector>

#include <IRremoteESP8266.h>
#include <IRsend.h>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

struct IrSignal {
    String category;
    String name;
    decode_type_t protocol;
    uint64_t code = 0;
    uint16_t bits = 0;
};

class IrModule : public Module {
   public:
    IrModule() : transmitter_(kIrLedPin) {}

    const char* Name() const override { return "IR Control"; }

   protected:
    void Init() override {
        PrepareDisplay();
        if (!ENABLE_INFILTRA_IR_MODULE || !HasIRTransceiver()) {
            RenderUnavailable("IR hardware missing");
            available_ = false;
            return;
        }
        available_ = true;
        sd_ready_ = PrepareSd();
        transmitter_.begin();
        LoadDatabase();
        LoadConfig();
        Render();
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            if (!signals_.empty()) {
                if (selected_index_ == 0) {
                    selected_index_ = signals_.size() - 1;
                } else {
                    --selected_index_;
                }
                Render();
            }
        }

        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            if (!signals_.empty()) {
                selected_index_ = (selected_index_ + 1) % signals_.size();
                Render();
            }
        }

        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            if (!signals_.empty()) {
                SendCurrent();
            }
        }
    }

    void Cleanup() override {
        if (available_) {
            SaveConfig();
        }
        signals_.clear();
        if (sd_ready_) {
            SD.end();
            sd_ready_ = false;
        }
        if (HasDisplay()) {
            M5.Display.clear();
        }
    }

   private:
    bool PrepareSd() {
        if (sd_ready_) {
            return true;
        }
        if (SD.begin(kSdCsPin)) {
            if (!SD.exists("/config")) {
                SD.mkdir("/config");
            }
            sd_ready_ = true;
            return true;
        }
        return false;
    }

    void PrepareDisplay() {
        if (HasDisplay()) {
            M5.Display.clear();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.setTextSize(1);
        }
    }

    void LoadDatabase() {
        signals_.clear();
        if (!sd_ready_ && !PrepareSd()) {
            SeedDefaults();
            return;
        }
        if (!SD.exists("/ir_db.json")) {
            SeedDefaults();
            return;
        }
        File file = SD.open("/ir_db.json", FILE_READ);
        if (!file) {
            SeedDefaults();
            return;
        }
        String content = file.readString();
        file.close();
        int pos = 0;
        while (true) {
            int name_pos = content.indexOf(""name"", pos);
            if (name_pos == -1) {
                break;
            }
            int name_start = content.indexOf('"', name_pos + 6);
            int name_end = content.indexOf('"', name_start + 1);
            if (name_start == -1 || name_end == -1) {
                break;
            }
            String name = content.substring(name_start + 1, name_end);

            int cat_pos = content.indexOf(""category"", name_end);
            String category = "General";
            if (cat_pos != -1) {
                int cat_start = content.indexOf('"', cat_pos + 10);
                int cat_end = content.indexOf('"', cat_start + 1);
                if (cat_start != -1 && cat_end != -1) {
                    category = content.substring(cat_start + 1, cat_end);
                }
            }

            int protocol_pos = content.indexOf(""protocol"", name_end);
            if (protocol_pos == -1) {
                break;
            }
            int protocol_start = content.indexOf('"', protocol_pos + 10);
            int protocol_end = content.indexOf('"', protocol_start + 1);
            if (protocol_start == -1 || protocol_end == -1) {
                break;
            }
            String protocol_str = content.substring(protocol_start + 1, protocol_end);

            int code_pos = content.indexOf(""code"", protocol_end);
            if (code_pos == -1) {
                break;
            }
            int code_start = content.indexOf('"', code_pos + 6);
            int code_end = content.indexOf('"', code_start + 1);
            if (code_start == -1 || code_end == -1) {
                break;
            }
            String code_str = content.substring(code_start + 1, code_end);

            int bits_pos = content.indexOf(""bits"", code_end);
            if (bits_pos == -1) {
                break;
            }
            int bits_start = content.indexOf(':', bits_pos + 6);
            int bits_end = content.indexOf(',', bits_start + 1);
            if (bits_end == -1) {
                bits_end = content.indexOf('}', bits_start + 1);
            }
            if (bits_start == -1 || bits_end == -1) {
                break;
            }
            uint16_t bits = content.substring(bits_start + 1, bits_end).toInt();

            IrSignal signal;
            signal.name = name;
            signal.category = category;
            signal.protocol = ParseProtocol(protocol_str);
            signal.code = strtoull(code_str.c_str(), nullptr, 0);
            signal.bits = bits;
            signals_.push_back(signal);
            pos = bits_end;
            if ((signals_.size() % 4) == 0) {
                yield();
            }
        }
        if (signals_.empty()) {
            SeedDefaults();
        }
    }

    decode_type_t ParseProtocol(const String& text) {
        if (text.equalsIgnoreCase("NEC")) {
            return decode_type_t::NEC;
        }
        if (text.equalsIgnoreCase("SONY")) {
            return decode_type_t::SONY;
        }
        if (text.equalsIgnoreCase("RC5")) {
            return decode_type_t::RC5;
        }
        if (text.equalsIgnoreCase("RC6")) {
            return decode_type_t::RC6;
        }
        return decode_type_t::UNKNOWN;
    }

    void SeedDefaults() {
        signals_.push_back({"Media", "TV Power", decode_type_t::NEC, 0x20DF10EF, 32});
        signals_.push_back({"Media", "Soundbar Vol+", decode_type_t::NEC, 0x807F40BF, 32});
        signals_.push_back({"Climate", "AC Toggle", decode_type_t::SONY, 0xA90, 12});
        signals_.push_back({"Projector", "Projector On", decode_type_t::RC5, 0x1FE48, 13});
    }

    void LoadConfig() {
        selected_index_ = 0;
        if (!sd_ready_ && !PrepareSd()) {
            return;
        }
        File file = SD.open("/config/ir_module.cfg", FILE_READ);
        if (!file) {
            return;
        }
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.startsWith("last_index=")) {
                selected_index_ = line.substring(String("last_index=").length()).toInt();
            }
        }
        file.close();
        if (selected_index_ >= signals_.size()) {
            selected_index_ = 0;
        }
    }

    void SaveConfig() {
        if (!sd_ready_ && !PrepareSd()) {
            return;
        }
        File file = SD.open("/config/ir_module.cfg", FILE_WRITE);
        if (!file) {
            BRUCE_LOG_WARN("Failed to persist IR config");
            return;
        }
        file.print("last_index=");
        file.println(static_cast<int>(selected_index_));
        file.close();
    }

    void SendCurrent() {
        if (selected_index_ >= signals_.size()) {
            return;
        }
        const auto& signal = signals_[selected_index_];
        BRUCE_LOG_INFO(String("Transmitting IR ") + signal.name);
        transmitter_.send(signal.protocol, signal.code, signal.bits);
        if (HasDisplay()) {
            M5.Display.setCursor(0, 90);
            M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
            M5.Display.printf("Sent %s
", signal.name.c_str());
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        }
    }

    void Render() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("IR Control");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        if (signals_.empty()) {
            M5.Display.println("No IR codes loaded");
            return;
        }
        const size_t max_rows = 4;
        size_t start = 0;
        if (selected_index_ >= max_rows) {
            start = selected_index_ - max_rows + 1;
        }
        for (size_t i = 0; i < max_rows && (start + i) < signals_.size(); ++i) {
            const auto& signal = signals_[start + i];
            if (start + i == selected_index_) {
                M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
            } else {
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }
            M5.Display.printf("%s - %s
", signal.category.c_str(), signal.name.c_str());
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.printf("  %s 0x%llX/%db
", ProtocolName(signal.protocol).c_str(), signal.code, signal.bits);
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        M5.Display.println("------------------");
        M5.Display.println("A:Prev B:Next C:Send");
    }

    String ProtocolName(decode_type_t protocol) const {
        switch (protocol) {
            case decode_type_t::NEC:
                return "NEC";
            case decode_type_t::SONY:
                return "SONY";
            case decode_type_t::RC5:
                return "RC5";
            case decode_type_t::RC6:
                return "RC6";
            default:
                return "UNKNOWN";
        }
    }

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("IR Control");
        M5.Display.println(reason);
    }

    static constexpr uint8_t kIrLedPin = 9;

    IRsend transmitter_;
    bool available_ = false;
    bool sd_ready_ = false;
    size_t selected_index_ = 0;
    std::vector<IrSignal> signals_;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
};

}  // namespace bruce::modules::infiltra
