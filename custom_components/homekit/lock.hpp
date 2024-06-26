#pragma once
#include <esphome/core/defines.h>
#ifdef USE_LOCK
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
namespace esphome
{
  namespace homekit
  {
    class LockEntity
    {
    private:
      static constexpr const char* TAG = "LockEntity";
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
    public:
      LockEntity() {}
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
        hap_serv_t* service = nullptr;
        std::string accessory_name = lockPtr->get_name();
        acc_cfg.name = accessory_name.data();
        acc_cfg.serial_num = std::to_string(lockPtr->get_object_id_hash()).data();
        accessory = hap_acc_create(&acc_cfg);
        service = hap_serv_lock_mechanism_create(lockPtr->state, lockPtr->state);

        ESP_LOGI(TAG, "ID HASH: %lu", lockPtr->get_object_id_hash());
        hap_serv_set_priv(service, strdup(std::to_string(lockPtr->get_object_id_hash()).c_str()));

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, lock_write);

        /* Add the Lock Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);


        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(lockPtr->get_object_id_hash()).c_str()));
        if (!lockPtr->is_internal())
          lockPtr->add_on_state_callback([this, lockPtr]() { this->on_lock_update(lockPtr); });
      }
    };
  }
}
#endif