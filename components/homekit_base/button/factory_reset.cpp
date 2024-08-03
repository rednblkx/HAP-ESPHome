#include "factory_reset.h"

namespace esphome {
namespace homekit {

  void ResetButton::press_action() { this->parent_->factory_reset(); }

}  // namespace homekit
}  // namespace esphome
