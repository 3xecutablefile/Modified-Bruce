#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <functional>

#include "core/Logger.h"

namespace bruce {

class Module {
   public:
    virtual ~Module() = default;
    virtual const char* Name() const = 0;

   virtual void Run() {
        BRUCE_LOG_INFO(String("Entering module: ") + Name());
        Init();
    }

    virtual void Loop() { Update(); }

    virtual void Exit() {
        Cleanup();
        BRUCE_LOG_INFO(String("Exiting module: ") + Name());
    }

    static void SetActivityCallback(std::function<void()> cb) { activity_callback_() = std::move(cb); }

   protected:
    virtual void Init() = 0;
    virtual void Update() = 0;
    virtual void Cleanup() = 0;

    void NotifyActivity() const {
        auto& cb = activity_callback_();
        if (cb) {
            cb();
        }
    }

    bool DebouncedPress(Button_Class& button, unsigned long& last_time_ms, uint32_t interval_ms = 50) {
        if (!button.wasPressed()) {
            return false;
        }
        const auto now = millis();
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
        const auto now = millis();
        if (now - last_time_ms < interval_ms) {
            return false;
        }
        last_time_ms = now;
        NotifyActivity();
        return true;
    }

   private:
    static std::function<void()>& activity_callback_() {
        static std::function<void()> callback;
        return callback;
    }
};

}  // namespace bruce
