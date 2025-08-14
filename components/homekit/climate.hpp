#pragma once
#include <esphome/core/defines.h>
#ifdef USE_CLIMATE
#include <esphome/core/application.h>
#include <esphome/components/climate/climate_mode.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "const.h"
#include "hap_entity.h"

namespace esphome
{
  namespace homekit
  {
    class ClimateEntity : public HAPEntity
    {
    private:
      static constexpr const char* TAG = "ClimateEntity";
      climate::Climate* climatePtr;
      static void on_climate_update(climate::Climate& obj) {
        ESP_LOGI(TAG, "%s Mode: %d Action: %d CTemp: %.2f TTemp: %.2f CHum: %.2f THum: %.2f", obj.get_name().c_str(), obj.mode, obj.action, obj.current_temperature, obj.target_temperature, obj.current_humidity, obj.target_humidity);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj.get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_THERMOSTAT);
          hap_char_t* current_mode = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE);
          hap_char_t* target_mode = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE);
          hap_char_t* current_temp = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_CURRENT_TEMPERATURE);
          hap_char_t* current_humidity = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);
          hap_char_t* target_temp = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_TARGET_TEMPERATURE);
          hap_char_t* target_humidity = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_TARGET_RELATIVE_HUMIDITY);
          hap_val_t state;
          state.i = obj.action;
          hap_char_update_val(current_mode, &state);
          state.i = obj.mode;
          hap_char_update_val(target_mode, &state);
          state.f = obj.current_temperature;
          hap_char_update_val(current_temp, &state);
          state.f = obj.target_temperature;
          hap_char_update_val(target_temp, &state);
          state.f = obj.target_humidity;
          hap_char_update_val(target_humidity, &state);
          state.f = obj.current_humidity;
          hap_char_update_val(current_humidity, &state);
        }
        return;
      }
      static int climate_read(hap_char_t* hc, hap_status_t* status_code, void* serv_priv, void* read_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        climate::Climate* obj = App.get_climate_by_key(static_cast<uint32_t>(std::stoul(key)));
        int i, ret = HAP_SUCCESS;
        const char* type = hap_char_get_type_uuid(hc);
        if (!strcmp(type, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE)) {
          hap_val_t state;
          switch (obj->action)
          {
          case climate::CLIMATE_ACTION_OFF:
            state.i = 0;
            hap_char_update_val(hc, &state);
            break;
          case climate::CLIMATE_ACTION_HEATING:
            state.i = 1;
            hap_char_update_val(hc, &state);
            break;
          case climate::CLIMATE_ACTION_COOLING:
            state.i = 2;
            hap_char_update_val(hc, &state);
            break;
          default:
            state.i = 0;
            hap_char_update_val(hc, &state);
            break;
          }
        } else if (!strcmp(type, HAP_CHAR_UUID_CURRENT_TEMPERATURE)) {
            hap_val_t state;
            state.f = obj->current_temperature;
            hap_char_update_val(hc, &state);
        } else if (!strcmp(type, HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY)) {
            hap_val_t state;
            state.f = obj->current_humidity;
            hap_char_update_val(hc, &state);
        }
        return ret;
      }
      static int climate_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        climate::Climate* obj = App.get_climate_by_key(static_cast<uint32_t>(std::stoul(key)));
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          const char* type = hap_char_get_type_uuid(write->hc);
          if (!strcmp(type, HAP_CHAR_UUID_TARGET_HEATING_COOLING_STATE)) {
            switch (write->val.i)
            {
            case 0:
              obj->make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_OFF).perform();
              break;
            case 1:
              obj->make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT).perform();
              break;
            case 2:
              obj->make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_COOL).perform();
              break;
            case 3:
              obj->make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_AUTO).perform();
              break;
            default:
              break;
            }
          }
          else if (!strcmp(type, HAP_CHAR_UUID_TARGET_TEMPERATURE)) {
            obj->make_call().set_target_temperature(write->val.f).perform();
          }
          else if (!strcmp(type, HAP_CHAR_UUID_TARGET_RELATIVE_HUMIDITY)) {
            obj->make_call().set_target_humidity(write->val.f).perform();
          }
        }
        return ret;
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      ClimateEntity(climate::Climate* climatePtr) : HAPEntity({{MODEL, "HAP-CLIMATE"}}), climatePtr(climatePtr) {}
      void setup(TemperatureUnits units = CELSIUS) {
        hap_acc_cfg_t acc_cfg = {
            .model = "ESP-CLIMATE",
            .manufacturer = "rednblkx",
            .fw_rev = "0.1.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* service = nullptr;
        hap_serv_t* service_fan = nullptr;
        std::string accessory_name = this->climatePtr->get_name();
        acc_cfg.name = accessory_name.data();
        acc_cfg.serial_num = std::to_string(this->climatePtr->get_object_id_hash()).data();
        climate::ClimateTraits climateTraits = this->climatePtr->get_traits();
        climate::ClimateMode climateMode = this->climatePtr->mode;
        climate::ClimateAction climateAction = this->climatePtr->action;
        uint8_t current_mode = 0;
        uint8_t target_mode = 0;
        switch (climateAction) {
        case climate::ClimateAction::CLIMATE_ACTION_OFF:
          current_mode = 0;
          break;

        case climate::ClimateAction::CLIMATE_ACTION_HEATING:
          current_mode = 1;
          break;

        case climate::ClimateAction::CLIMATE_ACTION_COOLING:
          current_mode = 2;
          break;

        default:
          current_mode = 0;
          break;
        }
        switch (climateMode) {
        case climate::ClimateMode::CLIMATE_MODE_OFF:
          target_mode = 0;
          break;

        case climate::ClimateMode::CLIMATE_MODE_HEAT:
          target_mode = 1;
          break;

        case climate::ClimateMode::CLIMATE_MODE_COOL:
          target_mode = 2;
          break;

        case climate::ClimateMode::CLIMATE_MODE_AUTO:
          target_mode = 3;
          break;

        default:
          break;
        }
        ESP_LOGI(TAG, "CTemp: %.2f TTemp: %.2f CHum: %.2f THum: %.2f", this->climatePtr->current_temperature, this->climatePtr->target_temperature, this->climatePtr->current_humidity, this->climatePtr->target_humidity);
        service = hap_serv_thermostat_create(current_mode, target_mode, this->climatePtr->current_temperature, this->climatePtr->target_temperature, units);
        if (climateTraits.get_supports_current_humidity()) {
          hap_serv_add_char(service, hap_char_current_relative_humidity_create(this->climatePtr->current_humidity));
        }
        if (climateTraits.get_supports_target_humidity()) {
          hap_serv_add_char(service, hap_char_target_relative_humidity_create(this->climatePtr->target_humidity));
        }
        // service_fan = hap_serv_fan_v2_create(!this->climatePtr->fan_mode ? 1 : 0);
        // hap_char_swing_mode_create();
        // hap_serv_link_serv()
        if (service) {
          /* Create accessory object */
          accessory = hap_acc_create(&acc_cfg);
          ESP_LOGI(TAG, "ID HASH: %lu", this->climatePtr->get_object_id_hash());
          hap_serv_set_priv(service, strdup(std::to_string(this->climatePtr->get_object_id_hash()).c_str()));

          /* Set the write callback for the service */
          hap_serv_set_write_cb(service, climate_write);
          hap_serv_set_read_cb(service, climate_read);

          /* Add the Lock Service to the Accessory Object */
          hap_acc_add_serv(accessory, service);


          /* Add the Accessory to the HomeKit Database */
          hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(this->climatePtr->get_object_id_hash()).c_str()));
        }
      }
    };
  }
}
#endif