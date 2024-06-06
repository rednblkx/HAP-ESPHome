#pragma once
#include "esphome/core/component.h"
// #include <mdns.h>
// #include "esphome/core/controller.h"
// #include <nvs.h>
// #include <esp_http_server.h>
// #include <sodium/crypto_hash_sha512.h>
// #include <mbedtls/base64.h>
// #include <bootloader_random.h>
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
#ifdef USE_LIGHT
#include "light.hpp"
#endif
#ifdef USE_LOCK
#include "lock.hpp"
#endif
#ifdef USE_SENSOR
#include "sensor.hpp"
#endif
#ifdef USE_SWITCH
#include "switch.hpp"
#endif
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
    static void hap_thread(void *arg);
    #ifdef USE_LIGHT
    LightEntity *lightEntity;
    #endif
    #ifdef USE_LOCK
    LockEntity *lockEntity;
    #endif
    #ifdef USE_SENSOR
    SensorEntity *sensorEntity;
    #endif
    #ifdef USE_SWITCH
    SwitchEntity *switchEntity;
    #endif
  public:
    #ifdef USE_LIGHT
    std::vector<light::LightState*> include_lights;
    std::vector<light::LightState*> exclude_lights;
    #endif
    #ifdef USE_LOCK
    std::vector<lock::Lock*> include_locks;
    std::vector<lock::Lock*> exclude_locks;
    #endif
    #ifdef USE_SENSOR
    std::vector<sensor::Sensor*> include_sensors;
    std::vector<sensor::Sensor*> exclude_sensors;
    #endif
    #ifdef USE_SWITCH
    std::vector<switch_::Switch*> include_switches;
    std::vector<switch_::Switch*> exclude_switches;
    #endif
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    void factory_reset();
    HAPRootComponent(bool);
    void setup() override;
    void loop() override;
    void dump_config() override;
    // void network_on_connect();
  };


}  // namespace homekit
}  // namespace esphome