#ifndef APEX_FILESYSTEM_H
#define APEX_FILESYSTEM_H

#include <Arduino.h>
#include <SPIFFS.h>

bool initFileSystem();
bool fileExists(const char* path);
String readFile(const char* path);
bool writeFile(const char* path, const String& content);
bool deleteFile(const char* path);

#endif
