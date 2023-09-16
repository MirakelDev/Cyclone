#ifndef __TNP3XXX_H
#define __TNP3XXX_H

#include <stdlib.h>

class Tnp3xxx {
  public:
    static unsigned char* CalculateKeyA(unsigned char sector, unsigned char* nuid);
  private:
    static unsigned long long ComputeCRC48(unsigned char* data, int length);
};

#endif