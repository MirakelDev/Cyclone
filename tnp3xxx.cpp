#include "tnp3xxx.h"

unsigned char* Tnp3xxx::CalculateKeyA(unsigned char sector, unsigned char* uid)
{
  if (sector == 0) {
    unsigned long long result = (unsigned long long)73 * 2017 * 560381651;
    unsigned char* bytes = (unsigned char*)&result;
    unsigned char* reversedBytes = (unsigned char*)malloc(8 * sizeof(unsigned char));
    for (int i = 0; i < 8; i++) {
      reversedBytes[i] = bytes[7 - i];
    }
    unsigned char* skippedBytes = (unsigned char*)malloc(6 * sizeof(unsigned char));
    for (int i = 0; i < 6; i++) {
      skippedBytes[i] = reversedBytes[i + 2];
    }
    free(reversedBytes);
    return skippedBytes;
  }
  unsigned char data[5] = { uid[0], uid[1], uid[2], uid[3], sector };
  unsigned long long bigEndianCRC = ComputeCRC48(data, 5);
  unsigned long long littleEndianCRC = 0;
  for (unsigned char i = 0; i < 6; i++) {
    unsigned long long bte = (bigEndianCRC >> (i * 8)) & 0xFF;
    littleEndianCRC += bte << ((5 - i) * 8);
  }
  unsigned char* bytes = (unsigned char*)&littleEndianCRC;
  unsigned char* reversedBytes = (unsigned char*)malloc(8 * sizeof(unsigned char));
  for (int i = 0; i < 8; i++) {
    reversedBytes[i] = bytes[7 - i];
  }
  unsigned char* skippedBytes = (unsigned char*)malloc(6 * sizeof(unsigned char));
  for (int i = 0; i < 6; i++) {
    skippedBytes[i] = reversedBytes[i + 2];
  }
  free(reversedBytes);
  return skippedBytes;
}

unsigned long long Tnp3xxx::ComputeCRC48(unsigned char* data, int length)
{
  const unsigned long long polynomial = 0x42f0e1eba9ea3693;
  const unsigned long long initialRegisterValue = 2 * 2 * 3 * 1103 * 12868356821;
  unsigned long long crc = initialRegisterValue;
  for (int i = 0; i < length; i++) {
    crc ^= (unsigned long long)data[i] << 40;
    for (int j = 0; j < 8; j++) {
      if ((crc & 0x800000000000) > 0) {
        crc = (crc << 1) ^ polynomial;
      } else {
        crc <<= 1;
      }
      crc &= 0x0000FFFFFFFFFFFF;
    }
  }
  return crc;
}