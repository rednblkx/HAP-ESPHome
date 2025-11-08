#pragma once
#include <esphome/core/defines.h>
#ifdef USE_FAN
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "hap_entity.h"

namespace esphome
{
  namespace homekit
  {
    class FanEntity : public HAPEntity
    {
    private:
      static constexpr const char* TAG = "FanEntity";
      fan::Fan* fanPtr;
      
      static int fanwrite(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        fan::Fan* fanPtr = (fan::Fan*)serv_priv;
        ESP_LOGD(TAG, "Write called for Accessory %s (%s)", std::to_string(fanPtr->get_object_id_hash()).c_str(), fanPtr->get_name().c_str());
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        auto call = fanPtr->make_call();
        bool update_speed = false;
        bool update_state = false;
        bool update_oscillating = false;
        
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGD(TAG, "Received Write for fan '%s' -> %s", fanPtr->get_name().c_str(), write->val.b ? "On" : "Off");
            call.set_state(write->val.b);
            update_state = true;
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ROTATION_SPEED)) {
            float speed_percentage = write->val.f;
            ESP_LOGD(TAG, "Received Write for fan '%s' speed -> %.1f%%", fanPtr->get_name().c_str(), speed_percentage);

            // Direct mapping: HomeKit percentage (0-100) to ESPHome speed (0-100)
            int speed_level = static_cast<int>(speed_percentage);
            
            ESP_LOGD(TAG, "Setting fan speed to level: %d", speed_level);
            call.set_speed(speed_level);
            update_speed = true;
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_SWING_MODE)) {
            bool swing_mode = write->val.i;
            ESP_LOGD(TAG, "Received Write for fan '%s' oscillation -> %s", fanPtr->get_name().c_str(), swing_mode ? "On" : "Off");
            
            call.set_oscillating(swing_mode);
            update_oscillating = true;
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        
        if (update_state || update_speed || update_oscillating) {
          call.perform();
        }
        return ret;
      }
      
      static void on_fanupdate(fan::Fan* obj) {
        ESP_LOGD(TAG, "%s state: %s, speed: %d, oscillating: %s", 
                 obj->get_name().c_str(), 
                 ONOFF(obj->state), 
                 obj->speed,
                 obj->oscillating ? "On" : "Off");
                 
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_FAN);
          if (hs) {
            // Update On characteristic
            hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
            if (on_char) {
              hap_val_t state;
              state.b = !!obj->state;
              hap_char_update_val(on_char, &state);
            }
            
            // Update Rotation Speed characteristic
            hap_char_t* speed_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ROTATION_SPEED);
            if (speed_char) {
              hap_val_t speed_val;
              // Direct mapping: ESPHome speed (0-100) to HomeKit percentage (0-100)
              speed_val.f = static_cast<float>(obj->speed);
              hap_char_update_val(speed_char, &speed_val);
            }
            
            // Update Swing Mode characteristic
            hap_char_t* swing_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_SWING_MODE);
            if (swing_char) {
              hap_val_t swing_val;
              swing_val.i = obj->oscillating ? 1 : 0; // 1 = enabled, 0 = disabled
              hap_char_update_val(swing_char, &swing_val);
            }
          }
        }
      }
      
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
      
    public:
      FanEntity(fan::Fan* fanPtr) : HAPEntity({{MODEL, "HAP-FAN"}}), fanPtr(fanPtr) {}
      
      void setup() {
        hap_acc_cfg_t acc_cfg = {
            .model = strdup(accessory_info[MODEL]),
            .manufacturer = strdup(accessory_info[MANUFACTURER]),
            .fw_rev = strdup(accessory_info[FW_REV]),
            .hw_rev = NULL,
            .pv = strdup("1.1.0"),
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* service = nullptr;
        std::string accessory_name = fanPtr->get_name();
        
        if (accessory_info[NAME] == NULL) {
          acc_cfg.name = strdup(accessory_name.c_str());
        }
        else {
          acc_cfg.name = strdup(accessory_info[NAME]);
        }
        
        if (accessory_info[SN] == NULL) {
          acc_cfg.serial_num = strdup(std::to_string(fanPtr->get_object_id_hash()).c_str());
        }
        else {
          acc_cfg.serial_num = strdup(accessory_info[SN]);
        }
        
        /* Create accessory object */
        accessory = hap_acc_create(&acc_cfg);
        
        /* Create the fan Service with initial state */
        service = hap_serv_fan_create(fanPtr->state);

        // Add Rotation Speed characteristic
        hap_char_t* speed_char = hap_char_rotation_speed_create(static_cast<float>(fanPtr->speed));
        hap_serv_add_char(service, speed_char);

        // Add Swing Mode characteristic
        hap_char_t* swing_char = hap_char_swing_mode_create(fanPtr->oscillating ? 1 : 0);
        hap_serv_add_char(service, swing_char);

        ESP_LOGD(TAG, "ID HASH: %lu", fanPtr->get_object_id_hash());
        hap_serv_set_priv(service, fanPtr);

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, fanwrite);

        /* Add the Fan Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(fanPtr->get_object_id_hash()).c_str()));
        
        if (!fanPtr->is_internal())
          fanPtr->add_on_state_callback([this]() { FanEntity::on_fanupdate(fanPtr); });
        
        ESP_LOGI(TAG, "Fan '%s' linked to HomeKit with speed and oscillation control", accessory_name.c_str());
      }
    };
  }
}
#endif
