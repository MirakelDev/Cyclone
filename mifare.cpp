#include "mifare.h"

const byte Mifare::DEFAULT_KEY[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

Mifare::Mifare(byte chipSelectPin, byte resetPowerDownPin) : MFRC522(chipSelectPin, resetPowerDownPin) {
}

void Mifare::Init() {
  SPI.begin();
  PCD_Init();
}

void Mifare::Reinit() {
    PICC_HaltA();
    PCD_StopCrypto1();
    PCD_Init();
    PICC_IsNewCardPresent();
    PICC_ReadCardSerial();
}

bool Mifare::IsNewCardPresent() {
  return PICC_IsNewCardPresent() && PICC_ReadCardSerial();
}

bool Mifare::IsCardPresent() {
  for (byte i = 0; i < 3; i++) {
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    MFRC522::StatusCode result = PICC_WakeupA(bufferATQA, &bufferSize);
    if (result == MFRC522::STATUS_OK && PICC_ReadCardSerial()) {
      PICC_HaltA();
      return true;
    }
  }
  return false;
}

bool Mifare::IsMagicCard(bool logErrors) {
  PCD_StopCrypto1();
  bool isMagicCard = MIFARE_OpenUidBackdoor(logErrors);
  if (!isMagicCard) {
    Reinit();
  }
  return isMagicCard;
}

bool Mifare::IsDataValid() {
  return _data_received;
}

bool Mifare::GetCardUid(byte *cardUid) {
  for (byte i = 0; i < 4; i++) {
    cardUid[i] = uid.uidByte[i];
  }
  return true;
}

void Mifare::SetKeyBySector(int sector, const byte *keyBytes) {
  for (byte i = 0; i < 6; i++) {
    _keys[sector][i] = keyBytes[i];
  }
}

bool Mifare::AuthenticateBlock(int block, const byte *keyBytes) {
  MIFARE_Key key;
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = keyBytes[i];
  }
  MFRC522::StatusCode status = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A, block, &key, &(uid));
  if (status != STATUS_OK) {
    Reinit();
    return false;
  } else {
    return true;
  }
}

bool Mifare::ReadCard(bool isMagicCard, bool debugLog) {
  Serial.println(F("Read Card..."));
  byte buffer[18];
  for (byte block = 0; block < 64; block++) {
    // Authenticate sector if needed
    if (block % 4 == 0 && !isMagicCard) {
      byte sector = (block / 4);
      if (AuthenticateBlock(block, _keys[sector])) {
        if (debugLog) Serial.printf("Successfully authenticated sector %02u\n", sector);
      } else {
        PICC_HaltA();
        PCD_StopCrypto1();
        Serial.printf("Failed to authenticate sector %02u\n", sector);
        return false;
      }
    }
    // Read block
    byte byteCount = sizeof(buffer);
    MFRC522::StatusCode status = MIFARE_Read(block, buffer, &byteCount);
    if (status != MFRC522::STATUS_OK) {
      PICC_HaltA();
      PCD_StopCrypto1();
      Serial.println(F("MIFARE_Read() failed: "));
      Serial.println(GetStatusCodeName(status));
      return false;
    } else {
      if (debugLog) Serial.printf("Successfully read block: %02u\n", block);
      for (int p = 0; p < 16; p++) {
        _data[(block*16)+p] = buffer[p];
      }
      if (block % 4 == 3 && !isMagicCard) {
        int sector = (block / 4);
        for (int j = 0; j < 6; j++) {
          _data[(block*16)+j] = _keys[sector][j]; //trailer blocks load sector key A
        };
      }
    }
  }
  Serial.println(F("Successfully read card"));
  PICC_HaltA();
  PCD_StopCrypto1();
  return true;
}

bool Mifare::WriteCard(bool isMagicCard, bool forceWriteBlockZero, bool debugLog) {
  Serial.println(F("Write Card..."));
  for (byte block = 0; block < 64; block++) {
    // Authenticate sector if needed
    if (block % 4 == 0 && !isMagicCard) {
      byte sector = (block / 4);
      if (AuthenticateBlock(block, _keys[sector])) {
        if (debugLog) Serial.printf("Successfully authenticated sector %02u\n", sector);
      } else {
        PICC_HaltA();
        PCD_StopCrypto1();
        Serial.printf("Failed to authenticate sector %02u\n", sector);
        return false;
      }
    }
    // Skip block zero if no magic card is found and force write (used for CUID/Gen2 cards) is false 
    if (block == 0 && !isMagicCard && !forceWriteBlockZero)
      continue;
    // Write block
    byte buffer[16];
    byte byteCount = sizeof(buffer);
    for (int p = 0; p < 16; p++) {
        buffer[p] = _data[(block*16)+p];
    }
    MFRC522::StatusCode status = MIFARE_Write(block, buffer, byteCount);
    if (status != MFRC522::STATUS_OK) {
      PICC_HaltA();
      PCD_StopCrypto1();
      Serial.println(F("MIFARE_Write() failed: "));
      Serial.println(GetStatusCodeName(status));
      return false;
    } else {
      if (debugLog) Serial.printf("Successfully written block: %02u\n", block);
    }
  }
  Serial.println(F("Successfully written card"));
  PICC_HaltA();
  PCD_StopCrypto1();
  return true;
}

size_t Mifare::ReadData(byte *buffer, size_t maxLen, size_t index) {
  size_t j = 0;
  for (size_t i = index; i < index + maxLen; i++) {
    buffer[j] = _data[i];
    j++;
  };
  return j;
}

void Mifare::WriteData(byte *buffer, size_t len, size_t index) {
  for (size_t i = 0; i < len; i++) {
    if (index + i < sizeof(_data)) {
      _data[index + i] = buffer[i];
    }
  }
  _data_received = index + len == sizeof(_data);
}

void Mifare::PrintReaderDetails() {
  Serial.println(F("*****************************"));
  Serial.println(F("MFRC522 Digital self test"));
  Serial.println(F("*****************************"));
  PCD_DumpVersionToSerial();  // Show version of PCD - MFRC522 Card Reader
  Serial.println(F("-----------------------------"));
  Serial.println(F("Only known versions supported"));
  Serial.println(F("-----------------------------"));
  Serial.println(F("Performing test..."));
  bool result = PCD_PerformSelfTest(); // perform the test
  Serial.println(F("-----------------------------"));
  Serial.print(F("Result: "));
  if (result)
    Serial.println(F("OK"));
  else
    Serial.println(F("DEFECT or UNKNOWN"));
  Serial.println();
}

void Mifare::PrintData() {
  for (byte block = 0; block < 64; block++) {
    Serial.printf("Block %02u: ", block);
    for (byte p = 0; p < 16; p++) {
      Serial.printf("%02X", _data[(block*16)+p]);
    }
    Serial.println();
  }
}