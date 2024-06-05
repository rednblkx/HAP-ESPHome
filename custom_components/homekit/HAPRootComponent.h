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
// #ifdef USE_ESP32
// #include <freertos/FreeRTOS.h>
// #include <freertos/semphr.h>
// #include <deque>
// #endif
// #include "list_entities.h"

namespace esphome
{
namespace homekit {

  class HAPRootComponent : public Component
  {
  private:
    static constexpr const char* TAG = "HAPRootComponent";
    std::string hostName;
    uint16_t tcpPortNum = 32042;
    static void hap_thread(void *arg);
    #ifdef USE_LIGHT
    static LightEntity lightEntity;
    #endif
    #ifdef USE_LOCK
    static LockEntity lockEntity;
    #endif
    #ifdef USE_SENSOR
    static SensorEntity sensorEntity;
    #endif
    #ifdef USE_SWITCH
    static SwitchEntity switchEntity;
    #endif
  public:
    #ifdef USE_LIGHT
    std::vector<light::LightState*> exclude_lights;
    #endif
    #ifdef USE_LOCK
    std::vector<lock::Lock*> exclude_locks;
    #endif
    #ifdef USE_SENSOR
    std::vector<sensor::Sensor*> exclude_sensors;
    #endif
    #ifdef USE_SWITCH
    std::vector<switch_::Switch*> exclude_switches;
    #endif
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    HAPRootComponent();
    void setup() override;
    void loop() override;
    void dump_config() override;
    // void network_on_connect();
  };


}  // namespace homekit
}  // namespace esphome