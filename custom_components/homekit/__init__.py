import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mdns, wifi, light, lock, sensor, switch
from esphome.const import CONF_PORT, PLATFORM_ESP32, CONF_ID
from esphome.core import ID, Lambda
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option

DEPENDENCIES = ['esp32', 'network']
CODEOWNERS = ["@rednblkx"]
MULTI_CONF = True

hap_component_ns = cg.esphome_ns.namespace('homekit')
HAPRootComponent = hap_component_ns.class_('HAPRootComponent', cg.Component, cg.Controller)

CONF_HAP_ID = "hap_id"
CONF_IDENTIFY_LAMBDA = "identify_fn"
CONF_INCLUDE_ENTITIES = "include"
CONF_EXCLUDE_ENTITIES = "exclude"
CONF_EXPOSE_ALL = "expose_all"

entities_list = {
    cv.Optional("light"): cv.ensure_list(cv.use_id(light.LightState)),
    cv.Optional("lock"):  cv.ensure_list(cv.use_id(lock.Lock)),
    cv.Optional("sensor"):  cv.ensure_list(cv.use_id(sensor.Sensor)),
    cv.Optional("switch"):  cv.ensure_list(cv.use_id(switch.Switch))
}

# identify_acc = {
#     cv.Optional("light"): cv.ensure_list({cv.use_id(light.LightState) : cv.returning_lambda}),
#     cv.Optional("lock"):  cv.ensure_list({cv.use_id(lock.Lock) : cv.returning_lambda}),
#     cv.Optional("sensor"):  cv.ensure_list({cv.use_id(sensor.Sensor) : cv.returning_lambda}),
#     cv.Optional("switch"):  cv.ensure_list({cv.use_id(switch.Switch) : cv.returning_lambda})
# }

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(HAPRootComponent),
    cv.Optional(CONF_EXPOSE_ALL, default=True) : cv.boolean,
    # cv.Optional(CONF_IDENTIFY_LAMBDA) : cv.All(cv.ensure_list(identify_acc), cv.Length(min=1, max=64)),
    cv.Optional(CONF_INCLUDE_ENTITIES) : cv.All(cv.ensure_list(entities_list), cv.Length(min=1, max=64)),
    cv.Optional(CONF_EXCLUDE_ENTITIES) : cv.All(cv.ensure_list(entities_list), cv.Length(min=1, max=64)),
    cv.Optional(CONF_PORT, default=32042): cv.port
}).extend(cv.COMPONENT_SCHEMA),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    cg.add_define("CONFIG_ESP_MFI_DEBUG_ENABLE")
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
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_EXPOSE_ALL])
    if CONF_PORT in config:
        add_idf_sdkconfig_option("CONFIG_HAP_HTTP_SERVER_PORT", config[CONF_PORT])
    if CONF_EXCLUDE_ENTITIES in config:
        if 'light' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['light']:
                light_c = await cg.get_variable(l)
                cg.add(var.exclude_lights.push_back(light_c))
        if 'lock' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['lock']:
                lock_c = await cg.get_variable(l)
                cg.add(var.exclude_locks.push_back(lock_c))
        if 'sensor' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['sensor']:
                sensor_c = await cg.get_variable(l)
                cg.add(var.exclude_sensors.push_back(sensor_c))
        if 'switch' in config[CONF_EXCLUDE_ENTITIES][0]:
            for l in config[CONF_EXCLUDE_ENTITIES][0]['switch']:
                switch_c = await cg.get_variable(l)
                cg.add(var.exclude_switches.push_back(switch_c))
    if CONF_INCLUDE_ENTITIES in config:
        if 'light' in config[CONF_INCLUDE_ENTITIES][0]:
            for l in config[CONF_INCLUDE_ENTITIES][0]['light']:
                light_c = await cg.get_variable(l)
                cg.add(var.include_lights.push_back(light_c))
        if 'lock' in config[CONF_INCLUDE_ENTITIES][0]:
            for l in config[CONF_INCLUDE_ENTITIES][0]['lock']:
                lock_c = await cg.get_variable(l)
                cg.add(var.include_locks.push_back(lock_c))
        if 'sensor' in config[CONF_INCLUDE_ENTITIES][0]:
            for l in config[CONF_INCLUDE_ENTITIES][0]['sensor']:
                sensor_c = await cg.get_variable(l)
                cg.add(var.include_sensors.push_back(sensor_c))
        if 'switch' in config[CONF_INCLUDE_ENTITIES][0]:
            for l in config[CONF_INCLUDE_ENTITIES][0]['switch']:
                switch_c = await cg.get_variable(l)
                cg.add(var.include_switches.push_back(switch_c))
    # if CONF_IDENTIFY_LAMBDA in config and 'light' in config[CONF_IDENTIFY_LAMBDA][0]:
    #     for key, value in config[CONF_IDENTIFY_LAMBDA][0]['light'][0].items():
    #         identify_template = await cg.process_lambda(value, [])
    await cg.register_component(var, config)