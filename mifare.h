#ifndef __MIFARE_H
#define __MIFARE_H

#include <SPI.h>
#include <MFRC522.h>

#include "debug.h"

class Mifare : public MFRC522 {
  public:
    static const byte DEFAULT_KEY[];

    Mifare(byte chipSelectPin, byte resetPowerDownPin);

    void Init();
    void Reinit();

    bool IsNewCardPresent();
    bool IsCardPresent();
    bool IsMagicCard(bool logErrors);
    bool IsDataValid();
    bool GetCardUid(byte *cardUid);
    void SetKeyBySector(int sector, const byte *keyBytes);
    bool AuthenticateBlock(int block, const byte *keyBytes);
    bool ReadCard(bool isMagicCard, bool debugLog);
    bool WriteCard(bool isMagicCard, bool forceWriteBlockZero, bool debugLog);
    size_t ReadData(byte *buffer, size_t maxLen, size_t index);
    void WriteData(byte *buffer, size_t len, size_t index);

    void PrintReaderDetails();
    void PrintData();

  private:
    byte _data[1024];    // data to read/write to the card
    byte _keys[16][6];   // keys to write
    bool _data_received; // true if 1024 bytes written to _data (used for validation)
};

#endif