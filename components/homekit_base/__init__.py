import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mdns, wifi, light, lock, sensor, switch
from esphome.const import CONF_PORT, PLATFORM_ESP32, CONF_ID
from esphome.core import ID, Lambda
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import re

DEPENDENCIES = ['esp32', 'network', 'mdns']
CODEOWNERS = ["@rednblkx"]
MULTI_CONF = True

homekit_ns = cg.esphome_ns.namespace('homekit')
HAPRootComponent = homekit_ns.class_('HAPRootComponent', cg.Component)
AInfo = homekit_ns.enum("AInfo")
CONF_HAP_ID = "hap_id"

def hk_setup_code(value):
    """Validate that a given config value is a valid icon."""
    value = cv.string_strict(value)
    if not value:
        return value
    if re.match("^[\\d]{3}-[\\d]{2}-[\\d]{3}$", value):
        return value
    raise cv.Invalid(
        'Setup code must match the format XXX-XX-XXX'
    )

ACC_INFO = {
    "name": AInfo.NAME,
    "model": AInfo.MODEL,
    "manufacturer": AInfo.MANUFACTURER,
    "serial_number": AInfo.SN,
    "fw_rev": AInfo.FW_REV,
}

ACCESSORY_INFORMATION = {
    cv.Optional(i): cv.string for i in ACC_INFO
}

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HAPRootComponent),
    cv.Optional(CONF_PORT, default=32042): cv.port,
    cv.Optional("meta") : ACCESSORY_INFORMATION,
    cv.Optional("setup_code", default="159-35-728"): hk_setup_code,
    cv.Optional("setup_id", default="ES32"): cv.All(cv.string_strict,cv.Upper,cv.Length(min=4, max=4, msg="Setup ID has to be a 4 character long alpha numeric string (with capital letters)"))
}).extend(cv.COMPONENT_SCHEMA),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    add_idf_component(
        name="esp_hap_core",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/esp_hap_core"
    )
    add_idf_component(
        name="esp_hap_apple_profiles",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/esp_hap_apple_profiles"
    )
    add_idf_component(
        name="esp_hap_extras",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/esp_hap_extras"
    )
    add_idf_component(
        name="esp_hap_platform",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/esp_hap_platform"
    )
    add_idf_component(
        name="hkdf-sha",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/hkdf-sha"
    )
    add_idf_component(
        name="mu_srp",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="esphome",
        path="components/homekit/mu_srp"
    )
    info_temp = []
    if "meta" in config:
        for m in config["meta"]:
            info_temp.append([ACC_INFO[m], config["meta"][m]])
    var = cg.new_Pvariable(config[CONF_ID], config["setup_code"], config["setup_id"], info_temp)
    if CONF_PORT in config:
        add_idf_sdkconfig_option("CONFIG_HAP_HTTP_SERVER_PORT", config[CONF_PORT])
    await cg.register_component(var, config)
