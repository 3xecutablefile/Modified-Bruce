#pragma once

#include <M5Unified.h>
#include <SD.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

struct FileEntry {
    String name;
    bool is_directory = false;
    uint64_t size = 0;
    time_t modified = 0;
};

class FileExplorerModule : public Module {
   public:
    const char* Name() const override { return "File Explorer"; }

   protected:
    void Init() override {
        PrepareDisplay();
        if (!ENABLE_INFILTRA_FILE_EXPLORER) {
            RenderUnavailable("Module disabled");
            available_ = false;
            return;
        }
        if (!SD.begin(kSdCsPin)) {
            RenderUnavailable("SD card missing");
            available_ = false;
            return;
        }
        available_ = true;
        current_path_ = "/";
        RefreshEntries();
        Render();
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (preview_mode_) {
            bool exit_preview = DebouncedPress(M5.BtnA, last_press_a_) ||
                                DebouncedPress(M5.BtnB, last_press_b_) ||
                                DebouncedPress(M5.BtnC, last_press_c_);
            if (exit_preview) {
                preview_mode_ = false;
                Render();
            }
            return;
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            MoveSelectionUp();
        }
        if (DebouncedLongPress(M5.BtnA, 1200, last_long_press_a_)) {
            NavigateParent();
        }
        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            MoveSelectionDown();
        }
        if (DebouncedLongPress(M5.BtnB, 1200, last_long_press_b_)) {
            EnterSelection();
        }
        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            TriggerPrimaryAction();
        }
        if (DebouncedLongPress(M5.BtnC, 1500, last_long_press_c_)) {
            HandleDelete();
        } else {
            delete_pending_ = false;
        }
    }

    void Cleanup() override {
        if (available_) {
            SD.end();
        }
        entries_.clear();
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

    void RefreshEntries() {
        entries_.clear();
        File dir = SD.open(current_path_);
        if (!dir) {
            ShowStatus("Open dir failed", TFT_RED);
            return;
        }
        for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
            FileEntry info;
            info.name = entry.name();
            info.is_directory = entry.isDirectory();
            info.size = entry.size();
            info.modified = entry.getLastWrite();
            entries_.push_back(info);
            entry.close();
            yield();
        }
        dir.close();
        std::sort(entries_.begin(), entries_.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
            }
            return a.name < b.name;
        });
        if (selected_index_ >= entries_.size()) {
            selected_index_ = entries_.empty() ? 0 : entries_.size() - 1;
        }
    }

    void Render() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("File Explorer");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.printf("%s\n", current_path_.c_str());
        if (entries_.empty()) {
            M5.Display.println("No files");
        } else {
            const size_t max_rows = 4;
            size_t start = 0;
            if (selected_index_ >= max_rows) {
                start = selected_index_ - max_rows + 1;
            }
            for (size_t i = 0; i < max_rows && (start + i) < entries_.size(); ++i) {
                const auto& entry = entries_[start + i];
                if (start + i == selected_index_) {
                    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
                } else {
                    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
                }
                String label = entry.is_directory ? String("[DIR] ") + entry.name : entry.name;
                M5.Display.println(label);
                M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
                if (entry.is_directory) {
                    M5.Display.println("  <folder>");
                } else {
                    M5.Display.printf("  %llu bytes\n", static_cast<unsigned long long>(entry.size));
                }
                M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            }
        }
        M5.Display.println("------------------");
        M5.Display.println("A:Up hold:Parent");
        M5.Display.println("B:Down hold:Enter");
        M5.Display.println("C:Preview hold:Delete");
    }

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("File Explorer");
        M5.Display.println(reason);
    }

    void MoveSelectionUp() {
        if (entries_.empty()) {
            return;
        }
        if (selected_index_ == 0) {
            selected_index_ = entries_.size() - 1;
        } else {
            --selected_index_;
        }
        Render();
    }

    void MoveSelectionDown() {
        if (entries_.empty()) {
            return;
        }
        selected_index_ = (selected_index_ + 1) % entries_.size();
        Render();
    }

    void NavigateParent() {
        if (current_path_ == "/") {
            return;
        }
        int last = current_path_.lastIndexOf('/');
        if (last <= 0) {
            current_path_ = "/";
        } else {
            current_path_ = current_path_.substring(0, last);
            if (!current_path_.endsWith("/")) {
                current_path_ += "/";
            }
        }
        selected_index_ = 0;
        RefreshEntries();
        Render();
    }

    void EnterSelection() {
        if (entries_.empty()) {
            return;
        }
        const auto& entry = entries_[selected_index_];
        if (!entry.is_directory) {
            PreviewFile();
            return;
        }
        String next = current_path_ + entry.name;
        if (!next.endsWith("/")) {
            next += "/";
        }
        current_path_ = next;
        selected_index_ = 0;
        RefreshEntries();
        Render();
    }

    void TriggerPrimaryAction() {
        if (entries_.empty()) {
            return;
        }
        const auto& entry = entries_[selected_index_];
        if (entry.is_directory) {
            EnterSelection();
        } else {
            PreviewFile();
        }
    }

    void PreviewFile() {
        if (entries_.empty()) {
            return;
        }
        const auto& entry = entries_[selected_index_];
        if (entry.is_directory) {
            return;
        }
        String path = current_path_ + entry.name;
        File file = SD.open(path, FILE_READ);
        if (!file) {
            ShowStatus("Preview failed", TFT_RED);
            return;
        }
        preview_mode_ = true;
        if (HasDisplay()) {
            M5.Display.clear();
            M5.Display.setCursor(0, 0);
            M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            M5.Display.println(entry.name);
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            for (int i = 0; i < 6 && file.available(); ++i) {
                String line = file.readStringUntil('\n');
                line.trim();
                M5.Display.println(line);
                yield();
            }
            M5.Display.println("------------------");
            M5.Display.println("Press any key");
        }
        file.close();
    }

    void HandleDelete() {
        if (entries_.empty()) {
            return;
        }
        if (!delete_pending_) {
            delete_pending_ = true;
            ShowStatus("Release to cancel", TFT_YELLOW);
            return;
        }
        const auto& entry = entries_[selected_index_];
        String path = current_path_ + entry.name;
        bool success = false;
        if (entry.is_directory) {
            success = SD.rmdir(path);
        } else {
            success = SD.remove(path);
        }
        ShowStatus(success ? "Deleted" : "Delete failed", success ? TFT_GREEN : TFT_RED);
        RefreshEntries();
        Render();
        delete_pending_ = false;
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("File explorer: ") + message);
        if (!HasDisplay()) {
            return;
        }
        M5.Display.setTextColor(color, TFT_BLACK);
        M5.Display.setCursor(0, 110);
        M5.Display.printf("%-20s\n", message.c_str());
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    bool available_ = false;
    bool preview_mode_ = false;
    bool delete_pending_ = false;
    String current_path_ = "/";
    size_t selected_index_ = 0;
    std::vector<FileEntry> entries_;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
    unsigned long last_long_press_a_ = 0;
    unsigned long last_long_press_b_ = 0;
    unsigned long last_long_press_c_ = 0;
};

}  // namespace bruce::modules::infiltra
