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
    std::string hostName;
    bool exposeAll = true;
  public:
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    void factory_reset();
    HAPRootComponent();
    void setup() override;
    void loop() override;
    void dump_config() override;
  };


}  // namespace homekit
}  // namespace esphome