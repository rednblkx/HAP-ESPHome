#include <esp_log.h>
#include <esphome/core/defines.h>
#include <esphome/core/component.h>
#ifdef USE_LIGHT
#include "light.hpp"
#endif
#ifdef USE_LOCK
#include "lock.hpp"
#endif
#ifdef USE_SWITCH
#include "switch.hpp"
#endif
#ifdef USE_SENSOR
#include "sensor.hpp"
#endif
#ifdef USE_CLIMATE
#include "climate.hpp"
#endif
namespace esphome
{
  namespace homekit
  {
    class HAPAccessory : public Component
    {
    public:
      HAPAccessory();
      float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1; }
      void setup() override;
      void loop() override;
      void dump_config() override;
#ifdef USE_LIGHT
      std::vector<light::LightState*> lights;
      void add_light(light::LightState* lightPtr);
      #endif
      #ifdef USE_LOCK
      std::vector<lock::Lock*> locks;
      void add_lock(lock::Lock* lockPtr);
      #endif
      #ifdef USE_SWITCH
      std::vector<switch_::Switch*> switches;
      void add_switch(switch_::Switch* switchPtr);
      #endif
      #ifdef USE_SENSOR
      std::vector<sensor::Sensor*> sensors;
      void add_sensor(sensor::Sensor* sensorPtr, TemperatureUnits units);
      #endif
      #ifdef USE_CLIMATE
      std::vector<climate::Climate*> climates;
      void add_climate(climate::Climate* sensorPtr);
      #endif
    };
  }
}