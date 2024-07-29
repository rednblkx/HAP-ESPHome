#include "HAPAccessory.h"
namespace esphome
{
  namespace homekit
  {
    HAPAccessory::HAPAccessory() {
    }
    void HAPAccessory::setup() {
      #ifdef USE_LIGHT
      for (const auto v : lights) {
        v->setup();
      }
      #endif
      #ifdef USE_LOCK
      for (const auto &v : locks) {
        v->setup();
        #ifdef USE_HOMEKEY
        if (nfc) {
          v->set_nfc_ctx(nfc);
        }
        #endif
      }
      #endif
      #ifdef USE_SWITCH
      for (const auto v : switches) {
        v->setup();
      }
      #endif
      #ifdef USE_SENSOR
      for (const auto v : sensors) {
        v->setup();
      }
      #endif
      #ifdef USE_CLIMATE
      for (const auto v : climates) {
        v->setup();
      }
      #endif
    }
    void HAPAccessory::loop() {}
    void HAPAccessory::dump_config() {}
    #ifdef USE_LIGHT
    LightEntity* HAPAccessory::add_light(light::LightState* lightPtr) {
      lights.push_back(new LightEntity(lightPtr));
      return lights.back();
    }
    #endif
    #ifdef USE_LOCK
    LockEntity* HAPAccessory::add_lock(lock::Lock* lockPtr) {
      locks.push_back(new LockEntity(lockPtr));
      return locks.back();
    }
    #endif
    #ifdef USE_HOMEKEY
    void HAPAccessory::set_nfc_ctx(pn532::PN532* nfcCtx) {
      nfc = nfcCtx;
    }
    #endif
    #ifdef USE_SWITCH
    SwitchEntity* HAPAccessory::add_switch(switch_::Switch* switchPtr) {
      switches.push_back(new SwitchEntity(switchPtr));
    }
    #endif
    #ifdef USE_SENSOR
    SensorEntity* HAPAccessory::add_sensor(sensor::Sensor* sensorPtr, TemperatureUnits units) {
      sensors.push_back(new SensorEntity(sensorPtr));
    }
    #endif
    #ifdef USE_CLIMATE
    ClimateEntity* HAPAccessory::add_climate(climate::Climate* climatePtr) {
      climates.push_back(new ClimateEntity(climatePtr));
    }
    #endif
  }
}