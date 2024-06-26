#pragma once

#include "esphome/components/button/button.h"
#include "../HAPRootComponent.h"

namespace esphome {
namespace homekit {

class ResetButton : public button::Button, public Parented<HAPRootComponent> {
 public:
  ResetButton() = default;

 protected:
  void press_action() override;
};

}  // namespace homekit
}  // namespace esphome
