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
#include "const.h"
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
#ifdef USE_CLIMATE
#include "climate.hpp"
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
    TemperatureUnits tempUnits;
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
    #ifdef USE_CLIMATE
    ClimateEntity *climateEntity;
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
    #ifdef USE_CLIMATE
    std::vector<climate::Climate*> include_climates;
    std::vector<climate::Climate*> exclude_climates;
    #endif
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    void factory_reset();
    HAPRootComponent(bool, TemperatureUnits);
    void setup() override;
    void loop() override;
    void dump_config() override;
    // void network_on_connect();
  };


}  // namespace homekit
}  // namespace esphome