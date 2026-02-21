#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace pn532_uart {

class PN532Uart : public pn532::PN532,
                 public uart::UARTDevice {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  bool is_read_ready() override;
  bool write_data(const std::vector<uint8_t> &data) override;
  bool read_data(std::vector<uint8_t> &data, uint8_t len) override;
  bool read_response(uint8_t command, std::vector<uint8_t> &data) override;
  void flush_rx_() override;
};

}  // namespace pn532_uart
}  // namespace esphome
