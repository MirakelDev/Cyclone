#include "config.h"

Config::Config(const char* filename) {
  _filename = filename;
}

JsonObject Config::Get() {
  StaticJsonDocument<200> config;
  File file = SPIFFS.open(_filename, "r");
  if (!file) {
    Serial.print(F("Failed to open config file: "));
    Serial.println(_filename);
    return config.to<JsonObject>();
  } else {
    JsonObject result = config.to<JsonObject>(); // empty object
    DeserializationError error = deserializeJson(config, file);
    if (error) {
      Serial.print(F("deserializeJson() failed with code: "));
      Serial.println(error.c_str());
    } else {
      result = config.as<JsonObject>();
    }
    file.close();
    return result;
  }
}

bool Config::Set(JsonObject settings) {
  File file = SPIFFS.open(_filename, "w");
  if (!file) {
    Serial.print(F("Failed to open config file: "));
    Serial.println(_filename);
    return false;
  } else {
    bool result = false;
    if (serializeJson(settings, file) == 0) {
      Serial.println(F("Failed to write config file"));
    } else {
      result = true;
    }
    file.close();
    return result;
  }
}