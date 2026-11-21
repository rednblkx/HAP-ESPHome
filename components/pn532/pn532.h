#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/gpio.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/automation.h"

#include <cinttypes>
#include <vector>

namespace esphome {
namespace pn532 {

static const uint8_t PN532_COMMAND_VERSION_DATA = 0x02;
static const uint8_t PN532_COMMAND_SAMCONFIGURATION = 0x14;
static const uint8_t PN532_COMMAND_RFCONFIGURATION = 0x32;
static const uint8_t PN532_COMMAND_INDATAEXCHANGE = 0x40;
static const uint8_t PN532_COMMAND_INCOMMUNICATETHRU = 0x42;
static const uint8_t PN532_COMMAND_READREGISTER = 0x06;
static const uint8_t PN532_COMMAND_WRITEREGISTER = 0x08;
static const uint8_t PN532_COMMAND_INLISTPASSIVETARGET = 0x4A;
static const uint8_t PN532_COMMAND_POWERDOWN = 0x16;

static const uint8_t PN532_IC_VERSION = 0x32;
// After this many consecutive failed ECP steps polling skips ahead to InListPassiveTarget.
static const uint16_t PN532_ECP_MAX_FAILURES = 3;
// Warn after this many consecutive failed tag polls, and again every ..._REPEAT failures.
static const uint32_t PN532_POLL_FAILURES_WARN = 10;
static const uint32_t PN532_POLL_FAILURES_REPEAT = 600;

// CIU registers read by the health check (UM0701-02 8.6.22, PN512 register set)
static const uint16_t PN532_REG_CIU_TXCONTROL = 0x6304;  // bit 0/1: Tx1RFEn/Tx2RFEn (antenna drivers)
static const uint16_t PN532_REG_CIU_ERROR = 0x6336;      // bit 6: TempErr (thermal shutdown)
static const uint16_t PN532_REG_CIU_STATUS1 = 0x6337;    // bit 2: RFOn (external RF field detected)
static const uint16_t PN532_REG_CIU_STATUS2 = 0x6338;    // bit 0..2: modem state
static const uint8_t PN532_CIU_TXCONTROL_DRIVERS = 0x03;
static const uint8_t PN532_CIU_ERROR_TEMPERR = 0x40;
static const uint8_t PN532_CIU_STATUS1_RFON = 0x04;

// Watchdog tuning
static const uint8_t PN532_RECOVERY_MAX_LEVEL = 4;
static const uint8_t PN532_HEALTHY_CHECKS_TO_RESET = 3;  // healthy checks before escalation starts over at level 1
static const uint32_t PN532_VERIFY_DELAY_MIN_MS = 5000;  // earliest health check after a recovery
static const uint32_t PN532_RESET_PULSE_MS = 20;
static const uint32_t PN532_RESET_SETTLE_MS = 50;
static const uint32_t PN532_POWER_OFF_MS = 500;
static const uint32_t PN532_POWER_SETTLE_MS = 100;
// RFConfiguration items / values (UM0701-02 7.3.1)
static const uint8_t PN532_RFCFG_FIELD = 0x01;
static const uint8_t PN532_RFCFG_MAX_RETRIES = 0x05;
static const uint8_t PN532_RF_FIELD_ON = 0x01;
static const uint8_t PN532_RF_AUTO_RFCA = 0x02;

enum PN532ReadReady {
  WOULDBLOCK = 0,
  TIMEOUT,
  READY,
};

class PN532BinarySensor;

class PN532 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;

  void loop() override;
  void on_powerdown() override { powerdown(); }

  void register_tag(PN532BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_ontag_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  template<typename F> void add_on_finished_write_callback(F &&callback) {
    this->on_finished_write_callback_.add(std::forward<F>(callback));
  }

  bool is_writing() { return this->next_task_ != READ; };

  // Watchdog / recovery
  void set_health_check_interval(uint32_t interval) { this->health_check_interval_ = interval; }
  void set_max_poll_stall(uint32_t stall) { this->max_poll_stall_ = stall; }
  void set_preventive_reinit_interval(uint32_t interval) { this->preventive_reinit_interval_ = interval; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_power_pin(GPIOPin *pin) { this->power_pin_ = pin; }
  template<typename F> void add_on_recovery_callback(F &&callback) {
    this->on_recovery_callback_.add(std::forward<F>(callback));
  }
  /// Re-initialise the chip now (recovery level 1), e.g. from a button or interval in YAML.
  void reinit();
  /// Number of recovery attempts (any level, including reinit()) since boot.
  uint32_t get_recovery_count() const { return this->recovery_count_; }
  /// Current escalation level (0 = no recovery pending, 1..4).
  uint8_t get_recovery_level() const { return this->escalation_level_; }
  /// Number of completed health checks since boot.
  uint32_t get_health_check_count() const { return this->health_check_count_; }
  /// Result of the last health check (true until the first check fails).
  bool is_healthy() const { return this->healthy_; }
  /// Milliseconds since the last successfully parsed tag poll.
  uint32_t get_last_poll_ok_age() const;

  std::vector<uint8_t> inDataExchange(const std::vector<uint8_t>& data);

  void set_ecp_frame(std::vector<uint8_t>& frame) { ecp_frame = frame; };


  void read_mode();
  void clean_mode();
  void format_mode();
  void write_mode(nfc::NdefMessage *message);
  bool powerdown();
  /// Wake the chip after powerdown(), re-initialise it and resume tag polling.
  bool wakeup();

 protected:
  bool init_chip_();
  /// Wake the chip from power-down (interface specific, e.g. an SPI NSS falling edge).
  virtual void wake_chip_() {}
  /// Bus state while the PN532 is unpowered (power_pin): drive lines low so they don't back-feed the chip.
  virtual void set_bus_power_off_(bool off) {}
  void poll_command_failed_(const char *what);

  bool is_idle_() const;
  void reset_poll_state_();
  void request_health_check_();
  void start_health_check_();
  void health_check_step_();
  void health_check_fail_(const char *reason);
  void finish_health_check_();
  bool read_registers_(const std::vector<uint16_t> &addresses, std::vector<uint8_t> &values);
  uint8_t next_recovery_level_() const;
  void recover_(uint8_t level);
  void finish_recovery_(uint8_t level);
  void preventive_reinit_();
  void ecp_step_failed_();
  bool rf_configure_(const std::vector<uint8_t> &config);
  bool turn_off_rf_();
  bool write_command_(const std::vector<uint8_t> &data);
  bool read_ack_();
  void send_ack_();
  void send_nack_();

  enum PN532ReadReady read_ready_(bool block);
  virtual bool is_read_ready() = 0;
  virtual bool write_data(const std::vector<uint8_t> &data) = 0;
  virtual bool read_data(std::vector<uint8_t> &data, uint8_t len) = 0;
  virtual bool read_response(uint8_t command, std::vector<uint8_t> &data) = 0;

  std::unique_ptr<nfc::NfcTag> read_tag_(nfc::NfcTagUid &uid);

  bool format_tag_(nfc::NfcTagUid &uid);
  bool clean_tag_(nfc::NfcTagUid &uid);
  bool write_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_classic_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  bool write_mifare_classic_block_(uint8_t block_num, const uint8_t *data, size_t len);
  bool auth_mifare_classic_block_(nfc::NfcTagUid &uid, uint8_t block_num, uint8_t key_num, const uint8_t *key);
  bool format_mifare_classic_mifare_(nfc::NfcTagUid &uid);
  bool format_mifare_classic_ndef_(nfc::NfcTagUid &uid);
  bool write_mifare_classic_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);

  std::unique_ptr<nfc::NfcTag> read_mifare_ultralight_tag_(nfc::NfcTagUid &uid);
  bool read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6);
  uint16_t read_mifare_ultralight_capacity_();
  bool find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                    uint8_t &message_start_index);
  bool write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len);
  bool write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message);
  bool clean_mifare_ultralight_();

  std::vector<uint8_t> ecp_frame;

  bool updates_enabled_{ true };
  bool requested_read_{ false };
  bool target_still_present{ false };
  bool requested_ecp_{ false };
  uint8_t ic_version_{0};
  uint16_t ecp_failures_{0};      // consecutive ECP steps without an answer
  uint32_t poll_failures_{0};     // consecutive InListPassiveTarget polls without a valid answer
  uint16_t command_failures_{0};  // consecutive poll commands that were not ACKed
  uint32_t last_poll_ok_{0};      // millis() of the last successfully parsed InListPassiveTarget response
  uint32_t polls_ok_{0};          // successfully parsed InListPassiveTarget responses since boot

  // Watchdog configuration
  uint32_t health_check_interval_{60000};
  uint32_t max_poll_stall_{30000};
  uint32_t preventive_reinit_interval_{0};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *power_pin_{nullptr};

  // Watchdog state
  enum HealthStep : uint8_t {
    HC_IDLE = 0,
    HC_VERSION,
    HC_REGISTERS,
    HC_RF_ON,
    HC_RF_READ,
    HC_RF_OFF,
    HC_EVALUATE,
  } health_step_{HC_IDLE};
  bool hc_ok_{true};
  const char *hc_reason_{""};
  uint8_t hc_fw_[2]{0, 0};
  uint8_t hc_regs_[4]{0, 0, 0, 0};  // TxControl, Error, Status1, Status2 (field off)
  uint8_t hc_tx_on_{0};             // TxControl with the field switched on
  uint32_t next_health_check_{0};
  uint32_t last_reinit_{0};
  uint32_t polls_ok_at_recovery_{0};
  bool healthy_{true};
  bool recovering_{false};
  bool fast_verify_{false};
  uint8_t escalation_level_{0};  // level of the last recovery attempt, 0 = none
  uint8_t verify_level_{0};      // recovery level awaiting confirmation by the next health check
  uint8_t healthy_streak_{0};
  uint32_t recovery_count_{0};
  uint32_t health_check_count_{0};
  CallbackManager<void(int)> on_recovery_callback_;
  int next_flow_{ 0 };
  std::vector<PN532BinarySensor *> binary_sensors_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontag_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontagremoved_;
  nfc::NfcTagUid current_uid_;
  nfc::NdefMessage *next_task_message_to_write_;
  optional<uint32_t> rd_start_time_{};
  enum PN532ReadReady rd_ready_ { WOULDBLOCK };
  enum NfcTask {
    READ = 0,
    CLEAN,
    FORMAT,
    WRITE,
  } next_task_{READ};
  enum PN532Error {
    NONE = 0,
    WAKEUP_FAILED,
    SAM_COMMAND_FAILED,
    VERSION_FAILED,
    RF_CONFIG_FAILED,
  } error_code_{NONE};
  CallbackManager<void()> on_finished_write_callback_;
};

class PN532BinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const nfc::NfcTagUid &uid) { uid_ = uid; }

  bool process(const nfc::NfcTagUid &data);

  void on_scan_end() {
    if (!this->found_) {
      this->publish_state(false);
    }
    this->found_ = false;
  }

 protected:
  nfc::NfcTagUid uid_;
  bool found_{false};
};

template<typename... Ts> class PN532IsWritingCondition : public Condition<Ts...>, public Parented<PN532> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_writing(); }
};

}  // namespace pn532
}  // namespace esphome
