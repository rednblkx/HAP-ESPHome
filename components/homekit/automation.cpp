#include "automation.h"

namespace esphome
{
  namespace homekit
  {
    void HKAuthTrigger::process(std::string issuerId, std::string endpointId) {
      this->trigger(issuerId, endpointId);
    }
    void HKFailTrigger::process() {
      this->trigger();
    }
  }
}