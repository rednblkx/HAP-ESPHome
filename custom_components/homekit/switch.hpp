#ifdef USE_SWITCH
#pragma once
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

namespace esphome
{
  namespace homekit
  {
    class SwitchEntity
    {
    private:
      bool exposeAll = true;
      std::vector<switch_::Switch*>& included;
      std::vector<switch_::Switch*>& excluded;
      static constexpr const char* TAG = "SwitchEntity";
      static int switch_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGI(TAG, "Received Write for switch %s state: %s", key.c_str(), write->val.b ? "On" : "Off");
            switch_::Switch* obj = App.get_switch_by_key(static_cast<uint32_t>(std::stoul(key)));
            ESP_LOGI(TAG, "[STATE] CURRENT STATE: %d", obj->state);
            write->val.b ? obj->turn_on() : obj->turn_off();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        return ret;
      }
      void on_switch_update(switch_::Switch* obj, bool v) {
        ESP_LOGI(TAG, "%s state: %d", obj->get_name().c_str(), v);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_SWITCH);
          hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
          hap_val_t state;
          state.b = v;
          hap_char_update_val(on_char, &state);
        }
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      SwitchEntity(bool exposeAll, std::vector<switch_::Switch*>& included, std::vector<switch_::Switch*>& excluded) : exposeAll(exposeAll), included(included), excluded(excluded) {
        for (auto* obj : exposeAll ? App.get_switches() : included) {
          if (!obj->is_internal())
            obj->add_on_state_callback([this, obj](bool v) { this->on_switch_update(obj, v); });
        }
      }
      void setup() {
        hap_acc_cfg_t bridge_cfg = {
            .model = "ESP-Switch",
            .manufacturer = "rednblkx",
            .fw_rev = "0.1.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* service = nullptr;
        for (auto obj : exposeAll ? App.get_switches() : included)
        {
          bool skip = false;
          for (auto&& e : excluded) {
            if (e->get_object_id_hash() == obj->get_object_id_hash()) {
              skip = true;
              break;
            }
          }
          if (skip) continue;
          std::string accessory_name = obj->get_name();
          bridge_cfg.name = accessory_name.data();
          bridge_cfg.serial_num = std::to_string(obj->get_object_id_hash()).data();
          /* Create accessory object */
          accessory = hap_acc_create(&bridge_cfg);
          /* Create the switch Service. Include the "name" since this is a user visible service  */
          service = hap_serv_switch_create(obj->state);

          ESP_LOGI(TAG, "ID HASH: %lu", obj->get_object_id_hash());
          hap_serv_set_priv(service, strdup(std::to_string(obj->get_object_id_hash()).c_str()));

          /* Set the write callback for the service */
          hap_serv_set_write_cb(service, switch_write);

          /* Add the switch Service to the Accessory Object */
          hap_acc_add_serv(accessory, service);

          /* Add the Accessory to the HomeKit Database */
          hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        }
      }
    };
  }
}
#endif