import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_PORT, PLATFORM_ESP32, CONF_ID
from esphome.core import CORE
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
from pathlib import Path
import re
from esphome.coroutine import CoroPriority, coroutine_with_priority

# Local stand-in component (directory name "libsodium") that forwards to Espressif's
# full libsodium port, see libsodium/CMakeLists.txt
LIBSODIUM_SHIM_PATH = str(Path(__file__).parent / "libsodium")
# Every name under which a libsodium dependency reaches the build
LIBSODIUM_NAMES = ("esphome/libsodium", "espressif/libsodium", "libsodium")

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
cv.only_on_esp32)

async def to_code(config):
    if getattr(CORE, "using_toolchain_esp_idf", False):
        cg.add_platformio_option("lib_ignore", ["libsodium"])
    # Every */libsodium dependency resolves to this local "libsodium", so ESPHome 2026.10's
    # esphome/libsodium (api/ota encryption) and espressif/libsodium no longer collide
    add_idf_component(name="libsodium", path=LIBSODIUM_SHIM_PATH)
    # Runs after every component's to_code, so it also catches the esphome/libsodium
    # entry that the noise component adds, whatever order the components load in
    CORE.add_job(_unify_libsodium)
    # Espressif's full port (1.0.22~1) behind it, as HomeKit pairing needs Ed25519 signing;
    # from git, since a registry */libsodium dependency would resolve to the local one again
    add_idf_component(
        name="libsodium_full",
        repo="https://github.com/espressif/idf-extra-components.git",
        ref="1e32ecf000d79db25389d44d29d04b61b72154d4",
        path="libsodium"
    )
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
    add_idf_sdkconfig_option("CONFIG_MBEDTLS_HKDF_C", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_MAX_SOCKETS", 16)
    await cg.register_component(var, config)

def _set_idf_component_path(name, path):
    """Point an IDF component at a local path, replacing a registry/git entry quietly."""
    try:
        from esphome.const import KEY_ESP32
        from esphome.components.esp32.const import (
            KEY_COMPONENTS, KEY_PATH, KEY_REF, KEY_REPO,
        )
        registry = CORE.data[KEY_ESP32][KEY_COMPONENTS]
    except (ImportError, KeyError):
        add_idf_component(name=name, path=path)
        return
    registry[name] = {KEY_REPO: None, KEY_REF: None, KEY_PATH: path}


@coroutine_with_priority(CoroPriority.LATE)
async def _unify_libsodium():
    # The IDF component manager refuses to build when two components share the short
    # name "libsodium". Declare every name as a root-level local dependency on the same
    # directory, so ESP-IDF registers exactly one component "libsodium".
    for name in LIBSODIUM_NAMES:
        _set_idf_component_path(name, LIBSODIUM_SHIM_PATH)