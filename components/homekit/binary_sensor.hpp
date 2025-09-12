#pragma once
#include <esphome/core/defines.h>
#ifdef USE_BINARY_SENSOR
#include <esphome/core/application.h>
#include <esphome/components/binary_sensor/binary_sensor.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "hap_entity.h"

namespace esphome
{
  namespace homekit
  {
    class BinarySensorEntity : public HAPEntity
    {
    private:
      static inline const char* TAG = "BinarySensorEntity";
      binary_sensor::BinarySensor* binarySensorPtr;
      uint32_t aid_ = 0;
      
      // Helper function to resolve device class mapping
      static inline void resolve_service_mapping(const std::string& device_class, const char*& service_uuid, const char*& char_uuid) {
        if (device_class == "motion") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_MOTION_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_MOTION_DETECTED);
        }
        else if (device_class == "occupancy") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_OCCUPANCY_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_OCCUPANCY_DETECTED);
        }
        else if (device_class == "door" || device_class == "window" || device_class == "opening" || 
                 device_class == "garage_door" || device_class == "vibration" || device_class == "tamper") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_CONTACT_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_CONTACT_SENSOR_STATE);
        }
        else if (device_class == "smoke") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_SMOKE_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_SMOKE_DETECTED);
        }
        else if (device_class == "gas") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_CARBON_MONOXIDE_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_CARBON_MONOXIDE_DETECTED);
        }
        else if (device_class == "moisture" || device_class == "water") {
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_LEAK_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_LEAK_DETECTED);
        }
        else {
          // Default to contact sensor for unknown device classes
          service_uuid = static_cast<const char*>(HAP_SERV_UUID_CONTACT_SENSOR);
          char_uuid = static_cast<const char*>(HAP_CHAR_UUID_CONTACT_SENSOR_STATE);
        }
      }
      
      static void on_binary_sensor_update(binary_sensor::BinarySensor* obj, bool v) {
        ESP_LOGD(TAG, "%s state: %d", obj->get_name().c_str(), v);
        
        // Use cached AID - get from object hash directly since we can't access instance from static callback
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = nullptr;
          hap_char_t* state_char = nullptr;
          
          // Find the service and characteristic based on device class using helper
          const std::string device_class = obj->get_device_class();
          const char* service_uuid;
          const char* char_uuid;
          resolve_service_mapping(device_class, service_uuid, char_uuid);
          
          hs = hap_acc_get_serv_by_uuid(acc, service_uuid);
          if (hs) {
            state_char = hap_serv_get_char_by_uuid(hs, char_uuid);
          }
          
          if (state_char) {
            hap_val_t state = {};
            // MotionDetected is boolean; most other *_DETECTED/STATE chars are u8 enums (0/1).
            if (std::strcmp(char_uuid, HAP_CHAR_UUID_MOTION_DETECTED) == 0) {
              state.b = v;
            } else {
              state.u = v ? 1 : 0;
            }
            hap_char_update_val(state_char, &state);
            ESP_LOGD(TAG, "Binary sensor '%s' (%s) state updated to: %s", obj->get_name().c_str(), device_class.c_str(), v ? "true" : "false");
          } else {
            ESP_LOGW(TAG, "Could not find characteristic for binary sensor '%s' with device class '%s'", obj->get_name().c_str(), device_class.c_str());
          }
        }
      }
      
      static int binary_sensor_read(hap_char_t* hc, hap_status_t* status_code, void* serv_priv, void* read_priv) {
        if (serv_priv) {
          binary_sensor::BinarySensor* binarySensorPtr = (binary_sensor::BinarySensor*)serv_priv;
          ESP_LOGD(TAG, "Read called for Accessory %s (%s)", std::to_string(binarySensorPtr->get_object_id_hash()).c_str(), binarySensorPtr->get_name().c_str());
          const std::string device_class = binarySensorPtr->get_device_class();
          const char* service_uuid = nullptr;
          const char* char_uuid = nullptr;
          resolve_service_mapping(device_class, service_uuid, char_uuid);

          hap_val_t sensorValue = {};
          if (std::strcmp(char_uuid, HAP_CHAR_UUID_MOTION_DETECTED) == 0) {
            sensorValue.b = binarySensorPtr->state;
          } else {
            sensorValue.u = binarySensorPtr->state ? 1 : 0;
          }
          hap_char_update_val(hc, &sensorValue);
          if (status_code) *status_code = HAP_STATUS_SUCCESS;
          return HAP_SUCCESS;
        }
        return HAP_FAIL;
      }
      
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
      
    public:
      BinarySensorEntity(binary_sensor::BinarySensor* binarySensorPtr) : HAPEntity({{MODEL, "HAP-BINARY-SENSOR"}}), binarySensorPtr(binarySensorPtr) {}
      
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
        std::string accessory_name = binarySensorPtr->get_name();
        const std::string device_class = binarySensorPtr->get_device_class();
        
        if (accessory_info[NAME] == NULL) {
          acc_cfg.name = strdup(accessory_name.c_str());
        }
        else {
          acc_cfg.name = strdup(accessory_info[NAME]);
        }
        if (accessory_info[SN] == NULL) {
          acc_cfg.serial_num = strdup(std::to_string(binarySensorPtr->get_object_id_hash()).c_str());
        }
        else {
          acc_cfg.serial_num = strdup(accessory_info[SN]);
        }
        
        /* Create accessory object */
        accessory = hap_acc_create(&acc_cfg);
        
        /* Create the appropriate service based on device class */
        const char* service_uuid;
        const char* char_uuid;
        resolve_service_mapping(device_class, service_uuid, char_uuid);
        
        if (device_class == "motion") {
          service = hap_serv_create(HAP_SERV_UUID_MOTION_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_motion_detected_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created motion sensor service for '%s'", accessory_name.c_str());
          }
        }
        else if (device_class == "occupancy") {
          service = hap_serv_create(HAP_SERV_UUID_OCCUPANCY_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_occupancy_detected_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created occupancy sensor service for '%s'", accessory_name.c_str());
          }
        }
        else if (device_class == "door" || device_class == "window" || device_class == "opening" || 
                 device_class == "garage_door" || device_class == "vibration" || device_class == "tamper") {
          service = hap_serv_create(HAP_SERV_UUID_CONTACT_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_contact_sensor_state_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created contact sensor service for '%s' (device class: %s)", accessory_name.c_str(), device_class.c_str());
          }
        }
        else if (device_class == "smoke") {
          service = hap_serv_create(HAP_SERV_UUID_SMOKE_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_smoke_detected_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created smoke sensor service for '%s'", accessory_name.c_str());
          }
        }
        else if (device_class == "gas") {
          service = hap_serv_create(HAP_SERV_UUID_CARBON_MONOXIDE_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_carbon_monoxide_detected_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created carbon monoxide sensor service for '%s'", accessory_name.c_str());
          }
        }
        else if (device_class == "moisture" || device_class == "water") {
          service = hap_serv_create(HAP_SERV_UUID_LEAK_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_leak_detected_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created leak sensor service for '%s'", accessory_name.c_str());
          }
        }
        
        if (!service) {
          // Default to contact sensor for unknown device classes
          service = hap_serv_create(HAP_SERV_UUID_CONTACT_SENSOR);
          if (service) {
            hap_serv_add_char(service, hap_char_contact_sensor_state_create(binarySensorPtr->state));
            ESP_LOGI(TAG, "Created default contact sensor service for '%s' (unknown device class: %s)", accessory_name.c_str(), device_class.c_str());
          }
        }

        if (!service) {
          ESP_LOGE(TAG, "Failed to create service for binary sensor '%s'", accessory_name.c_str());
          return;
        }

        ESP_LOGD(TAG, "ID HASH: %lu", binarySensorPtr->get_object_id_hash());
        hap_serv_set_priv(service, binarySensorPtr);

        /* Set the read callback for the service */
        hap_serv_set_read_cb(service, binary_sensor_read);

        /* Add the Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        aid_ = hap_get_unique_aid(std::to_string(binarySensorPtr->get_object_id_hash()).c_str());
        hap_add_bridged_accessory(accessory, aid_);
        
        if (!binarySensorPtr->is_internal())
          binarySensorPtr->add_on_state_callback([this](bool v) { BinarySensorEntity::on_binary_sensor_update(binarySensorPtr, v); });
          
        ESP_LOGI(TAG, "Binary sensor '%s' (device class: %s) linked to HomeKit", accessory_name.c_str(), device_class.c_str());
      }
    };
  }
}
#endif