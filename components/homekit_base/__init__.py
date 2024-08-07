import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mdns, wifi, light, lock, sensor, switch
from esphome.const import CONF_PORT, PLATFORM_ESP32, CONF_ID
from esphome.core import ID, Lambda
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import re

DEPENDENCIES = ['esp32', 'network']
CODEOWNERS = ["@rednblkx"]
MULTI_CONF = True

homekit_ns = cg.esphome_ns.namespace('homekit')
HAPRootComponent = homekit_ns.class_('HAPRootComponent', cg.Component)
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

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HAPRootComponent),
    cv.Optional(CONF_PORT, default=32042): cv.port,
    cv.Optional("setup_code", default="159-35-728"): hk_setup_code
}).extend(cv.COMPONENT_SCHEMA),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    # cg.add_define("CONFIG_ESP_MFI_DEBUG_ENABLE")
    add_idf_component(
        name="idf-extra-components",
        repo="https://github.com/espressif/idf-extra-components.git",
        ref="master",
        components=["libsodium", "jsmn", "json_parser", "json_generator"],
        submodules=["libsodium/libsodium"]
    )
    add_idf_component(
        name="esp-homekit-sdk",
        repo="https://github.com/rednblkx/esp-homekit-sdk",
        ref="master",
        components=["esp_hap_core", "esp_hap_apple_profiles", "esp_hap_extras", "esp_hap_platform", "hkdf-sha", "mu_srp"],
    )
    var = cg.new_Pvariable(config[CONF_ID], config["setup_code"])
    if CONF_PORT in config:
        add_idf_sdkconfig_option("CONFIG_HAP_HTTP_SERVER_PORT", config[CONF_PORT])
    await cg.register_component(var, config)