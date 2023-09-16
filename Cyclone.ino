#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "mifare.h"
#include "tnp3xxx.h"

#define DEBUG_LOG false
#define ENABLE_CORS

enum Modes {
  READ,
  WAIT,
  WRITE
};

bool isCardReadSuccessfully = false;
bool isMagicCard = false; // Gen1
bool isCuidCard = false;  // Gen2
enum Modes mode = READ;

AsyncWebServer server(80);
AsyncEventSource events("/events");
Mifare mifare(D8, D4);
Config config("/config.json");

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Started!"));

  SPIFFS.begin();

  initWifi();

  initWebserver();

  mifare.Init();

#if DEBUG_LOG
  mifare.PrintReaderDetails();
#endif
}

void loop() {
  while (true) {
    if (mifare.IsNewCardPresent())
      break;
    if (mode == WRITE) {
      if (mifare.IsCardPresent())
        break;
      else
        events.send("Waiting for card to write...", "write_warning", millis());
    }
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  initSectorKeys();

  switch (mode) {
    case READ:
      isCardReadSuccessfully = mifare.ReadCard(isMagicCard, DEBUG_LOG);
      if (isCardReadSuccessfully) {
        events.send("Card read successfully.", "read_success", millis());
      } else {
        events.send("Failed to read card!", "read_error", millis());
      }
#if DEBUG_LOG
      mifare.PrintData();
#endif
      break;
    case WRITE:
      if (isMagicCard) {
        events.send("Magic card (Gen 1) found, writing card...", "magiccard_success", millis());
        bool success = mifare.WriteCard(isMagicCard, isCuidCard, DEBUG_LOG);
        if (success) {
          events.send("Card written successfully.", "write_success", millis());
        } else {
          events.send("Failed to write card!", "write_error", millis());
        }
      } else {
        events.send("No Magic card (Gen 1) found! Writing card aborted!", "magiccard_error", millis());
      }
#if DEBUG_LOG
      mifare.PrintData();
#endif
      // reset mode after writing dump
      mode = READ;
      mifare.Reinit();
      break;
  }
}

#pragma region Wifi
void initWifi() {
  JsonObject settings = config.Get();
  if (settings.containsKey("ssid") && settings.containsKey("password")) {
    char ssid[100];
    char password[100];
    strcpy(ssid, settings["ssid"]);
    strcpy(password, settings["password"]);

    WiFi.hostname("Cyclone");
    WiFi.begin(ssid, password);

    byte retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
      delay(500);
      retries++;
      Serial.print(F("."));
    }
    if (retries < 20) {
      Serial.println();

      Serial.print(F("Local IP address: "));
      Serial.println(WiFi.localIP());
    } else {
      setupAp();
    }
  } else {
    setupAp();
  }
}

void setupAp() {
  Serial.println(F("Setting AP (Access Point)…"));
  WiFi.softAP("Cyclone", "12345678");

  Serial.print(F("AP IP address: "));
  Serial.println(WiFi.softAPIP());
}
#pragma endregion

#pragma region Webserver
void initWebserver() {
  server.serveStatic("/", SPIFFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("max-age=31536000");

  server.on("/read", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (isCardReadSuccessfully) {
      AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", 1024, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        return mifare.ReadData(buffer, maxLen, index);
      });
      response->addHeader("Content-Disposition", "attachment; filename=\"dump.bin\"");
      request->send(response);
    } else {
      request->send(404);
    }
  });

  server.on("/write", HTTP_POST, [](AsyncWebServerRequest *request) {
      if (mode == WAIT && mifare.IsDataValid()) {
        mode = WRITE;
        request->send(200);
      } else {
        mode = READ;
        request->send(400);
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("Upload started: %s\n", filename.c_str());
        mode = WAIT;
      }
      if (len) {
        mifare.WriteData(data, len, index);
      }
      if (final) {
        Serial.printf("Upload ended: %s, %u B\n", filename.c_str(), index + len);
      }
    });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonObject settings = config.Get();
    if (settings.containsKey("ssid") && settings.containsKey("password")) {
      char json[500];
      serializeJson(settings, json);
      request->send(200, "application/json", json);
    } else {
      request->send(404);
    }
  });

  server.addHandler(new AsyncCallbackJsonWebHandler("/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject settings = json.as<JsonObject>();
    const char* ssid = settings["ssid"];
    const char* password = settings["password"];

    if (*ssid != '\0' && *password != '\0') {
      if (config.Set(settings)) {
        request->onDisconnect([]() {
          ESP.restart();
        });
        events.send("Settings saved successfully. Restarting...", "settings_success", millis());
        request->send(200);
      } else {
        events.send("Failed to save settings!", "settings_error", millis());
        request->send(500);
      }      
    } else {
      events.send("SSID and Password must not be empty!", "settings_warning", millis());
      request->send(400);
    }
  }));

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.addHandler(&events);

#ifdef ENABLE_CORS
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Expose-Headers", "Content-Disposition");
#endif
  server.begin();
}
#pragma endregion

#pragma region Mifare
void initSectorKeys() {
  byte cardUid[4];
  mifare.GetCardUid(cardUid);
  Serial.print(F("UID: "));
  printHex(cardUid, 4);

  isMagicCard = false;
  if (mifare.IsMagicCard(DEBUG_LOG)) {
    Serial.println(F("Magic card (Gen 1) found, no authentication needed"));
    isMagicCard = true;
  } else
  if (mifare.AuthenticateBlock(0, Mifare::DEFAULT_KEY)) {
    Serial.print(F("Successfully authenticated first sector with key: "));
    printHex(Mifare::DEFAULT_KEY, 6);
    for (int sector = 0; sector < 16; sector++)
      mifare.SetKeyBySector(sector, Mifare::DEFAULT_KEY);
  } else {
    byte *keySectorZero = Tnp3xxx::CalculateKeyA(0, cardUid);
    if (mifare.AuthenticateBlock(0, keySectorZero)) {
      Serial.print(F("Successfully authenticated first sector with key: "));
      printHex(keySectorZero, 6);
      for (byte sector = 0; sector < 16; sector++) {
        byte *key = Tnp3xxx::CalculateKeyA(sector, cardUid);
        mifare.SetKeyBySector(sector, key);
      }
    }
  }
}
#pragma endregion

#pragma region Utils
void printHex(const byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.printf("%02X", buffer[i]);
  }
  Serial.println();
}
#pragma endregion
