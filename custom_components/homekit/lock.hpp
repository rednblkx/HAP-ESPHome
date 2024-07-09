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
      nvs_handle savedHKdata;
      readerData_t readerData;
      uint8_t tlv8_data[128];
      #ifdef USE_HOMEKEY
      static int nfcAccess_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv)
      {
        LockEntity* parent = (LockEntity*)serv_priv;
        LOG(D, "PROVISIONED READER KEY: %s", utils::bufToHexString(parent->readerData.reader_sk.data(), parent->readerData.reader_sk.size(), true).c_str());
        LOG(D, "READER PUBLIC KEY: %s", utils::bufToHexString(parent->readerData.reader_pk.data(), parent->readerData.reader_pk.size(), true).c_str());
        LOG(D, "READER GROUP IDENTIFIER: %s", utils::bufToHexString(parent->readerData.reader_gid.data(), parent->readerData.reader_gid.size(), true).c_str());
        LOG(D, "READER UNIQUE IDENTIFIER: %s", utils::bufToHexString(parent->readerData.reader_id.data(), parent->readerData.reader_id.size(), true).c_str());
        int i, ret = HAP_SUCCESS;
          hap_write_data_t *write;
          for (i = 0; i < count; i++) {
              write = &write_data[i];
              /* Setting a default error value */
              *(write->status) = HAP_STATUS_VAL_INVALID;
              if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_NFC_ACCESS_CONTROL_POINT)) {
                hap_tlv8_val_t buf = write->val.t;
                auto tlv_rx_data = std::vector<uint8_t>(buf.buf, buf.buf + buf.buflen);
                // esp_log_buffer_hex_internal(TAG, tlv_rx_data.data(), tlv_rx_data.size(), ESP_LOG_INFO);
                ESP_LOGD(TAG, "TLV RX DATA: %s SIZE: %d", utils::bufToHexString(tlv_rx_data.data(), tlv_rx_data.size(), true).c_str(), tlv_rx_data.size());
                HK_HomeKit ctx(parent->readerData, parent->savedHKdata, "READERDATA", tlv_rx_data);
                auto result = ctx.processResult();
                memcpy(parent->tlv8_data, result.data(), result.size());
                // if (strlen((const char*)readerData.reader_group_id) > 0) {
                //   memcpy(ecpData + 8, readerData.reader_group_id, sizeof(readerData.reader_group_id));
                //   with_crc16(ecpData, 16, ecpData + 16);
                // }
                hap_val_t new_val;
                new_val.t.buf = parent->tlv8_data;
                new_val.t.buflen = result.size();
                hap_char_update_val(write->hc, &new_val);
                *(write->status) = HAP_STATUS_SUCCESS;
              } else {
                  *(write->status) = HAP_STATUS_RES_ABSENT;
              }
          }
          return ret;
      }
      #endif
      static constexpr const char* TAG = "LockEntity";
      #ifdef USE_HOMEKEY
      pn532::PN532* nfc_ctx;
      std::vector<uint8_t> nfcSupportedConfBuffer{ 0x01, 0x01, 0x10, 0x02, 0x01, 0x10 };
      hap_tlv8_val_t nfcSupportedConf = {
        .buf = nfcSupportedConfBuffer.data(),
        .buflen = nfcSupportedConfBuffer.size()
      };
      hap_tlv8_val_t management = {
        .buf = 0,
        .buflen = 0
      };
      #endif
      void on_lock_update(lock::Lock* obj) {
        ESP_LOGI("on_lock_update", "%s state: %s", obj->get_name().c_str(), lock_state_to_string(obj->state));
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_LOCK_MECHANISM);
        hap_char_t* current_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_LOCK_CURRENT_STATE);
        hap_char_t* target_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_LOCK_TARGET_STATE);
        hap_val_t c;
        hap_val_t t;
        if (obj->state == lock::LockState::LOCK_STATE_LOCKED || obj->state == lock::LockState::LOCK_STATE_UNLOCKED) {
          c.i = obj->state % 2;
          t.i = obj->state % 2;
          hap_char_update_val(current_state, &c);
          hap_char_update_val(target_state, &t);
        }
        else if (obj->state == lock::LockState::LOCK_STATE_LOCKING || obj->state == lock::LockState::LOCK_STATE_UNLOCKING) {
          t.i = (obj->state % 5) % 3;
          hap_char_update_val(target_state, &t);
        }
        else if (obj->state == lock::LockState::LOCK_STATE_JAMMED) {
          c.i = obj->state;
          hap_char_update_val(current_state, &c);
        }
        return;
      }
      static int lock_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI("lock_write", "Write called for Accessory %s", (char*)serv_priv);
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          lock::Lock* obj = App.get_lock_by_key(static_cast<uint32_t>(std::stoul(key)));
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_LOCK_TARGET_STATE)) {
            ESP_LOGI("lock_write", "Target State req: %d", write->val.i);
            hap_char_update_val(write->hc, &(write->val));
            hap_char_t* c = hap_serv_get_char_by_uuid(hap_char_get_parent(write->hc), HAP_CHAR_UUID_LOCK_CURRENT_STATE);
            ESP_LOGI("lock_write", "Current State: %d", hap_char_get_val(c)->i);
            hap_char_update_val(c, &(write->val));
            write->val.i ? obj->lock() : obj->unlock();
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        return ret;
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
      void nfc_method(){
        uint8_t data[13] = { 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x08, 0x58, 0x01, 0x01, 0x0 };
        uint8_t selectCmdRes[9];
        uint16_t selectCmdResLength = 9;
        LOG(D, "SELECT HomeKey Applet, APDU: ");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, sizeof(data), ESP_LOG_VERBOSE);
        // write_command_({
        //   PN532_COMMAND_INDATAEXCHANGE
        // })
      }
    public:
      LockEntity() {
      #ifdef USE_HOMEKEY
        auto t = nvs_open("HK_DATA", NVS_READWRITE, &savedHKdata);
        LOG(I, "NVS_OPEN: %s", esp_err_to_name(t));
        size_t len = 0;
        if (!nvs_get_blob(savedHKdata, "READERDATA", NULL, &len)) {
          std::vector<uint8_t> savedBuf(len);
          nvs_get_blob(savedHKdata, "READERDATA", savedBuf.data(), &len);
          LOG(I, "NVS DATA LENGTH: %d", len);
          ESP_LOG_BUFFER_HEX_LEVEL(TAG, savedBuf.data(), savedBuf.size(), ESP_LOG_DEBUG);
          try {
            // delete readerData;
            readerData = msgpack::decode_msgpack<readerData_t>(savedBuf);
          }
          catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
          }
        }
        LOG(D, "PROVISIONED READER KEY: %s", utils::bufToHexString(readerData.reader_sk.data(), readerData.reader_sk.size(), true).c_str());
        LOG(D, "READER PUBLIC KEY: %s", utils::bufToHexString(readerData.reader_pk.data(), readerData.reader_pk.size(), true).c_str());
        LOG(D, "READER GROUP IDENTIFIER: %s", utils::bufToHexString(readerData.reader_gid.data(), readerData.reader_gid.size(), true).c_str());
        LOG(D, "READER UNIQUE IDENTIFIER: %s", utils::bufToHexString(readerData.reader_id.data(), readerData.reader_id.size(), true).c_str());
      #endif
        // readerData = new readerData_t();
      }
      // ~LockEntity() { delete readerData; }
      #ifdef USE_HOMEKEY
      void set_nfc_ctx(pn532::PN532* ctx) {
        nfc_ctx = ctx;
        auto trigger = new nfc::NfcOnTagTrigger();
        ctx->register_ontag_trigger(trigger);
        auto automation_id_3 = new Automation<std::string, nfc::NfcTag>(trigger);
        auto lambdaaction_id_3 = new LambdaAction<std::string, nfc::NfcTag>([this, ctx](std::string x, nfc::NfcTag tag) -> void {
          ESP_LOGD("main", "something nfc");
          std::function<bool(uint8_t*, uint8_t, uint8_t*, uint16_t*, bool)> lambda = [ctx](uint8_t* send, uint8_t sendLen, uint8_t* res, uint16_t* resLen, bool ignoreLog) -> bool {
            auto data = ctx->inDataExchange(std::vector<uint8_t>(send, send + sendLen));
            memcpy(res, data.data(), data.size());
            uint16_t t = data.size();
            memcpy(resLen, &t, sizeof(uint16_t)); return true;
          };
          auto versions = ctx->inDataExchange({ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x08, 0x58, 0x01, 0x01, 0x0 });
          HKAuthenticationContext authCtx(lambda, this->readerData, this->savedHKdata);
          authCtx.authenticate(KeyFlow(0));
          });
        automation_id_3->add_actions({lambdaaction_id_3});
      }
      #endif
      void setup(lock::Lock* lockPtr) {
        hap_acc_cfg_t acc_cfg = {
            .model = "ESP-LOCK",
            .manufacturer = "rednblkx",
            .fw_rev = "0.1.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* lockMechanism = nullptr;
        std::string accessory_name = lockPtr->get_name();
        acc_cfg.name = accessory_name.data();
        acc_cfg.serial_num = std::to_string(lockPtr->get_object_id_hash()).data();
        accessory = hap_acc_create(&acc_cfg);
        lockMechanism = hap_serv_lock_mechanism_create(lockPtr->state, lockPtr->state);

        ESP_LOGI(TAG, "ID HASH: %lu", lockPtr->get_object_id_hash());
        hap_serv_set_priv(lockMechanism, strdup(std::to_string(lockPtr->get_object_id_hash()).c_str()));

        /* Set the write callback for the service */
        hap_serv_set_write_cb(lockMechanism, lock_write);

        /* Add the Lock Service to the Accessory Object */
        hap_acc_add_serv(accessory, lockMechanism);

        #ifdef USE_HOMEKEY
        hap_serv_t* nfcAccess = nullptr;
        hap_serv_t* lockManagement = nullptr;
        nfcAccess = hap_serv_nfc_access_create(0, &management, &nfcSupportedConf);
        lockManagement = hap_serv_lock_management_create(&management, "1.0.0");
        hap_serv_set_priv(nfcAccess, this);
        hap_serv_set_write_cb(nfcAccess, nfcAccess_write);
        hap_acc_add_serv(accessory, nfcAccess);
        #endif

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(lockPtr->get_object_id_hash()).c_str()));
        if (!lockPtr->is_internal())
          lockPtr->add_on_state_callback([this, lockPtr]() { this->on_lock_update(lockPtr); });
      }
    };
  }
}
#endif