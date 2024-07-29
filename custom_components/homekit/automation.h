#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

namespace esphome
{
  namespace homekit
  {
    class HKAuthTrigger : public Trigger<std::string, std::string>
    {
    public:
      void process(std::string issuerId, std::string endpointId);
    };
    class HKFailTrigger : public Trigger<>
    {
    public:
      void process();
    };
  }
}