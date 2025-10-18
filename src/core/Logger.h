#pragma once

#include <Arduino.h>

namespace bruce {

enum class LogLevel {
    kError = 0,
    kWarn,
    kInfo,
    kDebug,
};

class Logger {
   public:
    static void Init(unsigned long baud = 115200) {
        if (!Serial) {
            Serial.begin(baud);
            while (!Serial) {
                delay(10);
            }
        }
    }

    static void Log(LogLevel level, const String& message) {
        if (!Serial) {
            return;
        }
        Serial.print("[");
        Serial.print(LevelToString(level));
        Serial.print("] ");
        Serial.println(message);
    }

    template <typename... Args>
    static void Logf(LogLevel level, const char* fmt, Args... args) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), fmt, args...);
        Log(level, buffer);
    }

    static void Error(const String& message) { Log(LogLevel::kError, message); }
    static void Warn(const String& message) { Log(LogLevel::kWarn, message); }
    static void Info(const String& message) { Log(LogLevel::kInfo, message); }
    static void Debug(const String& message) { Log(LogLevel::kDebug, message); }

   private:
    static const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::kError:
                return "ERROR";
            case LogLevel::kWarn:
                return "WARN";
            case LogLevel::kInfo:
                return "INFO";
            case LogLevel::kDebug:
                return "DEBUG";
            default:
                return "UNK";
        }
    }
};

}  // namespace bruce

#define BRUCE_LOG_ERROR(...) ::bruce::Logger::Logf(::bruce::LogLevel::kError, __VA_ARGS__)
#define BRUCE_LOG_WARN(...) ::bruce::Logger::Logf(::bruce::LogLevel::kWarn, __VA_ARGS__)
#define BRUCE_LOG_INFO(...) ::bruce::Logger::Logf(::bruce::LogLevel::kInfo, __VA_ARGS__)
#define BRUCE_LOG_DEBUG(...) ::bruce::Logger::Logf(::bruce::LogLevel::kDebug, __VA_ARGS__)
