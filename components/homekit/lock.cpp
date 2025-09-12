#ifdef USE_LOCK
#ifdef USE_HOMEKEY
#include <nvs.h>           // For nvs_handle
#include "../pn532/pn532.h" // For pn532::PN532
#endif
#include "lock.h"
// Required for std::ostringstream, std::setw, std::setfill
#include <sstream>
#include <iomanip>

namespace esphome {
namespace homekit {
#ifdef USE_HOMEKEY
readerData_t LockEntity::readerData;
nvs_handle LockEntity::savedHKdata;
pn532::PN532 *LockEntity::nfc_ctx;
#endif
#ifdef USE_HOMEKEY
int LockEntity::nfcAccess_write(hap_write_data_t write_data[], int count,
                                void *serv_priv, void *write_priv) {
  LockEntity *parent = (LockEntity *)serv_priv;
  LOG(I, "PROVISIONED READER KEY: %s",
      hk_utils::bufToHexString(parent->readerData.reader_sk.data(),
                               parent->readerData.reader_sk.size(), true)
          .c_str());
  LOG(I, "READER PUBLIC KEY: %s",
      hk_utils::bufToHexString(parent->readerData.reader_pk.data(),
                               parent->readerData.reader_pk.size(), true)
          .c_str());
  LOG(I, "READER GROUP IDENTIFIER: %s",
      hk_utils::bufToHexString(parent->readerData.reader_gid.data(),
                               parent->readerData.reader_gid.size(), true)
          .c_str());
  LOG(I, "READER UNIQUE IDENTIFIER: %s",
      hk_utils::bufToHexString(parent->readerData.reader_id.data(),
                               parent->readerData.reader_id.size(), true)
          .c_str());
  int i, ret = HAP_SUCCESS;
  hap_write_data_t *write;
  for (i = 0; i < count; i++) {
    write = &write_data[i];
    /* Setting a default error value */
    *(write->status) = HAP_STATUS_VAL_INVALID;
    if (!strcmp(hap_char_get_type_uuid(write->hc),
                HAP_CHAR_UUID_NFC_ACCESS_CONTROL_POINT)) {
      hap_tlv8_val_t buf = write->val.t;
      auto tlv_rx_data = std::vector<uint8_t>(buf.buf, buf.buf + buf.buflen);
      ESP_LOGD(
          TAG, "TLV RX DATA: %s SIZE: %d",
          hk_utils::bufToHexString(tlv_rx_data.data(), tlv_rx_data.size(), true)
              .c_str(),
          tlv_rx_data.size());
      HK_HomeKit ctx(parent->readerData, parent->savedHKdata, "READERDATA",
                     tlv_rx_data);
      auto result = ctx.processResult();
      memcpy(parent->tlv8_data, result.data(), result.size());
      // if (strlen((const char*)readerData.reader_group_id) > 0) {
      //   memcpy(ecpData + 8, readerData.reader_group_id,
      //   sizeof(readerData.reader_group_id)); with_crc16(ecpData, 16, ecpData
      //   + 16);
      // }
      hap_val_t new_val;
      new_val.t.buf = parent->tlv8_data;
      new_val.t.buflen = result.size();
      hap_char_update_val(write->hc, &new_val);
      *(write->status) = HAP_STATUS_SUCCESS;
    } else {
      *(write->status) = HAP_STATUS_RES_ABSENT;
    }
  }
  return ret;
}

void LockEntity::hap_event_handler(hap_event_t event, void *data) {
  if (event == HAP_EVENT_CTRL_PAIRED) {
    char *ctrl_id = (char *)data;
    hap_ctrl_data_t *ctrl = hap_get_controller_data(ctrl_id);
    if (ctrl->valid) {
      std::vector<uint8_t> id =
          hk_utils::getHashIdentifier(ctrl->info.ltpk, 32, true);
      ESP_LOG_BUFFER_HEX(TAG, ctrl->info.ltpk, 32);
      LOG(D, "Found allocated controller - Hash: %s",
          hk_utils::bufToHexString(id.data(), 8).c_str());
      hkIssuer_t *foundIssuer = nullptr;
      for (auto &issuer : readerData.issuers) {
        if (!memcmp(issuer.issuer_id.data(), id.data(), 8)) {
          LOG(D, "Issuer %s already added, skipping",
              hk_utils::bufToHexString(issuer.issuer_id.data(), 8).c_str());
          foundIssuer = &issuer;
          break;
        }
      }
      if (foundIssuer == nullptr) {
        LOG(D, "Adding new issuer - ID: %s",
            hk_utils::bufToHexString(id.data(), 8).c_str());
        hkIssuer_t issuer;
        issuer.issuer_id = id;
        issuer.issuer_pk.insert(issuer.issuer_pk.begin(), ctrl->info.ltpk,
                                ctrl->info.ltpk + 32);
        readerData.issuers.emplace_back(issuer);
        std::vector<uint8_t> data = nlohmann::json::to_msgpack(readerData);
        esp_err_t set_nvs =
            nvs_set_blob(savedHKdata, "READERDATA", data.data(), data.size());
        esp_err_t commit_nvs = nvs_commit(savedHKdata);
        LOG(I, "NVS SET STATUS: %s", esp_err_to_name(set_nvs));
        LOG(I, "NVS COMMIT STATUS: %s", esp_err_to_name(commit_nvs));
      }
    }
  } else if (event == HAP_EVENT_CTRL_UNPAIRED) {
    int ctrl_count = hap_get_paired_controller_count();
    if (ctrl_count == 0) {
      readerData = {};
      esp_err_t erase_nvs = nvs_erase_key(savedHKdata, "READERDATA");
      esp_err_t commit_nvs = nvs_commit(savedHKdata);
      LOG(D, "*** NVS W STATUS");
      LOG(D, "ERASE: %s", esp_err_to_name(erase_nvs));
      LOG(D, "COMMIT: %s", esp_err_to_name(commit_nvs));
      LOG(D, "*** NVS W STATUS");
    }
    // else {
    //   char* ctrl_id = (char*)data;
    //   hap_ctrl_data_t* ctrl = hap_get_controller_data(ctrl_id);
    //   if (ctrl->valid) {
    //   // readerData.issuers.erase(std::remove_if(readerData.issuers.begin(),
    //   readerData.issuers.end(),
    //   //   [ctrl](HomeKeyData_KeyIssuer x) {
    //   //     std::vector<uint8_t> id =
    //   hk_utils::getHashIdentifier(ctrl->info.ltpk, 32, true);
    //   //     LOG(D, "Found allocated controller - Hash: %s",
    //   hk_utils::bufToHexString(id.data(), 8).c_str());
    //   //     if (!memcmp(x.publicKey, id.data(), 8)) {
    //   //       return false;
    //   //     }
    //   //     LOG(D, "Issuer ID: %s - Associated controller was removed from
    //   Home, erasing from reader data.", hk_utils::bufToHexString(x.issuerId,
    //   8).c_str());
    //   //     return true;
    //   //   }),
    //   //   readerData.issuers.end());
    //   }
    // }
  }
}
#endif

void LockEntity::on_lock_update(lock::Lock *obj) {
  ESP_LOGD("on_lock_update", "%s state: %s", obj->get_name().c_str(),
           lock_state_to_string(obj->state));
  hap_acc_t *acc = hap_acc_get_by_aid(
      hap_get_unique_aid(std::to_string(obj->get_object_id_hash()).c_str()));
  hap_serv_t *hs = hap_acc_get_serv_by_uuid(acc, HAP_SERV_UUID_GARAGE_DOOR_OPENER);
  hap_char_t *current_state =
  hap_serv_get_char_by_uuid(hs, "0000000E-0000-1000-8000-0026BB765291");
  hap_char_t *target_state =
  hap_serv_get_char_by_uuid(hs, "00000032-0000-1000-8000-0026BB765291");
  hap_val_t c;
  hap_val_t t;
  if (obj->state == lock::LockState::LOCK_STATE_LOCKED ||
      obj->state == lock::LockState::LOCK_STATE_UNLOCKED) {
    c.i = obj->state % 2;
    t.i = obj->state % 2;
    hap_char_update_val(current_state, &c);
    hap_char_update_val(target_state, &t);
  } else if (obj->state == lock::LockState::LOCK_STATE_LOCKING ||
             obj->state == lock::LockState::LOCK_STATE_UNLOCKING) {
    t.i = (obj->state % 5) % 3;
    hap_char_update_val(target_state, &t);
  } else if (obj->state == lock::LockState::LOCK_STATE_JAMMED) {
    c.i = obj->state;
    hap_char_update_val(current_state, &c);
  }
  return;
}

int LockEntity::lock_write(hap_write_data_t write_data[], int count,
                           void *serv_priv, void *write_priv) {
  lock::Lock *lockPtr = (lock::Lock *)serv_priv;
  ESP_LOGD("lock_write", "Write called for Accessory '%s'(%s)",
           lockPtr->get_name().c_str(),
           std::to_string(lockPtr->get_object_id_hash()).c_str());
  int i, ret = HAP_SUCCESS;
  hap_write_data_t *write;
  for (i = 0; i < count; i++) {
    write = &write_data[i];
  if (!strcmp(hap_char_get_type_uuid(write->hc),
        "00000032-0000-1000-8000-0026BB765291")) {
      ESP_LOGD("lock_write", "Target State req: %d", write->val.i);
      hap_char_update_val(write->hc, &(write->val));
    hap_char_t *c = hap_serv_get_char_by_uuid(
      hap_char_get_parent(write->hc), "0000000E-0000-1000-8000-0026BB765291");
      ESP_LOGD("lock_write", "Current State: %d", hap_char_get_val(c)->i);
      hap_char_update_val(c, &(write->val));
      write->val.i ? lockPtr->lock() : lockPtr->unlock();
      *(write->status) = HAP_STATUS_SUCCESS;
    } else {
      *(write->status) = HAP_STATUS_RES_ABSENT;
    }
  }
  return ret;
}

int LockEntity::acc_identify(hap_acc_t *ha) {
  ESP_LOGI(TAG, "Accessory identified");
  return HAP_SUCCESS;
}

// Function to calculate CRC16
void crc16a(unsigned char *data, unsigned int size, unsigned char *result) {
  unsigned short w_crc = 0x6363;

  for (unsigned int i = 0; i < size; ++i) {
    unsigned char byte = data[i];
    byte = (byte ^ (w_crc & 0x00FF));
    byte = ((byte ^ (byte << 4)) & 0xFF);
    w_crc = ((w_crc >> 8) ^ (byte << 8) ^ (byte << 3) ^ (byte >> 4)) & 0xFFFF;
  }

  result[0] = static_cast<unsigned char>(w_crc & 0xFF);
  result[1] = static_cast<unsigned char>((w_crc >> 8) & 0xFF);
}

// Function to append CRC16 to data
void with_crc16(unsigned char *data, unsigned int size, unsigned char *result) {
  crc16a(data, size, result);
}

LockEntity::LockEntity(lock::Lock *lockPtr)
    : HAPEntity({{MODEL, "HAP-LOCK"}}), ptrToLock(lockPtr) {
#ifdef USE_HOMEKEY
  ecpData.resize(18);
  auto t = nvs_open("HK_DATA", NVS_READWRITE, &savedHKdata);
  LOG(D, "NVS_OPEN: %s", esp_err_to_name(t));
  size_t len = 0;
  if (!nvs_get_blob(savedHKdata, "READERDATA", NULL, &len)) {
    std::vector<uint8_t> savedBuf(len);
    nvs_get_blob(savedHKdata, "READERDATA", savedBuf.data(), &len);
    LOG(D, "NVS DATA LENGTH: %d", len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, savedBuf.data(), savedBuf.size(),
                             ESP_LOG_VERBOSE);
    // try {
    nlohmann::json data = nlohmann::json::from_msgpack(savedBuf);
    data.get_to<readerData_t>(readerData);
    // }
    // catch (const std::exception& e) {
    //   std::cerr << e.what() << '\n';
    // }
  }
  LOG(D, "PROVISIONED READER KEY: %s",
      hk_utils::bufToHexString(readerData.reader_sk.data(),
                               readerData.reader_sk.size(), true)
          .c_str());
  LOG(D, "READER PUBLIC KEY: %s",
      hk_utils::bufToHexString(readerData.reader_pk.data(),
                               readerData.reader_pk.size(), true)
          .c_str());
  LOG(D, "READER GROUP IDENTIFIER: %s",
      hk_utils::bufToHexString(readerData.reader_gid.data(),
                               readerData.reader_gid.size(), true)
          .c_str());
  LOG(D, "READER UNIQUE IDENTIFIER: %s",
      hk_utils::bufToHexString(readerData.reader_id.data(),
                               readerData.reader_id.size(), true)
          .c_str());
  LOG(D, "ISSUERS COUNT: %d", readerData.issuers.size());
  memcpy(ecpData.data() + 8, readerData.reader_gid.data(),
         readerData.reader_gid.size());
  with_crc16(ecpData.data(), 16, ecpData.data() + 16);
#endif
}
std::string intToFinishString(HKFinish d) {
  switch (d) {
  case TAN:
    return "TAN";
    break;
  case GOLD:
    return "GOLD";
    break;
  case SILVER:
    return "SILVER";
    break;
  case BLACK:
    return "BLACK";
    break;
  default:
    return "UNKNOWN";
    break;
  }
}
std::string hex_representation(const std::vector<uint8_t> &v) {
  std::string hex_tmp;
  for (auto x : v) {
  std::ostringstream oss;
  oss << std::hex << std::setw(2) << std::setfill('0') << (unsigned)x;
    hex_tmp += oss.str();
  }
  return hex_tmp;
}

#ifdef USE_HOMEKEY
void LockEntity::register_onhk_trigger(HKAuthTrigger *trig) {
  triggers_onhk_.push_back(trig);
}
void LockEntity::register_onhkfail_trigger(HKFailTrigger *trig) {
  triggers_onhk_fail_.push_back(trig);
}
void LockEntity::set_hk_hw_finish(HKFinish color) {
  ESP_LOGI(TAG, "SELECTED HK FINISH: %s", intToFinishString(color).c_str());
  hap_tlv8_val_t tlvData = {.buf = hk_color_vals[color].data(),
                            .buflen = hk_color_vals[color].size()};
  hkFinishTlvData = std::make_unique<hap_tlv8_val_t>(tlvData);
}
void LockEntity::set_nfc_ctx(pn532::PN532 *ctx) {
  ctx->set_ecp_frame(ecpData);
  nfc_ctx = ctx;
  auto trigger = new nfc::NfcOnTagTrigger();
  ctx->register_ontag_trigger(trigger);
  auto automation_id_3 = new Automation<std::string, nfc::NfcTag>(trigger);
  auto lambdaaction_id_3 = new LambdaAction<std::string, nfc::NfcTag>(
      [this, ctx](std::string x, nfc::NfcTag tag) -> void {
        std::function<bool(uint8_t *, uint8_t, uint8_t *, uint16_t *, bool)>
            lambda = [=](uint8_t *send, uint8_t sendLen, uint8_t *res,
                         uint16_t *resLen, bool ignoreLog) -> bool {
          auto data =
              ctx->inDataExchange(std::vector<uint8_t>(send, send + sendLen));
          data.erase(data.begin());
          ESP_LOGD(TAG, "%s", format_hex_pretty(data).c_str());
          memcpy(res, data.data(), data.size());
          uint16_t t = data.size();
          memcpy(resLen, &t, sizeof(uint16_t));
          return true;
        };
        auto versions =
            ctx->inDataExchange({0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00,
                                 0x08, 0x58, 0x01, 0x01, 0x0});
        if (versions.size() > 0) {
          ESP_LOGD(TAG, "HK SUPPORTED VERSIONS: %s",
                   format_hex_pretty(versions).c_str());
          if (versions.data()[versions.size() - 2] == 0x90 &&
              versions.data()[versions.size() - 1] == 0x0) {
            HKAuthenticationContext authCtx(lambda, readerData, savedHKdata);
            auto authResult = authCtx.authenticate(KeyFlow(kFlowFAST));
            if (std::get<0>(authResult).size() > 0 &&
                std::get<2>(authResult) != kFlowFailed) {
              for (auto &&t : triggers_onhk_) {
                t->process(hex_representation(std::get<0>(authResult)),
                           hex_representation(std::get<1>(authResult)));
              }
            } else {
              for (auto &&t : triggers_onhk_fail_) {
                t->process();
              }
            }
          } else {
            for (auto &&t : triggers_onhk_fail_) {
              t->process();
            }
            ESP_LOGE(TAG, "Invalid response for HK");
          }
        } else {
          ESP_LOGW(TAG, "Target probably not Homekey");
        }
      });
  automation_id_3->add_actions({lambdaaction_id_3});
}
#endif

void LockEntity::setup() {
  hap_acc_cfg_t acc_cfg = {
      .model = strdup(accessory_info[MODEL]),
      .manufacturer = strdup(accessory_info[MANUFACTURER]),
      .fw_rev = strdup(accessory_info[FW_REV]),
      .hw_rev = NULL,
      .hw_finish = hkFinishTlvData.get(),
      .pv = strdup("1.1.0"),
      .cid = HAP_CID_BRIDGE,
      .identify_routine = acc_identify,
  };
  hap_acc_t *accessory = nullptr;
  hap_serv_t *lockMechanism = nullptr;
  std::string accessory_name = ptrToLock->get_name();
  if (accessory_info[NAME] == NULL) {
    acc_cfg.name = strdup(accessory_name.c_str());
  } else {
    acc_cfg.name = strdup(accessory_info[NAME]);
  }
  if (accessory_info[SN] == NULL) {
    acc_cfg.serial_num =
        strdup(std::to_string(ptrToLock->get_object_id_hash()).c_str());
  } else {
    acc_cfg.serial_num = strdup(accessory_info[SN]);
  }
  accessory = hap_acc_create(&acc_cfg);
  lockMechanism =
      hap_serv_lock_mechanism_create(ptrToLock->state, ptrToLock->state);

  ESP_LOGD(TAG, "ID HASH: %lu", ptrToLock->get_object_id_hash());
  hap_serv_set_priv(lockMechanism, ptrToLock);

  /* Set the write callback for the service */
  hap_serv_set_write_cb(lockMechanism, lock_write);

  /* Add the Lock Service to the Accessory Object */
  hap_acc_add_serv(accessory, lockMechanism);

#ifdef USE_HOMEKEY
  hap_register_event_handler(hap_event_handler);
  hap_serv_t *nfcAccess = nullptr;
  hap_serv_t *lockManagement = nullptr;
  nfcAccess = hap_serv_nfc_access_create(0, &management, &nfcSupportedConf);
  lockManagement =
      hap_serv_lock_management_create(&management, strdup("1.0.0"));
  hap_serv_set_priv(nfcAccess, this);
  hap_serv_set_write_cb(nfcAccess, nfcAccess_write);
  hap_acc_add_serv(accessory, nfcAccess);
#endif

  /* Add the Accessory to the HomeKit Database */
  hap_add_bridged_accessory(
      accessory, hap_get_unique_aid(
                     std::to_string(ptrToLock->get_object_id_hash()).c_str()));
  if (!ptrToLock->is_internal())
    ptrToLock->add_on_state_callback(
        [this]() { LockEntity::on_lock_update(ptrToLock); });

  ESP_LOGI(TAG, "Lock '%s' linked to HomeKit", accessory_name.c_str());
}
} // namespace homekit
} // namespace esphome
#endif
