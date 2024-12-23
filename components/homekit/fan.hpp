#pragma once
#include <esphome/core/defines.h>
#ifdef USE_FAN
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <map>

namespace esphome
{
  namespace homekit
  {
    class FanEntity
    {
    private:
      static constexpr const char* TAG = "FanEntity";
      std::map<AInfo, const char*> accessory_info = {{NAME, NULL}, {MODEL, "HAP-FAN"}, {SN, NULL}, {MANUFACTURER, "rednblkx"}, {FW_REV, "0.1"}};
      fan::Fan* fanPtr;
      static int fanwrite(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        fan::Fan* fanPtr = (fan::Fan*)serv_priv;
        ESP_LOGD(TAG, "Write called for Accessory %s (%s)", std::to_string(fanPtr->get_object_id_hash()).c_str(), fanPtr->get_name().c_str());
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGD(TAG, "Received Write for fan '%s' -> %s", fanPtr->get_name().c_str(), write->val.b ? "On" : "Off");
            ESP_LOGD(TAG, "[STATE] CURRENT STATE: %d", fanPtr->state);
            fanPtr->make_call().set_state(write->val.b).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        return ret;
      }
      void on_fanupdate(fan::Fan* obj) {
        ESP_LOGD(TAG, "%s state: %s", obj->get_name().c_str(), ONOFF(obj->state));
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_FAN);
          hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
          hap_val_t state;
          state.b = !!obj->state;
          hap_char_update_val(on_char, &state);
        }
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      FanEntity(fan::Fan* fanPtr) : fanPtr(fanPtr) {}
      void setInfo(std::map<AInfo, const char*> info) {
        std::map<AInfo, const char*> merged_info;
        merged_info.merge(info);
        merged_info.merge(this->accessory_info);
        this->accessory_info.swap(merged_info);
      }
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
        /* Create the fan Service. */
        service = hap_serv_fan_create(fanPtr->state);

        ESP_LOGD(TAG, "ID HASH: %lu", fanPtr->get_object_id_hash());
        hap_serv_set_priv(service, fanPtr);

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, fanwrite);

        /* Add the Fan Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(fanPtr->get_object_id_hash()).c_str()));
        if (!fanPtr->is_internal())
          fanPtr->add_on_state_callback([this]() { this->on_fanupdate(fanPtr); });
        ESP_LOGI(TAG, "Fan '%s' linked to HomeKit", accessory_name.c_str());
      }
    };
  }
}
#endif