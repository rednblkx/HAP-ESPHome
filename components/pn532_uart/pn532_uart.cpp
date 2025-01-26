#include "pn532_uart.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_uart {

static const char *const TAG = "pn532_uart";

static const uint8_t WAKEUP_REQUEST[] = {0x55, 0x55, 0x00, 0x00, 0x00};

void PN532Uart::setup() {
  uint8_t buf;

  ESP_LOGI(TAG, "PN532Uart setup started!");

  this->dump_config();

  ESP_LOGI(TAG, "initiating wakeup...");
  // wakeup
  this->write_array(WAKEUP_REQUEST, sizeof(WAKEUP_REQUEST));
  while (this->available()) {
    this->read_byte(&buf);
  }

  PN532::setup();
}

bool PN532Uart::is_read_ready() {
  return (this->available() > 0) ? true : false;
}

bool PN532Uart::write_data(const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Writing data: %s", format_hex_pretty(data).c_str());
  this->write_array(data.data(), data.size());

  return(true);
}

bool PN532Uart::read_data(std::vector<uint8_t> &data, uint8_t len) {
  // Read data (transmission from the PN532 to the host)
  ESP_LOGV(TAG, "Reading data...");

  data.resize(len);
  this->read_array(data.data(), len);

  data.insert(data.begin(), 0x01);
  ESP_LOGV(TAG, "Read data: %s", format_hex_pretty(data).c_str());
  return true;
}

bool PN532Uart::read_response(uint8_t command, std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Reading response");

  std::vector<uint8_t> header(7);
  this->read_array(header.data(), 7);

  ESP_LOGV(TAG, "Header data: %s", format_hex_pretty(header).c_str());

  if (header[0] != 0x00 && header[1] != 0x00 && header[2] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return false;
  }

  bool valid_header = (static_cast<uint8_t>(header[3] + header[4]) == 0 &&  // LCS, len + lcs = 0
                       header[5] == 0xD5 &&        // TFI - frame from PN532 to system controller
                       header[6] == command + 1);  // Correct command response

  bool error_frame = (static_cast<uint8_t>(header[3] + header[4]) == 0 &&
                      header[5] == 0x7F &&
                      header[6] == 0x81);

  bool extended_frame = (header[3] == 0xFF && header[4] == 0xFF);

  if (!valid_header && !error_frame && !extended_frame) {
    ESP_LOGV(TAG, "read data invalid header!");
    return false;
  }


  // full length of message, including command response
  uint16_t full_len = header[3];
  if (extended_frame) {
    ESP_LOGV(TAG, "Abnormal length and checksum, possible Extended Frame");
    header.resize(10);
    this->read_array(header.data() + 7, 3);
    ESP_LOGV(TAG, "EF: Header data: %s", format_hex_pretty(header).c_str());
    if ((uint8_t)(header[5] + header[6] + header[7]) != 0) {
      ESP_LOGV(TAG, "EF: read data invalid header!");

      return false;
    }
    full_len = ((((uint16_t)header[5]) << 8) | header[6]);
  }
  // length of data, excluding command response
  uint16_t len = full_len - 1;
  if (full_len == 0)
    len = 0;

  ESP_LOGV(TAG, "Reading response of length %d", len);

  data.resize(len + 1);
  this->read_array(data.data(), len + 1);


  ESP_LOGV(TAG, "Response data: %s", format_hex_pretty(data).c_str());

  uint8_t checksum = header[5] + header[6];  // TFI + Command response code
  if (extended_frame) {
    checksum = header[8] + header[9];
  }
  for (int i = 0; i < len - 1; i++) {
    uint8_t dat = data[i];
    checksum += dat;
  }
  checksum = ~checksum + 1;

  if (data[len - 1] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[len - 1], checksum);
    return false;
  }

  if (data[len] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return false;
  }

  data.erase(data.end() - 2, data.end());  // Remove checksum and postamble

  if (error_frame) {
    return false;
  }

  return true;
}

void PN532Uart::dump_config() {
  PN532::dump_config();
}

}  // namespace pn532_spi
}  // namespace esphome
