#pragma once
#include <esphome/core/defines.h>
#ifdef USE_COVER
#include <esphome/core/application.h>
#include <hap.h>
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
      // HomeKit CurrentDoorState: 0 Open, 1 Closed, 2 Opening, 3 Closing, 4 Stopped
      static uint8_t current_door_state(cover::Cover* obj) {
        if (obj->current_operation == cover::COVER_OPERATION_OPENING) {
          return 2;
        }
        if (obj->current_operation == cover::COVER_OPERATION_CLOSING) {
          return 3;
        }
        if (obj->position >= 0.99f) {
          return 0;
        }
        if (obj->position <= 0.01f) {
          return 1;
        }
        return 4;
      }
      // HomeKit TargetDoorState: 0 Open, 1 Closed
      static uint8_t target_door_state(cover::Cover* obj) {
        uint8_t current = current_door_state(obj);
        return (current == 1 || current == 3) ? 1 : 0;
      }
      static int cover_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        cover::Cover* coverPtr = (cover::Cover*)serv_priv;
        ESP_LOGD(TAG, "Write called for Accessory %s (%s)", std::to_string(coverPtr->get_object_id_hash()).c_str(), coverPtr->get_name().c_str());
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_TARGET_DOOR_STATE)) {
            ESP_LOGD(TAG, "Received Write for garage door '%s' -> %s", coverPtr->get_name().c_str(), write->val.i ? "Close" : "Open");
            auto call = coverPtr->make_call();
            if (write->val.i) {
              call.set_command_close();
            }
            else {
              call.set_command_open();
            }
            call.perform();
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
        ESP_LOGD(TAG, "%s position: %.2f operation: %d", obj->get_name().c_str(), obj->position, (int)obj->current_operation);
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_GARAGE_DOOR_OPENER);
          hap_char_t* current_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_CURRENT_DOOR_STATE);
          hap_char_t* target_state = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_TARGET_DOOR_STATE);
          hap_val_t c;
          hap_val_t t;
          c.i = current_door_state(obj);
          t.i = target_door_state(obj);
          hap_char_update_val(target_state, &t);
          hap_char_update_val(current_state, &c);
        }
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      CoverEntity(cover::Cover* coverPtr) : HAPEntity({{MODEL, "HAP-GARAGE"}}), coverPtr(coverPtr) {}
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
        /* Create the Garage Door Opener Service. ObstructionDetected is required
         * by HAP but has no ESPHome feed yet, so it always reports false. */
        service = hap_serv_garage_door_opener_create(current_door_state(coverPtr), target_door_state(coverPtr), false);

        ESP_LOGD(TAG, "ID HASH: %lu", coverPtr->get_object_id_hash());
        hap_serv_set_priv(service, coverPtr);

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, cover_write);

        /* Add the Garage Door Opener Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(coverPtr->get_object_id_hash()).c_str()));
        if (!coverPtr->is_internal())
          coverPtr->add_on_state_callback([this]() { CoverEntity::on_cover_update(coverPtr); });
        ESP_LOGI(TAG, "Cover '%s' linked to HomeKit as Garage Door Opener", accessory_name.c_str());
      }
    };
  }
}
#endif
