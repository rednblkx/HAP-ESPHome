#include "HAPAccessory.h"
namespace esphome
{
  namespace homekit
  {
    HAPAccessory::HAPAccessory() {
      #ifdef USE_LOCK
      lockCtx = new LockEntity();
      #endif
    }
    HAPAccessory::~HAPAccessory() {
      #ifdef USE_LOCK
      delete lockCtx;
      #endif
    }
    void HAPAccessory::setup() {
      #ifdef USE_LIGHT
      for (const auto v : lights) {
        LightEntity ctx;
        ctx.setup(v);
      }
      #endif
      #ifdef USE_LOCK
      for (const auto v : locks) {
        lockCtx->setup(v);
        #ifdef USE_HOMEKEY
        if (nfc) {
          lockCtx->set_nfc_ctx(nfc);
        }
        #endif
      }
      #endif
      #ifdef USE_SWITCH
      for (const auto v : switches) {
        SwitchEntity ctx;
        ctx.setup(v);
      }
      #endif
      #ifdef USE_SENSOR
      for (const auto v : sensors) {
        SensorEntity ctx;
        ctx.setup(v);
      }
      #endif
      #ifdef USE_CLIMATE
      for (const auto v : climates) {
        ClimateEntity ctx;
        ctx.setup(v);
      }
      #endif
    }
    void HAPAccessory::loop() {}
    void HAPAccessory::dump_config() {}
    #ifdef USE_LIGHT
    void HAPAccessory::add_light(light::LightState* lightPtr) {
      lights.push_back(lightPtr);
    }
    #endif
    #ifdef USE_LOCK
    void HAPAccessory::add_lock(lock::Lock* lockPtr) {
      locks.push_back(lockPtr);
    }
    #endif
    #ifdef USE_HOMEKEY
    void HAPAccessory::set_nfc_ctx(pn532::PN532* nfcCtx) {
      nfc = nfcCtx;
    }
    #endif
    #ifdef USE_SWITCH
    void HAPAccessory::add_switch(switch_::Switch* switchPtr) {
      switches.push_back(switchPtr);
    }
    #endif
    #ifdef USE_SENSOR
    void HAPAccessory::add_sensor(sensor::Sensor* sensorPtr, TemperatureUnits units) {
      sensors.push_back(sensorPtr);
    }
    #endif
    #ifdef USE_CLIMATE
    void HAPAccessory::add_climate(climate::Climate* climatePtr) {
      climates.push_back(climatePtr);
    }
    #endif
  }
}