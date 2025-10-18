#pragma once

#include <ArduinoOTA.h>

#include "core/Logger.h"

namespace bruce::services {

class OtaService {
   public:
    void Begin() {
        ArduinoOTA.onStart([]() { BRUCE_LOG_INFO("OTA update start"); });
        ArduinoOTA.onEnd([]() { BRUCE_LOG_INFO("OTA update complete"); });
        ArduinoOTA.onError([](ota_error_t error) {
            BRUCE_LOG_ERROR(String("OTA error: ") + error);
        });
        ArduinoOTA.begin();
    }

    void Loop() { ArduinoOTA.handle(); }
};

}  // namespace bruce::services
