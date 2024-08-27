#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <hap.h>
#include <vector>
#include <tuple>
#include <algorithm>
#include <map>
#include "esphome/components/homekit/const.h"
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
namespace esphome
{
namespace homekit {

  class HAPRootComponent : public Component
  {
  #ifdef USE_BUTTON
    SUB_BUTTON(reset)
  #endif
  private:
    static constexpr const char* TAG = "HAPRootComponent";
    std::map<AInfo, const char*> accessory_info = {{NAME, "ESPH Bridge"}, {MODEL, "HAP-BRIDGE"}, {SN, "16161616"}, {MANUFACTURER, "rednblkx"}, {FW_REV, "0.1"}};
  public:
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    void factory_reset();
    HAPRootComponent(const char* setup_code = "159-35-728", const char* setup_id = "ES32", std::map<AInfo, const char*> info = {{NAME, "ESPH Bridge"}, {MODEL, "HAP-BRIDGE"}, {SN, "16161616"}, {MANUFACTURER, "rednblkx"}, {FW_REV, "0.1"}});
    void setup() override;
    void loop() override;
    void dump_config() override;
  };


}  // namespace homekit
}  // namespace esphome