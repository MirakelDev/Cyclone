#ifndef __CONFIG_H
#define __CONFIG_H

#include <ArduinoJson.h>
#include <FS.h>

class Config {
  public:
    Config(const char* filename);

    JsonObject Get();
    bool Set(JsonObject settings);
  private:
    const char* _filename;
};

#endif