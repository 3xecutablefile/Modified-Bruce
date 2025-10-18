#pragma once

#include <ESPAsyncWebServer.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>

#include <vector>

#include "config.h"
#include "modules/Module.h"

namespace bruce::modules::infiltra {

class FileSenderModule : public Module {
   public:
    explicit FileSenderModule(AsyncWebServer& server) : server_(server) {}

    const char* Name() const override { return "Send Files"; }

   protected:
    void Init() override {
        PrepareDisplay();
        if (!ENABLE_INFILTRA_FILE_SENDER) {
            RenderUnavailable("Module disabled");
            available_ = false;
            return;
        }
        if (!HasWiFi()) {
            RenderUnavailable("WiFi inactive");
            available_ = false;
            return;
        }
        if (!SD.begin(kSdCsPin)) {
            RenderUnavailable("SD card missing");
            available_ = false;
            return;
        }
        available_ = true;
        EnsureRoutes();
        ComputeServerUrl();
        RenderIdle();
    }

    void Update() override {
        if (!available_) {
            return;
        }

        if (DebouncedPress(M5.BtnA, last_press_a_)) {
            server_enabled_ = true;
            ComputeServerUrl();
            ShowStatus("Server ready", TFT_GREEN);
            RenderActive();
        }
        if (DebouncedPress(M5.BtnB, last_press_b_)) {
            server_enabled_ = false;
            ShowStatus("Server paused", TFT_YELLOW);
            RenderIdle();
        }
        if (DebouncedPress(M5.BtnC, last_press_c_)) {
            ComputeServerUrl();
            RenderActive();
        }

        if (info_message_active_ && millis() > info_message_until_) {
            info_message_active_ = false;
            RenderActive();
        }
    }

    void Cleanup() override {
        server_enabled_ = false;
        if (available_) {
            SD.end();
        }
        if (upload_file_) {
            upload_file_.close();
            upload_file_ = File();
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

    void EnsureRoutes() {
        if (routes_registered_) {
            return;
        }
        server_.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            if (!server_enabled_) {
                request->send(503, "text/plain", "file service paused");
                return;
            }
            String html = "<html><head><title>File Share</title></head><body><h1>Files</h1><ul>";
            File root = SD.open("/");
            if (!root) {
                request->send(500, "text/plain", "sd error");
                return;
            }
            for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
                String name = entry.name();
                if (!entry.isDirectory()) {
                    html += "<li><a href='" + name + "'>" + name + "</a> (" + String(entry.size()) + " bytes)</li>";
                }
                entry.close();
                yield();
            }
            root.close();
            html += "</ul><p>Uploads can be sent via /upload (POST).</p></body></html>";
            request->send(200, "text/html", html);
        });

        server_.on("/upload", HTTP_POST, [this](AsyncWebServerRequest* request) {
            if (!server_enabled_) {
                request->send(503, "text/plain", "file service paused");
                return;
            }
            request->send(200, "text/plain", "Upload handled");
        },
                                [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data,
                                       size_t len, bool final) {
                                    if (!server_enabled_) {
                                        return;
                                    }
                                    if (!upload_file_) {
                                        String path = "/" + filename;
                                        upload_file_ = SD.open(path, FILE_WRITE);
                                        upload_bytes_written_ = 0;
                                    }
                                    if (upload_file_) {
                                        upload_file_.write(data, len);
                                        upload_bytes_written_ += len;
                                        if (final) {
                                            upload_file_.close();
                                            upload_file_ = File();
                                            ++uploads_completed_;
                                        }
                                    }
                                    yield();
                                });

        server_.onNotFound([this](AsyncWebServerRequest* request) {
            if (!server_enabled_) {
                request->send(503, "text/plain", "file service paused");
                return;
            }
            String path = request->url();
            if (path.startsWith("/")) {
                path.remove(0, 1);
            }
            if (!SD.exists(path)) {
                request->send(404, "text/plain", "not found");
                return;
            }
            total_downloads_++;
            request->send(SD, path, "application/octet-stream");
        });

        routes_registered_ = true;
    }

    void ComputeServerUrl() {
        if (!HasWiFi()) {
            server_url_ = "http://0.0.0.0";
            return;
        }
        IPAddress ip = WiFi.localIP();
        if (ip.toString() == "0.0.0.0") {
            ip = WiFi.softAPIP();
        }
        server_url_ = String("http://") + ip.toString() + ":80";
    }

    void RenderIdle() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("File Sender");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.println("Press A to start");
        M5.Display.println("Press B to stop");
        M5.Display.println(server_url_);
        M5.Display.println("Uploads handled via /upload");
    }

    void RenderActive() {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.println("File Sender");
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.println(server_enabled_ ? "Online" : "Paused");
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.println(server_url_);
        DrawQrPlaceholder();
        M5.Display.println("Downloads: " + String(total_downloads_));
        M5.Display.println("Uploads: " + String(uploads_completed_));
        if (upload_bytes_written_ > 0) {
            M5.Display.printf("Last upload: %u bytes\n", static_cast<unsigned>(upload_bytes_written_));
        }
        M5.Display.println("A:Start B:Stop C:Refresh");
    }

    void DrawQrPlaceholder() {
        if (!HasDisplay()) {
            return;
        }
        const int start_x = 0;
        const int start_y = 40;
        const int size = 4;
        uint32_t hash = 0;
        for (size_t i = 0; i < server_url_.length(); ++i) {
            hash = (hash * 131) + server_url_[i];
        }
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 6; ++col) {
                bool bit = ((hash >> (row * 5 + col)) & 1) != 0;
                uint16_t color = bit ? TFT_BLACK : TFT_WHITE;
                M5.Display.fillRect(start_x + col * size, start_y + row * size, size, size, color);
            }
        }
        M5.Display.setCursor(0, start_y + 6 * size + 2);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.println("Scan URL above");
    }

    void ShowStatus(const String& message, uint16_t color) {
        BRUCE_LOG_INFO(String("File sender: ") + message);
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

    void RenderUnavailable(const char* reason) {
        if (!HasDisplay()) {
            return;
        }
        M5.Display.clear();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.println("File Sender");
        M5.Display.println(reason);
    }

    AsyncWebServer& server_;
    bool routes_registered_ = false;
    bool server_enabled_ = false;
    bool available_ = false;
    bool info_message_active_ = false;
    unsigned long info_message_until_ = 0;
    String server_url_ = "http://0.0.0.0";
    size_t total_downloads_ = 0;
    size_t uploads_completed_ = 0;
    File upload_file_;
    size_t upload_bytes_written_ = 0;
    unsigned long last_press_a_ = 0;
    unsigned long last_press_b_ = 0;
    unsigned long last_press_c_ = 0;
};

}  // namespace bruce::modules::infiltra
