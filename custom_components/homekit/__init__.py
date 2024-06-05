import esphome.codegen as cg
# from esphome import cpp_generator as cppgen
import esphome.config_validation as cv
from esphome.components import mdns, wifi, light, lock, sensor, switch
# from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
# from esphome.const import CONF_ID, CONF_PORT, PLATFORM_ESP32, CONF_ON_CONNECT, CONF_AUTOMATION_ID, CONF_LAMBDA, CONF_WIFI, CONF_LIGHT
from esphome.const import CONF_PORT, PLATFORM_ESP32, CONF_ID
# from esphome import automation
# from esphome.core import CORE
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
# from esphome.helpers import copy_file_if_changed

DEPENDENCIES = ['esp32', 'network']

hap_component_ns = cg.esphome_ns.namespace('homekit')
HAPRootComponent = hap_component_ns.class_('HAPRootComponent', cg.Component, cg.Controller)

CONF_EXCLUDE_ENTITIES = "exclude"

exclude_list = {
    cv.Optional("light"): cv.ensure_list(cv.use_id(light.LightState)),
    cv.Optional("lock"):  cv.ensure_list(cv.use_id(lock.Lock)),
    cv.Optional("sensor"):  cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Optional("switch"):  cv.ensure_list(cv.use_id(switch.Switch))
}

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HAPRootComponent),
    # cv.GenerateID(CONF_AUTOMATION_ID): cv.declare_id(automation.Automation),
    # cv.GenerateID(CONF_LAMBDA): cv.declare_id(automation.LambdaAction),
    cv.Optional(CONF_EXCLUDE_ENTITIES) : cv.All(cv.ensure_list(exclude_list), cv.Length(min=1, max=64)),
    cv.Optional(CONF_PORT, default=32042): cv.port
}).extend(cv.COMPONENT_SCHEMA),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    add_idf_component(
        name="libsodium",
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
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_PORT in config:
        add_idf_sdkconfig_option("CONFIG_HAP_HTTP_SERVER_PORT", config[CONF_PORT])
    if CONF_EXCLUDE_ENTITIES in config:
        if 'light' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['light']:
                light = await cg.get_variable(l)
                cg.add(var.exclude_lights.push_back(light))
        if 'lock' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['lock']:
                lock = await cg.get_variable(l)
                cg.add(var.exclude_locks.push_back(lock))
        if 'sensor' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['sensor']:
                sensor = await cg.get_variable(l)
                cg.add(var.exclude_sensors.push_back(sensor))
        if 'switch' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['switch']:
                switch = await cg.get_variable(l)
                cg.add(var.exclude_switches.push_back(switch))
    await cg.register_component(var, config)
    # if CONF_WIFI in CORE.config:
    #     wifi_parent = await cg.get_variable(CORE.config[CONF_WIFI]['id'])
    #     arg_types = [arg[0] for arg in []]
    #     obj = cg.new_Pvariable(config[CONF_AUTOMATION_ID], cg.TemplateArguments(*arg_types), wifi_parent.get_connect_trigger())
    #     lambdaAction = cg.new_Pvariable(config[CONF_LAMBDA], cg.TemplateArguments(*arg_types), cppgen.LambdaExpression((var.network_on_connect(),";"),()))
    #     cg.add(obj.add_actions([lambdaAction]))