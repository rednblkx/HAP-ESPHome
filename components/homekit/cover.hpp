#pragma once
#include <esphome/core/defines.h>
#ifdef USE_COVER
#include <esphome/core/application.h>
#include <hap.h>
#include <sstream>
#include <iomanip>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "hap_entity.h"

namespace esphome
{
  namespace homekit
  {
    class CoverEntity : public HAPEntity
    {
    private:
      static constexpr const char* TAG = "CoverEntity";
      cover::Cover* coverPtr;
      
      static int cover_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        cover::Cover* coverPtr = (cover::Cover*)serv_priv;
        ESP_LOGD(TAG, "Write called for Accessory %s (%s)", std::to_string(coverPtr->get_object_id_hash()).c_str(), coverPtr->get_name().c_str());
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          
          // Check for target door state characteristic (0x32)
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_TARGET_DOOR_STATE)) {
            ESP_LOGD(TAG, "Received Write for garage door '%s' -> %s", coverPtr->get_name().c_str(), write->val.i == 0 ? "Open" : "Close");
            if (write->val.i == 0) {
              // Open
              coverPtr->open();
            } else {
              // Close  
              coverPtr->close();
            }
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        return ret;
      }
      
      static void on_cover_update(cover::Cover* obj) {
        ESP_LOGD(TAG, "%s state: %s", obj->get_name().c_str(), cover_operation_to_str(obj->current_operation));
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, "00000041-0000-1000-8000-0026BB765291"); // Garage Door Opener service
          
          if (hs) {
            hap_char_t* current_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_CURRENT_DOOR_STATE); // Current Door State
            hap_char_t* target_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_TARGET_DOOR_STATE);  // Target Door State
            
            if (current_state && target_state) {
              hap_val_t c, t;
              
              // Read current target_state value to preserve it when stopped
              hap_char_get_val(target_state, &t);
              
              // Map ESPHome cover states to HomeKit garage door states
              switch (obj->current_operation) {
                case cover::COVER_OPERATION_IDLE:
                  if (obj->position == 1.0f) {
                    c.i = 0; // Open
                    t.i = 0; // Target Open
                  } else if (obj->position == 0.0f) {
                    c.i = 1; // Closed
                    t.i = 1; // Target Closed
                  } else {
                    c.i = 4; // Stopped
                  }
                  break;
                case cover::COVER_OPERATION_OPENING:
                  c.i = 2; // Opening
                  t.i = 0; // Target Open
                  break;
                case cover::COVER_OPERATION_CLOSING:
                  c.i = 3; // Closing
                  t.i = 1; // Target Closed
                  break;
              }
              
              hap_char_update_val(current_state, &c);
              hap_char_update_val(target_state, &t);
            }
          }
        }
      }
      
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
      
    public:
      CoverEntity(cover::Cover* coverPtr) : HAPEntity({{MODEL, "HAP-GARAGE-DOOR"}}), coverPtr(coverPtr) {}
      
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
        std::string accessory_name = coverPtr->get_name();
        
        if (accessory_info[NAME] == NULL) {
          acc_cfg.name = strdup(accessory_name.c_str());
        }
        else {
          acc_cfg.name = strdup(accessory_info[NAME]);
        }
        if (accessory_info[SN] == NULL) {
          acc_cfg.serial_num = strdup(std::to_string(coverPtr->get_object_id_hash()).c_str());
        }
        else {
          acc_cfg.serial_num = strdup(accessory_info[SN]);
        }
        
        /* Create accessory object */
        accessory = hap_acc_create(&acc_cfg);
        
        /* Create the garage door opener Service. */
        // Initialize with current states
        int current_state = 1; // Default to closed
        int target_state = 1;  // Default to closed
        
        if (coverPtr->position == 1.0f) {
          current_state = 0; // Open
          target_state = 0;
        } else if (coverPtr->position == 0.0f) {
          current_state = 1; // Closed  
          target_state = 1;
        }
        
        // Try to create garage door service using specific function if available
        // Otherwise fallback to manual creation
  service = hap_serv_create("00000041-0000-1000-8000-0026BB765291"); // Garage Door Opener service UUID
        if (service) {
          // Add required characteristics manually using standard functions if available
          // Current Door State characteristic (0x0E)  
          hap_serv_add_char(service, hap_char_create(HAP_CHAR_UUID_CURRENT_DOOR_STATE, HAP_CHAR_PERM_PR | HAP_CHAR_PERM_EV, HAP_VAL_TYPE_UINT8, sizeof(uint8_t), &current_state, 0, 4, 1));
          // Target Door State characteristic (0x32)
          hap_serv_add_char(service, hap_char_create(HAP_CHAR_UUID_TARGET_DOOR_STATE, HAP_CHAR_PERM_PR | HAP_CHAR_PERM_PW | HAP_CHAR_PERM_EV, HAP_VAL_TYPE_UINT8, sizeof(uint8_t), &target_state, 0, 1, 1));
        }
        
        if (!service) {
          ESP_LOGE(TAG, "Failed to create garage door service");
          return;
        }

        ESP_LOGD(TAG, "ID HASH: %lu", coverPtr->get_object_id_hash());
        hap_serv_set_priv(service, coverPtr);

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, cover_write);

        /* Add the Garage Door Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(coverPtr->get_object_id_hash()).c_str()));
        
        if (!coverPtr->is_internal())
          coverPtr->add_on_state_callback([this]() { CoverEntity::on_cover_update(coverPtr); });
          
        ESP_LOGI(TAG, "Garage Door '%s' linked to HomeKit", accessory_name.c_str());
      }
    };
  }
}
#endif