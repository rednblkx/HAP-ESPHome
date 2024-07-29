#pragma once
#include <esphome/core/defines.h>
#ifdef USE_LOCK
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#ifdef USE_HOMEKEY
#include <nvs.h>
#include <HK_HomeKit.h>
#include <hkAuthContext.h>
#include <esphome/components/pn532/pn532.h>
#include <esphome/core/base_automation.h>
#endif

namespace esphome
{
  namespace homekit
  {
    class LockEntity
    {
    private:
      static constexpr const char* TAG = "LockEntity";
      lock::Lock* ptrToLock;
      static nvs_handle savedHKdata;
      static readerData_t readerData;
      uint8_t tlv8_data[128];
      #ifdef USE_HOMEKEY
      static pn532::PN532* nfc_ctx;
      std::vector<uint8_t> nfcSupportedConfBuffer{ 0x01, 0x01, 0x10, 0x02, 0x01, 0x10 };
      hap_tlv8_val_t nfcSupportedConf = {
        .buf = nfcSupportedConfBuffer.data(),
        .buflen = nfcSupportedConfBuffer.size()
      };
      hap_tlv8_val_t management = {
        .buf = 0,
        .buflen = 0
      };
      std::function<void(std::string issuerId, std::string endpointId)> onSuccess_HK;
      #endif
      static int nfcAccess_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv);
      void on_lock_update(lock::Lock* obj);
      static int lock_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv);
      static void hap_event_handler(hap_event_t event, void* data);
      static int acc_identify(hap_acc_t* ha);


      public:
      LockEntity(lock::Lock* lockPtr);
      void setup();
      void set_nfc_ctx(pn532::PN532* ctx);
      void set_onSuccess_fn(std::function<void(std::string issuerId, std::string endpointId)> onSuccess_HK);
    };
  }
}
#endif