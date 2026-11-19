#pragma once
#include "const.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/version.h"
#include <string>
#include <esp_log.h>
#include <map>

namespace esphome
{
  namespace homekit
  {
    // Entity name as std::string. ESPHome 2026.11 deprecates get_name() (removed in
    // 2027.5) in favour of get_name_to(), which does not exist before 2026.11.
    inline std::string entity_name(const EntityBase *entity)
    {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
      char buf[ENTITY_NAME_BUF_SIZE];
      StringRef name = entity->get_name_to(buf);
      return std::string(name.c_str(), name.size());
#else
      const StringRef &name = entity->get_name();
      return std::string(name.c_str(), name.size());
#endif
    }

    class HAPEntity
    {
    private:
      static constexpr const char* TAG = "HAPEntity";
    protected:
      std::map<AInfo, const char*> accessory_info = { {NAME, NULL}, {MODEL, "HAP-GENERIC"}, {SN, NULL}, {MANUFACTURER, "rednblkx"}, {FW_REV, "0.1"} };
    public:
      HAPEntity(){ }
      HAPEntity(std::map<AInfo, const char*> accessory_info) { setInfo(accessory_info); }
      void setInfo(std::map<AInfo, const char*> info) {
        for (const auto& pair : info) {
          if (this->accessory_info.find(pair.first) != this->accessory_info.end()) {
            this->accessory_info[pair.first] = pair.second;
          }
        }
      }
      void virtual setup() { ESP_LOGI(TAG, "Uninmplemented!"); }
    };
  }
}