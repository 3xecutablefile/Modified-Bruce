#ifndef APEX_SERVER_H
#define APEX_SERVER_H

#include <ESPAsyncWebServer.h>

void setupWebServer(AsyncWebServer* server, bool isSetupMode);
void handleSetupForm(AsyncWebServerRequest* request);

#endif
