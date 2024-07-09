#pragma once
#include <esp_log.h>
#include <esphome/core/defines.h>
#include <esphome/components/pn532_spi/pn532_spi.h>
#include <esphome/components/pn532/pn532.h>

namespace esphome
{
  namespace nfc
  {
    class LockNFC : pn532_spi::PN532Spi
    {
      private:
        const char* TAG = "LockNFC";
      public:
        pn532_spi::PN532Spi*& ctx;
        LockNFC(pn532_spi::PN532Spi*&);
    };
  }
}