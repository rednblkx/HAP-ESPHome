#include <esphome/core/defines.h>
#ifdef USE_LIGHT
#pragma once
#include <esphome/core/application.h>
#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

namespace esphome
{
  namespace homekit
  {
    class LightEntity
    {
    private:
      static constexpr const char* TAG = "LightEntity";
      std::vector<light::LightState*> included;
      static int light_write(hap_write_data_t write_data[], int count, void* serv_priv, void* write_priv) {
        std::string key((char*)serv_priv);
        ESP_LOGI(TAG, "Write called for Accessory %s", (char*)serv_priv);
        int i, ret = HAP_SUCCESS;
        hap_write_data_t* write;
        for (i = 0; i < count; i++) {
          write = &write_data[i];
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGI(TAG, "Received Write for Light %s state: %s", key.c_str(), write->val.b ? "On" : "Off");
            light::LightState* obj = App.get_light_by_key(static_cast<uint32_t>(std::stoul(key)));
            ESP_LOGI(TAG, "[STATE] CURRENT STATE: %d", (int)(obj->current_values.get_state() * 100));
            write->val.b ? obj->turn_on().set_save(true).perform() : obj->turn_off().set_save(true).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_BRIGHTNESS)) {
            ESP_LOGI(TAG, "Received Write for Light %s Level: %d", key.c_str(), write->val.i);
            light::LightState* obj = App.get_light_by_key(static_cast<uint32_t>(std::stoul(key)));
            ESP_LOGI(TAG, "[LEVEL] CURRENT BRIGHTNESS: %d", (int)(obj->current_values.get_brightness() * 100));
            ESP_LOGI(TAG, "TARGET BRIGHTNESS: %d", (int)write->val.i);
            obj->make_call().set_save(true).set_brightness((float)(write->val.i) / 100).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_HUE)) {
            ESP_LOGI(TAG, "Received Write for Light %s Hue: %.2f", key.c_str(), write->val.f);
            light::LightState* obj = App.get_light_by_key(static_cast<uint32_t>(std::stoul(key)));
            int hue = 0;
            float saturation = 0;
            float colorValue = 0;
            rgb_to_hsv(obj->remote_values.get_red(), obj->remote_values.get_green(), obj->remote_values.get_blue(), hue, saturation, colorValue);
            ESP_LOGI(TAG, "[HUE] CURRENT Hue: %d, Saturation: %.2f, Value: %.2f", hue, saturation, colorValue);
            ESP_LOGI(TAG, "TARGET HUE: %.2f", write->val.f);
            float tR = 0;
            float tG = 0;
            float tB = 0;
            hsv_to_rgb(write->val.f, saturation, colorValue, tR, tG, tB);
            ESP_LOGI(TAG, "TARGET RGB: %.2f %.2f %.2f", tR, tG, tB);
            obj->make_call().set_rgb(tR, tG, tB).set_save(true).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_SATURATION)) {
            ESP_LOGI(TAG, "Received Write for Light %s Saturation: %.2f", key.c_str(), write->val.f);
            light::LightState* obj = App.get_light_by_key(static_cast<uint32_t>(std::stoul(key)));
            int hue = 0;
            float saturation = 0;
            float colorValue = 0;
            rgb_to_hsv(obj->remote_values.get_red(), obj->remote_values.get_green(), obj->remote_values.get_blue(), hue, saturation, colorValue);
            ESP_LOGI(TAG, "[SATURATION] CURRENT Hue: %d, Saturation: %.2f, Value: %.2f", hue, saturation, colorValue);
            ESP_LOGI(TAG, "TARGET SATURATION: %.2f", write->val.f);
            float tR = 0;
            float tG = 0;
            float tB = 0;
            hsv_to_rgb(hue, write->val.f / 100, colorValue, tR, tG, tB);
            ESP_LOGI(TAG, "TARGET RGB: %.2f %.2f %.2f", tR, tG, tB);
            obj->make_call().set_rgb(tR, tG, tB).set_save(true).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_COLOR_TEMPERATURE)) {
            ESP_LOGI(TAG, "Received Write for Light %s Level: %d", key.c_str(), write->val.i);
            light::LightState* obj = App.get_light_by_key(static_cast<uint32_t>(std::stoul(key)));
            ESP_LOGI(TAG, "[LEVEL] CURRENT COLOR TEMPERATURE(mired): %.2f", obj->current_values.get_color_temperature());
            ESP_LOGI(TAG, "TARGET COLOR TEMPERATURE(mired): %lu", write->val.u);
            obj->make_call().set_color_temperature(write->val.u).set_save(true).perform();
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
          }
          else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
          }
        }
        return ret;
      }
      void on_light_update(light::LightState* obj) {
        bool rgb = obj->current_values.get_color_mode() & light::ColorCapability::RGB;
        bool level = obj->get_traits().supports_color_capability(light::ColorCapability::BRIGHTNESS);
        bool temperature = obj->current_values.get_color_mode() & light::ColorCapability::COLOR_TEMPERATURE;
        if (rgb) {
          ESP_LOGI(TAG, "%s RED: %.2f, GREEN: %.2f, BLUE: %.2f", obj->get_name().c_str(), obj->current_values.get_red(), obj->current_values.get_green(), obj->current_values.get_blue());
        }
        ESP_LOGI(TAG, "%s state: %d brightness: %d", obj->get_name().c_str(), (int)(obj->current_values.get_state() * 100), (int)(obj->current_values.get_brightness() * 100));
        hap_acc_t* acc = hap_acc_get_by_aid(hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
        if (acc) {
          hap_serv_t* hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_LIGHTBULB);
          hap_char_t* on_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_ON);
          hap_val_t state;
          state.b = obj->current_values.get_state();
          hap_char_update_val(on_char, &state);
          if (level) {
            hap_char_t* level_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_BRIGHTNESS);
            hap_val_t level;
            level.i = (int)(obj->current_values.get_brightness() * 100);
            hap_char_update_val(level_char, &level);
          }
          if (rgb) {
            hap_char_t* hue_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_HUE);
            hap_char_t* saturation_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_SATURATION);
            hap_val_t hue;
            hap_val_t saturation;
            int cHue = 0;
            float cSaturation = 0;
            float colorValue = 0;
            rgb_to_hsv(obj->current_values.get_red(), obj->current_values.get_green(), obj->current_values.get_blue(), cHue, cSaturation, colorValue);
            hue.f = cHue;
            saturation.f = cSaturation * 100;
            hap_char_update_val(hue_char, &hue);
            hap_char_update_val(saturation_char, &saturation);
          }
          if (temperature) {
            hap_char_t* temp_char = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_COLOR_TEMPERATURE);
            hap_val_t temp;
            temp.u = obj->current_values.get_color_temperature();
            hap_char_update_val(temp_char, &temp);
          }
        }
      }
      static int acc_identify(hap_acc_t* ha) {
        ESP_LOGI(TAG, "Accessory identified");
        return HAP_SUCCESS;
      }
    public:
      LightEntity() {}
      void setup(light::LightState* lightPtr) {
        hap_acc_cfg_t acc_cfg = {
            .model = "ESP-LIGHT",
            .manufacturer = "rednblkx",
            .fw_rev = "0.1.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,
            .identify_routine = acc_identify,
        };
        hap_acc_t* accessory = nullptr;
        hap_serv_t* service = nullptr;
        std::string accessory_name = lightPtr->get_name();
        acc_cfg.name = accessory_name.data();
        acc_cfg.serial_num = std::to_string(lightPtr->get_object_id_hash()).data();
        accessory = hap_acc_create(&acc_cfg);
        int hue = 0;
        float saturation = 0;
        float colorValue = 0;
        rgb_to_hsv(lightPtr->current_values.get_red(), lightPtr->current_values.get_green(), lightPtr->current_values.get_blue(), hue, saturation, colorValue);
        service = hap_serv_lightbulb_create(lightPtr->current_values.get_state());
        hap_serv_add_char(service, hap_char_name_create(accessory_name.data()));
        if (lightPtr->get_traits().supports_color_capability(light::ColorCapability::BRIGHTNESS)) {
          hap_serv_add_char(service, hap_char_brightness_create(lightPtr->current_values.get_brightness() * 100));
        }
        if (lightPtr->get_traits().supports_color_capability(light::ColorCapability::RGB)) {
          hap_serv_add_char(service, hap_char_hue_create(hue));
          hap_serv_add_char(service, hap_char_saturation_create(saturation * 100));
        }
        if (lightPtr->get_traits().supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE)) {
          hap_serv_add_char(service, hap_char_color_temperature_create(lightPtr->current_values.get_color_temperature()));
        }
        ESP_LOGI(TAG, "ID HASH: %lu", lightPtr->get_object_id_hash());
        hap_serv_set_priv(service, strdup(std::to_string(lightPtr->get_object_id_hash()).c_str()));

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, light_write);

        /* Add the Light Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(std::to_string(lightPtr->get_object_id_hash()).c_str()));
        if (!lightPtr->is_internal())
          lightPtr->add_new_target_state_reached_callback([this, lightPtr]() { this->on_light_update(lightPtr); });
      }
    };
  }
}
#endif