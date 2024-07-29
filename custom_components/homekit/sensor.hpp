#pragma once
#include <esphome/core/defines.h>
#ifdef USE_SENSOR
#include "const.h"
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
namespace esphome
{
  namespace homekit
  {
    class SensorEntity
    {
    private:
      static constexpr const char* TAG = "SensorEntity";
      sensor::Sensor* sensorPtr;
      void on_sensor_update(sensor::Sensor* obj, float v) {
        ESP_LOGI(TAG, "%s value: %.2f", obj->get_name().c_str(), v);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_serv_get_next(hap_acc_get_first_serv(acc));
          if (hs) {
            hap_char_t* on_char = hap_serv_get_first_char(hs);
            ESP_LOGI(TAG, "HAP CURRENT VALUE: %.2f", hap_char_get_val(on_char)->f);
            hap_val_t state;
            state.f = v;
            hap_char_update_val(on_char, &state);
          }
        }
        return;
      }
      static int sensor_read(hap_char_t* hc, hap_status_t* status_code, void* serv_priv, void* read_priv) {
        if (serv_priv) {
          std::string key((char*)serv_priv);
          ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
          hap_val_t sensorValue;
          sensor::Sensor* obj = App.get_sensor_by_key(static_cast<uint32_t>(std::stoul(key)));
          sensorValue.f = obj->get_state();
          hap_char_update_val(hc, &sensorValue);
          return HAP_SUCCESS;
        }
        return HAP_FAIL;
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      SensorEntity(sensor::Sensor* sensorPtr): sensorPtr(sensorPtr) {}
      void setup() {
        hap_serv_t* service = nullptr;

        std::string device_class = sensorPtr->get_device_class();
        if (std::equal(device_class.begin(), device_class.end(), "temperature")) {
          service = hap_serv_temperature_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "humidity")) {
          service = hap_serv_humidity_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "illuminance")) {
          service = hap_serv_light_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "aqi")) {
          service = hap_serv_air_quality_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "co2")) {
          service = hap_serv_carbon_dioxide_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "co")) {
          service = hap_serv_carbon_monoxide_sensor_create(sensorPtr->state);
        }
        else if (std::equal(device_class.begin(), device_class.end(), "pm10")) {
          service = hap_serv_create(HAP_SERV_UUID_AIR_QUALITY_SENSOR);
          hap_serv_add_char(service, hap_char_pm_10_density_create(sensorPtr->state));
        }
        else if (std::equal(device_class.begin(), device_class.end(), "pm25")) {
          service = hap_serv_create(HAP_SERV_UUID_AIR_QUALITY_SENSOR);
          hap_serv_add_char(service, hap_char_pm_2_5_density_create(sensorPtr->state));
        }
        if (service) {
          hap_acc_cfg_t acc_cfg = {
              .model = "ESP-SENSOR",
              .manufacturer = "rednblkx",
              .fw_rev = "0.1.0",
              .hw_rev = NULL,
              .pv = "1.1.0",
              .cid = HAP_CID_BRIDGE,
              .identify_routine = acc_identify,
          };
          hap_acc_t* accessory = nullptr;
          std::string accessory_name = sensorPtr->get_name();
          acc_cfg.name = accessory_name.data();
          acc_cfg.serial_num = std::to_string(sensorPtr->get_object_id_hash()).data();
          accessory = hap_acc_create(&acc_cfg);
          ESP_LOGD(TAG, "ID HASH: %lu", sensorPtr->get_object_id_hash());
          hap_serv_set_priv(service, strdup(std::to_string(sensorPtr->get_object_id_hash()).c_str()));

          /* Set the write callback for the service */
          hap_serv_set_read_cb(service, sensor_read);

          /* Add the Sensor Service to the Accessory Object */
          hap_acc_add_serv(accessory, service);


          /* Add the Accessory to the HomeKit Database */
          hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(sensorPtr->get_object_id_hash()).c_str()));
          if (!sensorPtr->is_internal())
            sensorPtr->add_on_state_callback([this, sensorPtr](float v) { this->on_sensor_update(sensorPtr, v); });
          ESP_LOGI(TAG, "Sensor '%s' linked to HomeKit", accessory_name.c_str());
        }
      }
    };
  }
}
#endif