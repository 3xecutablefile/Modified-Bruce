#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "core/Logger.h"
#include "modules/Module.h"

namespace bruce {

class MenuSystem {
   public:
    using ModuleFactory = std::function<std::shared_ptr<Module>()>;

    void RegisterModule(ModuleFactory factory) {
        if (!factory) {
            BRUCE_LOG_WARN("Attempted to register null module factory");
            return;
        }
        auto instance = factory();
        if (!instance) {
            BRUCE_LOG_WARN("Module factory produced null instance");
            return;
        }
        MenuEntry entry;
        entry.label = instance->Name();
        entry.factory = std::move(factory);
        entries_.push_back(std::move(entry));
        instance.reset();
        ++module_count_;
        BRUCE_LOG_INFO(String("Registered module: ") + entries_.back().label.c_str());
        BuildFilteredIndices();
    }

    void Begin() {
        cursor_ = 0;
        EnsureAboutEntry();
        BuildFilteredIndices();
        Render();
    }

    void Loop() {
        if (entries_.empty()) {
            return;
        }

        M5.update();
        if (active_module_) {
            active_module_->Loop();
            if (M5.BtnPWR.wasPressed()) {
                NotifyActivity();
                active_module_->Exit();
                active_module_.reset();
                Render();
            }
            return;
        }

        HandleMenuInput();

        if (!filter_mode_ && system_message_until_ != 0 && millis() > system_message_until_) {
            system_message_until_ = 0;
            Render();
        }
    }

    void SetActivityCallback(std::function<void()> cb) {
        activity_callback_ = std::move(cb);
        Module::SetActivityCallback(activity_callback_);
    }

    bool IsModuleActive() const { return static_cast<bool>(active_module_); }

    void ShowSystemMessage(const String& message, uint16_t color = TFT_YELLOW) {
        system_message_ = message;
        system_message_color_ = color;
        system_message_until_ = millis() + 2000;
        Render();
    }

    size_t ModuleCount() const { return module_count_; }

   private:
    struct MenuEntry {
        std::string label;
        ModuleFactory factory;
        bool favorite = false;
        bool is_about = false;
    };

    void ActivateCurrentEntry() {
        if (filtered_indices_.empty()) {
            return;
        }
        const auto& entry = entries_[filtered_indices_[cursor_]];
        if (!entry.factory) {
            BRUCE_LOG_WARN("Missing module factory for entry");
            return;
        }
        active_module_ = entry.factory();
        if (!active_module_) {
            BRUCE_LOG_ERROR("Module factory returned null instance");
            return;
        }
        BRUCE_LOG_INFO(String("Launching module: ") + active_module_->Name());
        active_module_->Run();
    }

    void Render() {
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextSize(2);
        M5.Display.println("Bruce+Infiltra");
        M5.Display.setTextSize(1);
        if (!filter_text_.empty()) {
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.printf("Filter: %s\n", filter_text_.c_str());
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        if (filtered_indices_.empty()) {
            M5.Display.println("No modules");
            return;
        }
        size_t max_rows = 6;
        size_t start = 0;
        if (cursor_ >= max_rows) {
            start = cursor_ - max_rows + 1;
        }
        for (size_t i = 0; i < max_rows && (start + i) < filtered_indices_.size(); ++i) {
            const auto index = filtered_indices_[start + i];
            const auto& entry = entries_[index];
            if (start + i == cursor_) {
                M5.Display.print("> ");
            } else {
                M5.Display.print("  ");
            }
            M5.Display.print(entry.favorite ? "*" : " ");
            M5.Display.println(entry.label.c_str());
        }
        M5.Display.println();
        M5.Display.println("A:Select B:Next");
        M5.Display.println("C:Fav hold:Filter");
        M5.Display.printf("Modules:%u Version:%s\n", static_cast<unsigned>(module_count_), VERSION);
        M5.Display.printf("Heap:%uB\n", static_cast<unsigned>(ESP.getFreeHeap()));
        if (system_message_until_ > millis()) {
            M5.Display.setTextColor(system_message_color_, TFT_BLACK);
            M5.Display.printf("%s\n", system_message_.c_str());
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        }
    }

    void HandleMenuInput() {
        if (filter_mode_) {
            HandleFilterInput();
            return;
        }

        if (DebouncedPress(M5.BtnB, last_menu_press_b_)) {
            if (!filtered_indices_.empty()) {
                cursor_ = (cursor_ + 1) % filtered_indices_.size();
                Render();
            }
        }
        if (DebouncedPress(M5.BtnA, last_menu_press_a_)) {
            ActivateCurrentEntry();
            return;
        }
        if (DebouncedLongPress(M5.BtnC, 800, last_menu_long_press_c_)) {
            filter_mode_ = true;
            filter_char_index_ = 0;
            RenderFilterPrompt();
            return;
        }
        if (DebouncedPress(M5.BtnC, last_menu_press_c_)) {
            ToggleFavorite();
        }
    }

    void HandleFilterInput() {
        if (DebouncedPress(M5.BtnB, last_menu_press_b_)) {
            filter_char_index_ = (filter_char_index_ + 1) % filter_chars_.size();
            RenderFilterPrompt();
        }
        if (DebouncedPress(M5.BtnA, last_menu_press_a_)) {
            char ch = filter_chars_[filter_char_index_];
            if (ch == '<') {
                if (!filter_text_.empty()) {
                    filter_text_.pop_back();
                }
            } else if (ch == '#') {
                filter_text_.clear();
            } else {
                filter_text_.push_back(ch);
            }
            BuildFilteredIndices();
            RenderFilterPrompt();
        }
        if (DebouncedPress(M5.BtnC, last_menu_press_c_)) {
            filter_mode_ = false;
            BuildFilteredIndices();
            Render();
        }
    }

    void RenderFilterPrompt() {
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("Filter Modules");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.printf("Current: %s\n", filter_text_.c_str());
        M5.Display.printf("Char: %c\n", filter_chars_[filter_char_index_]);
        M5.Display.println("A:Add B:Next C:Done");
        M5.Display.println("< back # clear");
    }

    void ToggleFavorite() {
        if (filtered_indices_.empty()) {
            return;
        }
        auto& entry = entries_[filtered_indices_[cursor_]];
        entry.favorite = !entry.favorite;
        BuildFilteredIndices();
        Render();
    }

    void EnsureAboutEntry() {
        auto it = std::find_if(entries_.begin(), entries_.end(), [](const MenuEntry& e) { return e.is_about; });
        if (it != entries_.end()) {
            return;
        }
        MenuEntry about;
        about.label = "About";
        about.is_about = true;
        about.factory = []() {
            class AboutModule : public Module {
               public:
                const char* Name() const override { return "About"; }

               protected:
                void Init() override {
                    if (!HasDisplay()) {
                        return;
                    }
                    M5.Display.clear();
                    M5.Display.setCursor(0, 0);
                    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
                    M5.Display.println("About");
                    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
                    M5.Display.printf("Version: %s\n", VERSION);
                    M5.Display.printf("Device: %s\n", DEVICE_NAME);
                    M5.Display.println("Bruce + Infiltra Hybrid");
                    M5.Display.println("Authorized research only");
                    M5.Display.println("PWR to exit");
                }

                void Update() override {
                    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
                        NotifyActivity();
                    }
                }

                void Cleanup() override {
                    if (HasDisplay()) {
                        M5.Display.clear();
                    }
                }
            };
            return std::make_shared<AboutModule>();
        };
        entries_.push_back(std::move(about));
    }

    void BuildFilteredIndices() {
        filtered_indices_.clear();
        std::vector<size_t> favored;
        std::vector<size_t> others;
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!MatchesFilter(entries_[i])) {
                continue;
            }
            if (entries_[i].favorite) {
                favored.push_back(i);
            } else {
                others.push_back(i);
            }
        }
        filtered_indices_.insert(filtered_indices_.end(), favored.begin(), favored.end());
        filtered_indices_.insert(filtered_indices_.end(), others.begin(), others.end());
        if (cursor_ >= filtered_indices_.size()) {
            cursor_ = filtered_indices_.empty() ? 0 : filtered_indices_.size() - 1;
        }
    }

    bool MatchesFilter(const MenuEntry& entry) const {
        if (filter_text_.empty()) {
            return true;
        }
        std::string label_lower = entry.label;
        std::string filter_lower = filter_text_;
        std::transform(label_lower.begin(), label_lower.end(), label_lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return label_lower.find(filter_lower) != std::string::npos;
    }

    void NotifyActivity() {
        if (activity_callback_) {
            activity_callback_();
        }
    }

    bool DebouncedPress(Button_Class& button, unsigned long& last_time_ms, uint32_t interval_ms = 50) {
        if (!button.wasPressed()) {
            return false;
        }
        auto now = millis();
        if (now - last_time_ms < interval_ms) {
            return false;
        }
        last_time_ms = now;
        NotifyActivity();
        return true;
    }

    bool DebouncedLongPress(Button_Class& button, unsigned long duration_ms, unsigned long& last_time_ms,
                            uint32_t interval_ms = 50) {
        if (!button.pressedFor(duration_ms)) {
            return false;
        }
        auto now = millis();
        if (now - last_time_ms < interval_ms) {
            return false;
        }
        last_time_ms = now;
        NotifyActivity();
        return true;
    }

    std::vector<MenuEntry> entries_;
    std::vector<size_t> filtered_indices_;
    std::shared_ptr<Module> active_module_;
    size_t cursor_ = 0;
    size_t module_count_ = 0;
    std::string filter_text_;
    bool filter_mode_ = false;
    size_t filter_char_index_ = 0;
    const std::string filter_chars_ = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789< #";
    unsigned long system_message_until_ = 0;
    uint16_t system_message_color_ = TFT_YELLOW;
    String system_message_;
    std::function<void()> activity_callback_;
    unsigned long last_menu_press_a_ = 0;
    unsigned long last_menu_press_b_ = 0;
    unsigned long last_menu_press_c_ = 0;
    unsigned long last_menu_long_press_c_ = 0;
};

}  // namespace bruce
