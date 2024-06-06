#ifdef USE_SENSOR
#pragma once
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
      bool exposeAll;
      std::vector<sensor::Sensor*> &included;
      std::vector<sensor::Sensor*>& excluded;
      static constexpr const char* TAG = "SensorEntity";
      void on_sensor_update(sensor::Sensor* obj, float v) {
        ESP_LOGI(TAG, "%s value: %.2f", obj->get_name().c_str(), v);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_SWITCH);
          hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
          hap_val_t state;
          state.b = v;
          hap_char_update_val(on_char, &state);
        }
        return;
      }
      static int sensor_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        int ret = HAP_SUCCESS;
        hap_val_t sensorValue;
        sensor::Sensor* obj = App.get_sensor_by_key(static_cast<uint32_t>(std::stoul(key)));
        sensorValue.f = obj->get_state();
        hap_char_update_val(hc, &sensorValue);
        return ret;
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      SensorEntity(bool exposeAll, std::vector<sensor::Sensor*> &included, std::vector<sensor::Sensor*> &excluded):exposeAll(exposeAll), included(included), excluded(excluded) {
        for (auto* obj : exposeAll ? App.get_sensors() : included) {
          if (!obj->is_internal())
            obj->add_on_state_callback([this, obj](float v) { this->on_sensor_update(obj, v); });
        }
      }
      void setup() {
        hap_acc_cfg_t bridge_cfg = {
            .model = "ESP-SENSOR",
            .manufacturer = "rednblkx",
            .fw_rev = "0.1.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* service = nullptr;
        for (auto entity : exposeAll ? App.get_sensors() : included) 
        {
          bool skip = false;
          for (auto&& e : excluded)
          {
            if (e->get_object_id_hash() == entity->get_object_id_hash()) {
              skip = true;
              break;
            }
          }
          if (skip) continue;
          std::string accessory_name = entity->get_name();
          bridge_cfg.name = accessory_name.data();
          bridge_cfg.serial_num = std::to_string(entity->get_object_id_hash()).data();
          std::string device_class = entity->get_device_class();
          if (std::equal(device_class.begin(), device_class.end(), "temperature")) {
            service = hap_serv_temperature_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "humidity")) {
            service = hap_serv_humidity_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "illuminance")) {
            service = hap_serv_light_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "aqi")) {
            service = hap_serv_air_quality_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "co2")) {
            service = hap_serv_carbon_dioxide_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "co")) {
            service = hap_serv_carbon_monoxide_sensor_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "pm10")) {
            service = hap_char_pm_10_density_create(entity->state);
          } else if (std::equal(device_class.begin(), device_class.end(), "pm25")) {
            service = hap_char_pm_2_5_density_create(entity->state);
          }
          /* Set the Accessory name as the Private data for the service,
          * so that the correct accessory can be identified in the
          * write callback
          */
          if (service) {
            /* Create accessory object */
            accessory = hap_acc_create(&bridge_cfg);
            ESP_LOGI(TAG, "ID HASH: %lu", entity->get_object_id_hash());
            hap_serv_set_priv(service, strdup(std::to_string(entity->get_object_id_hash()).c_str()));

            /* Set the write callback for the service */
            hap_serv_set_read_cb(service, sensor_read);

            /* Add the Lock Service to the Accessory Object */
            hap_acc_add_serv(accessory, service);


            /* Add the Accessory to the HomeKit Database */
            hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(entity->get_object_id_hash()).c_str()));
          }
        }
      }
    };
  }
}
#endif