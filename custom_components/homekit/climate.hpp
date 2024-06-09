// #ifdef USE_CLIMATE
#pragma once
#include <esphome/core/application.h>
#include <esphome/components/climate/climate_mode.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "const.h"
namespace esphome
{
  namespace homekit
  {
    class ClimateEntity
    {
    private:
      bool exposeAll;
      TemperatureUnits tempUnits = CELSIUS;
      std::vector<climate::Climate*>& included;
      std::vector<climate::Climate*> &excluded;
      static constexpr const char* TAG = "ClimateEntity";
      void on_climate_update(climate::Climate& obj) {
        ESP_LOGI(TAG, "%s value: %.2f", obj.get_name().c_str(), v);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj.get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_SWITCH);
          hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
          hap_val_t state;
          // state.b = v;
          hap_char_update_val(on_char, &state);
        }
        return;
      }
      static int climate_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        int ret = HAP_SUCCESS;
        hap_val_t ClimateValue;
        climate::Climate* obj = App.get_climate_by_key(static_cast<uint32_t>(std::stoul(key)));
        ClimateValue.f = obj->current_temperature;
        hap_char_update_val(hc, &ClimateValue);
        return ret;
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      ClimateEntity(bool exposeAll, std::vector<climate::Climate*> &included, std::vector<climate::Climate*> &excluded):exposeAll(exposeAll), included(included), excluded(excluded) {
        for (auto* obj : exposeAll ? App.get_climates() : included) {
          if (!obj->is_internal())
            obj->add_on_state_callback([this](climate::Climate &c) { this->on_climate_update(c); });
        }
      }
      void setUnits(TemperatureUnits units) {
        tempUnits = units;
      }
      void setup() {
        hap_acc_cfg_t bridge_cfg = {
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
        for (auto entity : exposeAll ? App.get_climates() : included) 
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
          climate::ClimateTraits climateTraits = entity->get_traits();
          climate::ClimateMode climateMode = entity->mode;
          climate::ClimateAction climateAction = entity->action;
          uint8_t current_mode = 0;
          uint8_t target_mode = 0;
          switch (climateAction)
          {
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
            break;
          }
          switch (climateMode)
          {
          case climate::ClimateMode::CLIMATE_MODE_OFF:
            target_mode = 0;
            break;

          case climate::ClimateMode::CLIMATE_MODE_HEAT:
            target_mode = 1;
            break;

          case climate::ClimateMode::CLIMATE_MODE_COOL:
            target_mode = 2;
            break;

          case climate::ClimateMode::CLIMATE_MODE_HEAT_COOL:
          case climate::ClimateMode::CLIMATE_MODE_AUTO:
            target_mode = 3;
            break;

          default:
            break;
          }
          service = hap_serv_thermostat_create(current_mode, target_mode, entity->current_temperature, entity->target_temperature, tempUnits);
          if (climateTraits.get_supports_current_humidity()) {
            hap_serv_add_char(service, hap_char_current_relative_humidity_create(entity->current_humidity));
          }
          if (climateTraits.get_supports_target_humidity()) {
            hap_char_target_relative_humidity_create(entity->target_humidity);
          }
          // service_fan = hap_serv_fan_v2_create(!entity->fan_mode ? 1 : 0);
          // hap_char_swing_mode_create();
          // hap_serv_link_serv()
          if (service) {
            /* Create accessory object */
            accessory = hap_acc_create(&bridge_cfg);
            ESP_LOGI(TAG, "ID HASH: %lu", entity->get_object_id_hash());
            hap_serv_set_priv(service, strdup(std::to_string(entity->get_object_id_hash()).c_str()));

            /* Set the write callback for the service */
            hap_serv_set_read_cb(service, climate_read);

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
// #endif