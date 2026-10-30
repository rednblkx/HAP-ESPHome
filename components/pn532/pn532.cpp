#include "pn532.h"

#include <memory>
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532 {

static const char *const TAG = "pn532";

void PN532::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  if (this->power_pin_ != nullptr) {
    this->power_pin_->setup();
    this->power_pin_->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  if (this->power_pin_ != nullptr || this->reset_pin_ != nullptr) {
    delay(PN532_RESET_SETTLE_MS);
    this->wake_chip_();
  }

  bool ok = this->init_chip_();
  // setup() may block, so try the hardware recovery levels synchronously before giving up.
  if (!ok && this->reset_pin_ != nullptr) {
    ESP_LOGW(TAG, "Initialisation failed, trying a hardware reset");
    this->reset_pin_->digital_write(false);
    delay(PN532_RESET_PULSE_MS);
    this->reset_pin_->digital_write(true);
    delay(PN532_RESET_SETTLE_MS);
    this->wake_chip_();
    ok = this->init_chip_();
  }
  if (!ok && this->power_pin_ != nullptr) {
    ESP_LOGW(TAG, "Initialisation failed, trying a power cycle");
    this->set_bus_power_off_(true);
    if (this->reset_pin_ != nullptr)
      this->reset_pin_->digital_write(false);
    this->power_pin_->digital_write(false);
    delay(PN532_POWER_OFF_MS);
    App.feed_wdt();
    this->power_pin_->digital_write(true);
    if (this->reset_pin_ != nullptr)
      this->reset_pin_->digital_write(true);
    this->set_bus_power_off_(false);
    delay(PN532_POWER_SETTLE_MS);
    this->wake_chip_();
    ok = this->init_chip_();
  }
  if (!ok) {
    this->mark_failed();
    return;
  }

  const uint32_t now = millis();
  this->last_poll_ok_ = now;
  this->last_reinit_ = now;
  this->next_health_check_ = now + this->health_check_interval_;
}

bool PN532::init_chip_() {
  // Forget any half-finished readiness wait from before.
  this->rd_start_time_.reset();
  this->rd_ready_ = WOULDBLOCK;

  // An ACK frame sent by the host aborts whatever command the PN532 may still be
  // processing (UM0701-02 6.2.1.3), so we start from a known idle state even if
  // the chip was left mid-command (e.g. after an ESP software restart).
  this->send_ack_();

  // Get version data
  std::vector<uint8_t> version_data;
  bool got_version = false;
  for (uint8_t attempt = 0; attempt < 2 && !got_version; attempt++) {
    version_data.clear();
    if (!this->write_command_({PN532_COMMAND_VERSION_DATA})) {
      ESP_LOGW(TAG, "Error sending version command%s", attempt == 0 ? ", trying again" : "");
      continue;
    }
    got_version = this->read_response(PN532_COMMAND_VERSION_DATA, version_data) && version_data.size() >= 3;
    if (!got_version)
      ESP_LOGW(TAG, "Error getting version%s", attempt == 0 ? ", trying again" : "");
  }
  if (!got_version) {
    ESP_LOGE(TAG, "No version response from PN532");
    this->error_code_ = VERSION_FAILED;
    return false;
  }
  if (version_data[0] != PN532_IC_VERSION)
    ESP_LOGW(TAG, "Unexpected IC version 0x%02X (expected 0x%02X)", version_data[0], PN532_IC_VERSION);
  this->ic_version_ = version_data[0];
  App.feed_wdt();
  ESP_LOGD(TAG, "Found chip PN5%02X, Firmware v%d.%d", version_data[0], version_data[1], version_data[2]);

  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,  // normal mode
          0x14,  // zero timeout (not in virtual card mode)
          0x01,
      })) {
    ESP_LOGE(TAG, "No wakeup ack");
    this->error_code_ = WAKEUP_FAILED;
    return false;
  }

  std::vector<uint8_t> wakeup_result;
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, wakeup_result)) {
    this->error_code_ = WAKEUP_FAILED;
    return false;
  }

  // Set up SAM (secure access module)
  uint8_t sam_timeout = std::min<uint8_t>(255u, this->update_interval_ / 50);
  if (!this->write_command_({
          PN532_COMMAND_SAMCONFIGURATION,
          0x01,         // normal mode
          sam_timeout,  // timeout as multiple of 50ms (actually only for virtual card mode, but shouldn't matter)
          0x01,         // Enable IRQ
      })) {
    this->error_code_ = SAM_COMMAND_FAILED;
    return false;
  }

  std::vector<uint8_t> sam_result;
  if (!this->read_response(PN532_COMMAND_SAMCONFIGURATION, sam_result)) {
    ESP_LOGV(TAG, "Invalid SAM result: (%u)", sam_result.size());  // NOLINT
    for (uint8_t dat : sam_result) {
      ESP_LOGV(TAG, " 0x%02X", dat);
    }
    this->error_code_ = SAM_COMMAND_FAILED;
    return false;
  }

  // Cycle the RF field once (off, settle, on) so the CIU antenna drivers are
  // re-armed explicitly, and enable Auto RFCA like HomeKey-ESP32 v0.8 does
  // (RFConfiguration item 1 = 0x03: AutoRFCA + RF on).
  if (!this->rf_configure_({PN532_RFCFG_FIELD, 0x00})) {
    ESP_LOGE(TAG, "Error switching RF field off");
    this->error_code_ = RF_CONFIG_FAILED;
    return false;
  }
  delay(10);
  App.feed_wdt();
  if (!this->rf_configure_({PN532_RFCFG_FIELD, PN532_RF_AUTO_RFCA | PN532_RF_FIELD_ON})) {
    ESP_LOGE(TAG, "Error switching RF field on");
    this->error_code_ = RF_CONFIG_FAILED;
    return false;
  }

  // Set up Passive Activation Tries to 0 (MxRtyATR 0xFF, MxRtyPSL 0x01, MxRtyPassiveActivation 0x00)
  if (!this->rf_configure_({PN532_RFCFG_MAX_RETRIES, 0xFF, 0x01, 0x00})) {
    ESP_LOGE(TAG, "Error setting passive activation retries");
    this->error_code_ = WAKEUP_FAILED;
    return false;
  }

  if (!this->turn_off_rf_()) {
    this->error_code_ = RF_CONFIG_FAILED;
    return false;
  }

  this->error_code_ = NONE;
  return true;
}

bool PN532::powerdown() {
  this->updates_enabled_ = false;
  this->requested_read_ = false;
  this->requested_ecp_ = false;
  this->next_flow_ = 0;
  ESP_LOGI(TAG, "Powering down PN532");
  if (!this->write_command_({PN532_COMMAND_POWERDOWN, 0b10100000})) {  // enable i2c,spi wakeup
    ESP_LOGE(TAG, "Error writing powerdown command to PN532");
    return false;
  }
  std::vector<uint8_t> response;
  if (!this->read_response(PN532_COMMAND_POWERDOWN, response) || response.empty()) {
    ESP_LOGE(TAG, "Error reading PN532 powerdown response");
    return false;
  }
  if (response[0] != 0x00) {
    ESP_LOGE(TAG, "Error on PN532 powerdown: %02x", response[0]);
    return false;
  }
  ESP_LOGV(TAG, "Powerdown successful");
  delay(1);
  return true;
}

bool PN532::wakeup() {
  ESP_LOGI(TAG, "Waking up PN532");
  this->wake_chip_();
  bool ok = this->init_chip_();
  if (!ok) {
    ESP_LOGW(TAG, "PN532 re-initialisation after wake-up failed");
    this->status_set_warning();
  }
  // Resume polling even if init failed, so the failure stays visible and is retried.
  this->reset_poll_state_();
  const uint32_t now = millis();
  this->last_poll_ok_ = now;
  this->last_reinit_ = now;
  this->next_health_check_ = now + (ok ? this->health_check_interval_ : 0);
  this->updates_enabled_ = true;
  return ok;
}

void PN532::poll_command_failed_(const char *what) {
  ESP_LOGW(TAG, "%s", what);
  this->status_set_warning();
  if (this->command_failures_ < UINT16_MAX)
    this->command_failures_++;
  if (this->command_failures_ == PN532_ECP_MAX_FAILURES)
    this->request_health_check_();
}

void PN532::ecp_step_failed_() {
  // Without this the ECP state machine stays in the same phase forever when the
  // chip stops answering it, and InListPassiveTarget is never sent again.
  if (this->ecp_failures_ < UINT16_MAX)
    this->ecp_failures_++;
  if (this->ecp_failures_ >= PN532_ECP_MAX_FAILURES) {
    if (this->ecp_failures_ == PN532_ECP_MAX_FAILURES)
      ESP_LOGW(TAG, "ECP step %d failed %u times in a row, continuing with tag polling", this->next_flow_,
               this->ecp_failures_);
    this->next_flow_ = 2;
  }
}

void PN532::update() {
  if (!this->updates_enabled_ || this->recovering_ || this->health_step_ != HC_IDLE)
    return;

  // Never start a new command while the previous one is still being answered;
  // the second frame would interleave with the first response.
  if (this->requested_read_ || this->requested_ecp_) {
    ESP_LOGV(TAG, "Previous request still pending, skipping this poll");
    return;
  }

  for (auto *obj : this->binary_sensors_)
    obj->on_scan_end();

  // Enhanced Contactless Polling sequence
  if (this->ecp_frame.size() > 0) {
    if (this->next_flow_ == 0) {
      if (!this->write_command_({
              PN532_COMMAND_WRITEREGISTER,
              (0x633d >> 8) & 0xFF,
              0x633d & 0xFF,
              0x0
        })) {
        this->poll_command_failed_("Setting 8bit TX failed!");
        this->ecp_step_failed_();
        return;
      }
      this->command_failures_ = 0;
      if (this->healthy_)
        this->status_clear_warning();
      this->requested_ecp_ = true;
      return;
    }
    else if (this->next_flow_ == 1) {
      std::vector<uint8_t> frame_send{ PN532_COMMAND_INCOMMUNICATETHRU };
      frame_send.insert(frame_send.end(), ecp_frame.begin(), ecp_frame.end());
      if (!this->write_command_(frame_send)) {
        this->poll_command_failed_("Sending ECP Frame failed!");
        this->ecp_step_failed_();
        return;
      }
      this->command_failures_ = 0;
      if (this->healthy_)
        this->status_clear_warning();
      this->requested_ecp_ = true;
      return;
    }
  }
  // End of ECP sequence

  if (!this->write_command_({
          PN532_COMMAND_INLISTPASSIVETARGET,
          0x01,  // max 1 card
          0x00,  // baud rate ISO14443A (106 kbit/s)
      })) {
    this->poll_command_failed_("Requesting tag read failed!");
    this->next_flow_ = 0;
    return;
  }
  this->command_failures_ = 0;
  if (this->healthy_)  // keep the watchdog's warning until a health check passes
    this->status_clear_warning();
  this->requested_read_ = true;
}

void PN532::loop() {
  if (this->recovering_)
    return;  // a timed recovery sequence is running
  if (this->health_step_ != HC_IDLE) {
    // One bus transaction per loop() call keeps each blocking slice short.
    this->health_check_step_();
    return;
  }
  if (this->is_idle_()) {
    const uint32_t now = millis();
    if (this->preventive_reinit_interval_ > 0 && now - this->last_reinit_ >= this->preventive_reinit_interval_) {
      this->preventive_reinit_();
      return;
    }
    if (this->health_check_interval_ > 0 && static_cast<int32_t>(now - this->next_health_check_) >= 0) {
      this->start_health_check_();
      return;
    }
  }

  // Enhanced Contactless Polling sequence
  if (this->requested_ecp_ && (this->next_flow_ == 0 || this->next_flow_ == 1)) {
    auto ready = this->read_ready_(false);
    if (ready == WOULDBLOCK)
      return;

    if (ready == READY) {
      std::vector<uint8_t> read;
      this->read_response(this->next_flow_ == 0 ? PN532_COMMAND_WRITEREGISTER : PN532_COMMAND_INCOMMUNICATETHRU,
                          read);
      ESP_LOGV(TAG, "ECP: %s", format_hex(read).c_str());
      if (this->ecp_failures_ >= PN532_ECP_MAX_FAILURES)
        ESP_LOGI(TAG, "ECP step %d answered again", this->next_flow_);
      this->ecp_failures_ = 0;
      this->next_flow_++;
    } else {
      this->send_ack_();  // abort the step that never became ready
      this->ecp_step_failed_();
    }
    this->requested_ecp_ = false;
    return;
  }
  // End of ECP sequence
  if (!this->requested_read_)
    return;

  auto ready = this->read_ready_(false);
  if (ready == WOULDBLOCK)
    return;

  bool success = false;
  std::vector<uint8_t> read;

  if (ready == READY) {
    success = this->read_response(PN532_COMMAND_INLISTPASSIVETARGET, read) && !read.empty();
  } else {
    this->send_ack_();  // abort still running InListPassiveTarget
  }

  this->requested_read_ = false;

  if (success) {
    this->last_poll_ok_ = millis();
    this->polls_ok_++;
    if (this->poll_failures_ >= PN532_POLL_FAILURES_WARN)
      ESP_LOGI(TAG, "Tag polling answered again after %" PRIu32 " failed polls", this->poll_failures_);
    this->poll_failures_ = 0;
  } else {
    // An error frame, a garbage frame or no answer at all used to be
    // indistinguishable from "no tag" at DEBUG level.
    this->poll_failures_++;
    if (this->poll_failures_ == PN532_POLL_FAILURES_WARN || this->poll_failures_ % PN532_POLL_FAILURES_REPEAT == 0)
      ESP_LOGW(TAG, "Tag polling failed %" PRIu32 " times in a row (last: %s)", this->poll_failures_,
               ready == READY ? "error or invalid frame" : "no answer");
    if (this->poll_failures_ == PN532_POLL_FAILURES_WARN)
      this->request_health_check_();
  }

  if (!success) {
    // Something failed
    if (!this->current_uid_.empty()) {
      auto tag = make_unique<nfc::NfcTag>(this->current_uid_);
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(tag);
    }
    this->current_uid_ = {};
    // this->turn_off_rf_();
    this->next_flow_ = 0;
    return;
  }

  uint8_t num_targets = read[0];
  if (num_targets != 1) {
    this->target_still_present = false;
    // no tags found or too many
    if (!this->current_uid_.empty()) {
      auto tag = make_unique<nfc::NfcTag>(this->current_uid_);
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(tag);
    }
    this->current_uid_ = {};
    // this->turn_off_rf_();
    this->next_flow_ = 0;
    return;
  } else {
    if (this->target_still_present) {
      this->current_uid_ = {};
      this->next_flow_ = 0;
      ESP_LOGD(TAG, "Tag still present");
      return;
    }
    this->target_still_present = true;
  }

  if (read.size() < 6U || read[5] > nfc::NFC_UID_MAX_LENGTH || read.size() < 6U + read[5]) {
    // oops, pn532 returned invalid data
    ESP_LOGW(TAG, "Invalid InListPassiveTarget response: %s", format_hex_pretty(read).c_str());
    this->target_still_present = false;
    this->next_flow_ = 0;
    return;
  }
  uint8_t nfcid_length = read[5];
  nfc::NfcTagUid nfcid(read.begin() + 6, read.begin() + 6 + nfcid_length);

  bool report = true;
  for (auto *bin_sens : this->binary_sensors_) {
    if (bin_sens->process(nfcid)) {
      report = false;
    }
  }

  if (nfcid.size() == this->current_uid_.size()) {
    bool same_uid = true;
    for (size_t i = 0; i < nfcid.size(); i++)
      same_uid &= nfcid[i] == this->current_uid_[i];
    if (same_uid)
      return;
  }

  this->current_uid_ = nfcid;

  if (next_task_ == READ) {
    auto tag = this->read_tag_(nfcid);
    for (auto *trigger : this->triggers_ontag_)
      trigger->process(tag);

    if (report) {
      char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
      ESP_LOGD(TAG, "Found new tag '%s'", nfc::format_uid_to(uid_buf, nfcid));
      if (tag->has_ndef_message()) {
        const auto &message = tag->get_ndef_message();
        const auto &records = message->get_records();
        ESP_LOGD(TAG, "  NDEF formatted records:");
        for (const auto &record : records) {
          ESP_LOGD(TAG, "    %s - %s", record->get_type().c_str(), record->get_payload().c_str());
        }
      }
    }
  } else if (next_task_ == CLEAN) {
    ESP_LOGD(TAG, "  Tag cleaning");
    if (!this->clean_tag_(nfcid)) {
      ESP_LOGE(TAG, "  Tag was not fully cleaned successfully");
    }
    ESP_LOGD(TAG, "  Tag cleaned!");
  } else if (next_task_ == FORMAT) {
    ESP_LOGD(TAG, "  Tag formatting");
    if (!this->format_tag_(nfcid)) {
      ESP_LOGE(TAG, "Error formatting tag as NDEF");
    }
    ESP_LOGD(TAG, "  Tag formatted!");
  } else if (next_task_ == WRITE) {
    if (this->next_task_message_to_write_ != nullptr) {
      ESP_LOGD(TAG, "  Tag writing");
      ESP_LOGD(TAG, "  Tag formatting");
      if (!this->format_tag_(nfcid)) {
        ESP_LOGE(TAG, "  Tag could not be formatted for writing");
      } else {
        ESP_LOGD(TAG, "  Writing NDEF data");
        if (!this->write_tag_(nfcid, this->next_task_message_to_write_)) {
          ESP_LOGE(TAG, "  Failed to write message to tag");
        }
        ESP_LOGD(TAG, "  Finished writing NDEF data");
        delete this->next_task_message_to_write_;
        this->next_task_message_to_write_ = nullptr;
        this->on_finished_write_callback_.call();
      }
    }
  }

  this->read_mode();

  this->turn_off_rf_();

  this->next_flow_ = 0;
}

// ---------------------------------------------------------------------------
// Watchdog: periodic health check and escalating recovery
// ---------------------------------------------------------------------------

uint32_t PN532::get_last_poll_ok_age() const { return millis() - this->last_poll_ok_; }

bool PN532::is_idle_() const {
  return this->updates_enabled_ && !this->recovering_ && !this->requested_read_ && !this->requested_ecp_ &&
         this->next_flow_ == 0 && !this->target_still_present && this->current_uid_.empty();
}

void PN532::reset_poll_state_() {
  if (!this->current_uid_.empty()) {
    auto tag = make_unique<nfc::NfcTag>(this->current_uid_);
    for (auto *trigger : this->triggers_ontagremoved_)
      trigger->process(tag);
  }
  this->current_uid_ = {};
  this->target_still_present = false;
  this->requested_read_ = false;
  this->requested_ecp_ = false;
  this->next_flow_ = 0;
  this->rd_start_time_.reset();
  this->rd_ready_ = WOULDBLOCK;
  this->ecp_failures_ = 0;
  this->poll_failures_ = 0;
  this->command_failures_ = 0;
  this->health_step_ = HC_IDLE;
}

void PN532::request_health_check_() {
  // Bring the next health check forward (it still only runs while idle).
  if (this->health_check_interval_ > 0)
    this->next_health_check_ = millis();
}

void PN532::start_health_check_() {
  this->hc_ok_ = true;
  this->hc_reason_ = "";
  this->hc_fw_[0] = this->hc_fw_[1] = 0;
  for (auto &reg : this->hc_regs_)
    reg = 0;
  this->hc_tx_on_ = 0;
  this->health_step_ = HC_VERSION;
}

void PN532::health_check_fail_(const char *reason) {
  if (this->hc_ok_) {
    this->hc_ok_ = false;
    this->hc_reason_ = reason;
  }
}

bool PN532::read_registers_(const std::vector<uint16_t> &addresses, std::vector<uint8_t> &values) {
  std::vector<uint8_t> command{PN532_COMMAND_READREGISTER};
  for (uint16_t address : addresses) {
    command.push_back(address >> 8);
    command.push_back(address & 0xFF);
  }
  values.clear();
  if (!this->write_command_(command) || !this->read_response(PN532_COMMAND_READREGISTER, values))
    return false;
  // PN533-style firmware prepends a status byte; PN532 v1.6 returns just the values.
  if (values.size() == addresses.size() + 1)
    values.erase(values.begin());
  return values.size() == addresses.size();
}

void PN532::health_check_step_() {
  switch (this->health_step_) {
    case HC_IDLE:
      return;
    case HC_VERSION: {
      std::vector<uint8_t> version;
      if (!this->write_command_({PN532_COMMAND_VERSION_DATA}) ||
          !this->read_response(PN532_COMMAND_VERSION_DATA, version) || version.size() < 3) {
        this->health_check_fail_("no answer to GetFirmwareVersion");
        this->health_step_ = HC_EVALUATE;  // chip is not talking, skip the rest
        return;
      }
      this->hc_fw_[0] = version[1];
      this->hc_fw_[1] = version[2];
      if (version[0] != this->ic_version_) {
        this->health_check_fail_("unexpected IC version in GetFirmwareVersion");
        this->health_step_ = HC_EVALUATE;
        return;
      }
      this->health_step_ = HC_REGISTERS;
      return;
    }
    case HC_REGISTERS: {
      std::vector<uint8_t> regs;
      if (!this->read_registers_({PN532_REG_CIU_TXCONTROL, PN532_REG_CIU_ERROR, PN532_REG_CIU_STATUS1,
                                  PN532_REG_CIU_STATUS2},
                                 regs)) {
        this->health_check_fail_("ReadRegister failed");
        this->health_step_ = HC_EVALUATE;
        return;
      }
      for (size_t i = 0; i < 4; i++)
        this->hc_regs_[i] = regs[i];
      if (regs[1] & PN532_CIU_ERROR_TEMPERR)
        this->health_check_fail_("CIU temperature error (TempErr), antenna drivers shut down");
      this->health_step_ = HC_RF_ON;
      return;
    }
    case HC_RF_ON:
      // Between polls the field is off, so TxControl alone says nothing. Switch the
      // field on and check that the antenna drivers actually follow.
      if (!this->rf_configure_({PN532_RFCFG_FIELD, PN532_RF_AUTO_RFCA | PN532_RF_FIELD_ON})) {
        this->health_check_fail_("RFConfiguration (field on) failed");
        this->health_step_ = HC_RF_OFF;
        return;
      }
      this->health_step_ = HC_RF_READ;
      return;
    case HC_RF_READ: {
      std::vector<uint8_t> regs;
      if (!this->read_registers_({PN532_REG_CIU_TXCONTROL}, regs)) {
        this->health_check_fail_("ReadRegister (field on) failed");
      } else {
        this->hc_tx_on_ = regs[0];
        // With an external field present Auto RFCA legitimately keeps our field off.
        bool external_field = this->hc_regs_[2] & PN532_CIU_STATUS1_RFON;
        if ((regs[0] & PN532_CIU_TXCONTROL_DRIVERS) != PN532_CIU_TXCONTROL_DRIVERS && !external_field)
          this->health_check_fail_("antenna drivers did not switch on (CIU_TxControl)");
      }
      this->health_step_ = HC_RF_OFF;
      return;
    }
    case HC_RF_OFF:
      if (!this->turn_off_rf_())
        this->health_check_fail_("RFConfiguration (field off) failed");
      this->health_step_ = HC_EVALUATE;
      return;
    case HC_EVALUATE:
      this->finish_health_check_();
      return;
  }
}

void PN532::finish_health_check_() {
  this->health_step_ = HC_IDLE;
  const uint32_t now = millis();
  const uint32_t poll_age = now - this->last_poll_ok_;

  if (this->hc_ok_ && this->max_poll_stall_ > 0) {
    if (this->verify_level_ > 0) {
      // Right after a recovery: at least one tag poll must have succeeded since.
      if (this->polls_ok_ == this->polls_ok_at_recovery_)
        this->health_check_fail_("no successful tag poll since re-initialisation");
    } else {
      const uint32_t stall_limit = std::max(this->max_poll_stall_, 3 * this->get_update_interval());
      if (poll_age > stall_limit)
        this->health_check_fail_("tag polling stalled");
    }
  }

  ESP_LOGD(TAG,
           "Health check: fw %u.%u, TxControl 0x%02X (field on 0x%02X), Error 0x%02X, Status1 0x%02X, "
           "Status2 0x%02X, last poll ok %" PRIu32 " ms ago, %" PRIu32 " failed polls in a row",
           this->hc_fw_[0], this->hc_fw_[1], this->hc_regs_[0], this->hc_tx_on_, this->hc_regs_[1],
           this->hc_regs_[2], this->hc_regs_[3], poll_age, this->poll_failures_);

  this->health_check_count_++;
  this->next_health_check_ = now + this->health_check_interval_;

  if (this->hc_ok_) {
    this->healthy_ = true;
    this->status_clear_warning();
    if (this->verify_level_ > 0) {
      ESP_LOGI(TAG, "PN532 healthy again, recovered at level %u", this->verify_level_);
      this->verify_level_ = 0;
    }
    if (this->escalation_level_ > 0 && ++this->healthy_streak_ >= PN532_HEALTHY_CHECKS_TO_RESET) {
      ESP_LOGD(TAG, "Stable for %u health checks, recovery escalation starts over at level 1", this->healthy_streak_);
      this->escalation_level_ = 0;
      this->healthy_streak_ = 0;
    }
    return;
  }

  ESP_LOGW(TAG,
           "PN532 unhealthy: %s (TxControl 0x%02X/0x%02X, Error 0x%02X, Status1 0x%02X, last poll ok %" PRIu32
           " ms ago)",
           this->hc_reason_, this->hc_regs_[0], this->hc_tx_on_, this->hc_regs_[1], this->hc_regs_[2], poll_age);
  if (this->verify_level_ > 0)
    ESP_LOGW(TAG, "Recovery level %u did not restore the reader", this->verify_level_);
  this->healthy_ = false;
  this->healthy_streak_ = 0;
  this->status_set_warning();
  this->recover_(this->next_recovery_level_());
}

uint8_t PN532::next_recovery_level_() const {
  auto available = [this](uint8_t level) {
    return level <= 2 || (level == 3 && this->reset_pin_ != nullptr) || (level == 4 && this->power_pin_ != nullptr);
  };
  for (uint8_t level = this->escalation_level_ + 1; level <= PN532_RECOVERY_MAX_LEVEL; level++) {
    if (available(level))
      return level;
  }
  // Already at the highest configured level: repeat it.
  for (uint8_t level = PN532_RECOVERY_MAX_LEVEL; level > 1; level--) {
    if (available(level))
      return level;
  }
  return 1;
}

void PN532::reinit() {
  if (this->recovering_) {
    ESP_LOGW(TAG, "Re-initialisation requested, but a recovery is already running");
    return;
  }
  ESP_LOGW(TAG, "Re-initialisation requested");
  this->recover_(1);
}

void PN532::recover_(uint8_t level) {
  this->recovering_ = true;
  // Verify quickly only when trying something new; repeating the highest level
  // (e.g. a reader that is gone for good) waits a full health check interval.
  this->fast_verify_ = level > this->escalation_level_;
  this->escalation_level_ = level;
  this->recovery_count_++;
  this->reset_poll_state_();

  switch (level) {
    case 2: {
      ESP_LOGW(TAG, "Recovery level 2: PowerDown + wake-up, then re-initialise");
      std::vector<uint8_t> response;
      if (!this->write_command_({PN532_COMMAND_POWERDOWN, 0b10100000}) ||  // wake-up on I2C/SPI
          !this->read_response(PN532_COMMAND_POWERDOWN, response))
        ESP_LOGW(TAG, "Recovery level 2: PowerDown not answered, waking up anyway");
      this->set_timeout("pn532_recover", 10, [this]() {
        this->wake_chip_();
        this->set_timeout("pn532_recover", 20, [this]() { this->finish_recovery_(2); });
      });
      return;
    }
    case 3:
      ESP_LOGW(TAG, "Recovery level 3: hardware reset via reset_pin, then re-initialise");
      this->reset_pin_->digital_write(false);
      this->set_timeout("pn532_recover", PN532_RESET_PULSE_MS, [this]() {
        this->reset_pin_->digital_write(true);
        this->set_timeout("pn532_recover", PN532_RESET_SETTLE_MS, [this]() {
          this->wake_chip_();
          this->finish_recovery_(3);
        });
      });
      return;
    case 4:
      ESP_LOGW(TAG, "Recovery level 4: power cycle via power_pin (%" PRIu32 " ms off), then re-initialise",
               PN532_POWER_OFF_MS);
      // Best effort against back-feeding: drive CS (and RSTPDN) low while the chip is unpowered.
      this->set_bus_power_off_(true);
      if (this->reset_pin_ != nullptr)
        this->reset_pin_->digital_write(false);
      this->power_pin_->digital_write(false);
      this->set_timeout("pn532_recover", PN532_POWER_OFF_MS, [this]() {
        this->power_pin_->digital_write(true);
        if (this->reset_pin_ != nullptr)
          this->reset_pin_->digital_write(true);
        this->set_bus_power_off_(false);
        this->set_timeout("pn532_recover", PN532_POWER_SETTLE_MS, [this]() {
          this->wake_chip_();
          this->finish_recovery_(4);
        });
      });
      return;
    default:
      ESP_LOGW(TAG, "Recovery level 1: soft re-initialisation");
      this->finish_recovery_(1);
      return;
  }
}

void PN532::finish_recovery_(uint8_t level) {
  bool ok = this->init_chip_();
  this->reset_poll_state_();
  this->recovering_ = false;

  const uint32_t now = millis();
  this->last_poll_ok_ = now;
  this->last_reinit_ = now;
  this->polls_ok_at_recovery_ = this->polls_ok_;
  this->verify_level_ = level;

  if (ok) {
    ESP_LOGW(TAG, "Recovery level %u: chip re-initialised, verifying", level);
  } else {
    ESP_LOGW(TAG, "Recovery level %u: re-initialisation failed", level);
    this->status_set_warning();
  }

  if (this->health_check_interval_ > 0) {
    uint32_t verify_delay = this->health_check_interval_;
    if (this->fast_verify_)
      verify_delay = std::min(verify_delay, std::max(PN532_VERIFY_DELAY_MIN_MS, 5 * this->get_update_interval()));
    this->next_health_check_ = now + verify_delay;
  }

  this->on_recovery_callback_.call(level);
}

void PN532::preventive_reinit_() {
  ESP_LOGI(TAG, "Preventive re-initialisation");
  this->reset_poll_state_();
  bool ok = this->init_chip_();
  const uint32_t now = millis();
  this->last_poll_ok_ = now;
  this->last_reinit_ = now;
  if (!ok) {
    ESP_LOGW(TAG, "Preventive re-initialisation failed");
    this->status_set_warning();
    this->request_health_check_();
  }
}

std::vector<uint8_t> PN532::inDataExchange(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> dataToSend{ PN532_COMMAND_INDATAEXCHANGE, 0x01 };
  dataToSend.insert(dataToSend.end(), data.begin(), data.end());
  if (!this->write_command_(dataToSend)) {
    ESP_LOGW(TAG, "Error sending InDataExchange command, trying again...");
    if (!this->write_command_(dataToSend)) {
      ESP_LOGE(TAG, "Error sending InDataExchange command");
      // this->mark_failed();
      return std::vector<uint8_t>();
    }
  }
  int timeout = 0;
  while (this->read_ready_(true) != pn532::PN532ReadReady::READY) { timeout++; delay(1); if (timeout > 250) break; }

  std::vector<uint8_t> buffer(0);
  if (!this->read_response(PN532_COMMAND_INDATAEXCHANGE, buffer)) {
    ESP_LOGE(TAG, "Error getting response");
    // this->mark_failed();
    return std::vector<uint8_t>();
  }
  return buffer;
}

bool PN532::write_command_(const std::vector<uint8_t>& data) {
  std::vector<uint8_t> write_data;
  // Preamble
  write_data.push_back(0x00);

  // Start code
  write_data.push_back(0x00);
  write_data.push_back(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  write_data.push_back(real_length);
  // LCS (Length checksum)
  write_data.push_back(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  write_data.push_back(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    write_data.push_back(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  write_data.push_back(~checksum + 1);
  // Postamble
  write_data.push_back(0x00);

  this->write_data(write_data);

  return this->read_ack_();
}

bool PN532::read_ack_() {
  ESP_LOGV(TAG, "Reading ACK");

  std::vector<uint8_t> data;
  if (!this->read_data(data, 6)) {
    return false;
  }

  bool matches = (data[1] == 0x00 &&                     // preamble
                  data[2] == 0x00 &&                     // start of packet
                  data[3] == 0xFF && data[4] == 0x00 &&  // ACK packet code
                  data[5] == 0xFF && data[6] == 0x00);   // postamble
  ESP_LOGV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

void PN532::send_ack_() {
  ESP_LOGV(TAG, "Sending ACK for abort");
  this->write_data({0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00});
  delay(10);
}
void PN532::send_nack_() {
  ESP_LOGV(TAG, "Sending NACK for retransmit");
  this->write_data({0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00});
  delay(10);
}

enum PN532ReadReady PN532::read_ready_(bool block) {
  if (this->rd_ready_ == READY) {
    if (block) {
      this->rd_start_time_.reset();
      this->rd_ready_ = WOULDBLOCK;
    }
    return READY;
  }

  if (!this->rd_start_time_.has_value()) {
    this->rd_start_time_ = millis();
  }

  while (true) {
    if (this->is_read_ready()) {
      this->rd_ready_ = READY;
      break;
    }

    if (millis() - *this->rd_start_time_ > 100) {
      ESP_LOGV(TAG, "Timed out waiting for readiness from PN532!");
      this->rd_ready_ = TIMEOUT;
      break;
    }

    if (!block) {
      this->rd_ready_ = WOULDBLOCK;
      break;
    }

    yield();
  }

  auto rdy = this->rd_ready_;
  if (block || rdy == TIMEOUT) {
    this->rd_start_time_.reset();
    this->rd_ready_ = WOULDBLOCK;
  }
  return rdy;
}

bool PN532::rf_configure_(const std::vector<uint8_t> &config) {
  std::vector<uint8_t> command{PN532_COMMAND_RFCONFIGURATION};
  command.insert(command.end(), config.begin(), config.end());
  if (!this->write_command_(command))
    return false;
  // Always consume the response frame; leaving it unread makes the next
  // command's readiness check see this stale frame instead.
  std::vector<uint8_t> response;
  return this->read_response(PN532_COMMAND_RFCONFIGURATION, response);
}

bool PN532::turn_off_rf_() {
  ESP_LOGV(TAG, "Turning RF field OFF");
  // Field off, keep Auto RFCA enabled.
  if (!this->rf_configure_({PN532_RFCFG_FIELD, PN532_RF_AUTO_RFCA})) {
    ESP_LOGW(TAG, "Error turning RF field off");
    return false;
  }
  return true;
}

std::unique_ptr<nfc::NfcTag> PN532::read_tag_(nfc::NfcTagUid &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());

  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    ESP_LOGD(TAG, "Mifare classic");
    return this->read_mifare_classic_tag_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    ESP_LOGD(TAG, "Mifare ultralight");
    return this->read_mifare_ultralight_tag_(uid);
  } else if (type == nfc::TAG_TYPE_UNKNOWN) {
    ESP_LOGV(TAG, "Cannot determine tag type");
    return make_unique<nfc::NfcTag>(uid);
  } else {
    return make_unique<nfc::NfcTag>(uid);
  }
}

void PN532::read_mode() {
  this->next_task_ = READ;
  ESP_LOGD(TAG, "Waiting to read next tag");
}
void PN532::clean_mode() {
  this->next_task_ = CLEAN;
  ESP_LOGD(TAG, "Waiting to clean next tag");
}
void PN532::format_mode() {
  this->next_task_ = FORMAT;
  ESP_LOGD(TAG, "Waiting to format next tag");
}
void PN532::write_mode(nfc::NdefMessage *message) {
  this->next_task_ = WRITE;
  this->next_task_message_to_write_ = message;
  ESP_LOGD(TAG, "Waiting to write next tag");
}

bool PN532::clean_tag_(nfc::NfcTagUid &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_mifare_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::format_tag_(nfc::NfcTagUid &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->format_mifare_classic_ndef_(uid);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->clean_mifare_ultralight_();
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

bool PN532::write_tag_(nfc::NfcTagUid &uid, nfc::NdefMessage *message) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    return this->write_mifare_classic_tag_(uid, message);
  } else if (type == nfc::TAG_TYPE_2) {
    return this->write_mifare_ultralight_tag_(uid, message);
  }
  ESP_LOGE(TAG, "Unsupported Tag for formatting");
  return false;
}

void PN532::dump_config() {
  ESP_LOGCONFIG(TAG, "PN532:");
  switch (this->error_code_) {
    case NONE:
      break;
    case WAKEUP_FAILED:
      ESP_LOGE(TAG, "Wake Up command failed!");
      break;
    case SAM_COMMAND_FAILED:
      ESP_LOGE(TAG, "SAM command failed!");
      break;
    case VERSION_FAILED:
      ESP_LOGE(TAG, "No firmware version response!");
      break;
    case RF_CONFIG_FAILED:
      ESP_LOGE(TAG, "RF configuration failed!");
      break;
  }

  LOG_UPDATE_INTERVAL(this);
  if (this->health_check_interval_ > 0) {
    ESP_LOGCONFIG(TAG, "  Health check interval: %" PRIu32 " ms, max poll stall: %" PRIu32 " ms",
                  this->health_check_interval_, this->max_poll_stall_);
  } else {
    ESP_LOGCONFIG(TAG, "  Health check: disabled");
  }
  if (this->preventive_reinit_interval_ > 0)
    ESP_LOGCONFIG(TAG, "  Preventive re-init interval: %" PRIu32 " ms", this->preventive_reinit_interval_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Power Pin: ", this->power_pin_);

  for (auto *child : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Tag", child);
  }
}

bool PN532BinarySensor::process(const nfc::NfcTagUid &data) {
  if (data.size() != this->uid_.size())
    return false;

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != this->uid_[i])
      return false;
  }

  this->publish_state(true);
  this->found_ = true;
  return true;
}

}  // namespace pn532
}  // namespace esphome
