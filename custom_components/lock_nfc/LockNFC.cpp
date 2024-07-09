#include "LockNFC.h"

using namespace esphome::pn532;

namespace esphome
{
  namespace nfc
  {
    LockNFC::LockNFC(pn532_spi::PN532Spi *&nfc) : pn532_spi::PN532Spi(*nfc), ctx(nfc) {
    }
  }
}