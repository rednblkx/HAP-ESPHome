import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mdns, wifi, light, lock, sensor, switch, climate, pn532
from esphome.const import PLATFORM_ESP32, CONF_ID
from esphome.core import ID, Lambda
from esphome.components.esp32 import add_idf_component
from .. import homekit_base

AUTO_LOAD = ["homekit_base"]
DEPENDENCIES = ['esp32', 'network', 'homekit_base']
CODEOWNERS = ["@rednblkx"]

homekit_ns = homekit_base.homekit_ns
HAPRootComponent = homekit_base.HAPRootComponent
TemperatureUnits = homekit_ns.enum("TemperatureUnits")
HAPAccessory = homekit_ns.class_('HAPAccessory', cg.Component)
CONF_IDENTIFY_LAMBDA = "identify_fn"

TEMP_UNITS = {
    "CELSIUS": TemperatureUnits.CELSIUS,
    "FAHRENHEIT": TemperatureUnits.FAHRENHEIT
}

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID() : cv.declare_id(HAPAccessory),
    cv.Optional("light"): cv.ensure_list({cv.Required(CONF_ID):  cv.use_id(light.LightState)}),
    cv.Optional("lock"):  cv.ensure_list({cv.Required(CONF_ID): cv.use_id(lock.Lock), cv.Optional("nfc_id") : cv.use_id(pn532.PN532)}),
    cv.Optional("sensor"):  cv.ensure_list({cv.Required(CONF_ID): cv.use_id(sensor.Sensor), cv.Optional("temp_units", default="CELSIUS") : cv.enum(TEMP_UNITS)}),
    cv.Optional("switch"):  cv.ensure_list({cv.Required(CONF_ID): cv.use_id(switch.Switch)}),
    cv.Optional("climate"):  cv.ensure_list({cv.Required(CONF_ID): cv.use_id(climate.Climate)}),
}).extend(cv.COMPONENT_SCHEMA),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if 'light' in config: 
        for l in config["light"]:
            cg.add(var.add_light(await cg.get_variable(l['id'])))
    if 'sensor' in config:
        for l in config["sensor"]:
            cg.add(var.add_sensor(await cg.get_variable(l['id']), l['temp_units']))
    if 'lock' in config:
        for l in config["lock"]:
            cg.add(var.add_lock(await cg.get_variable(l['id'])))
            if "nfc_id" in l:
                cg.add_build_flag("-fexceptions")
                cg.add_platformio_option("build_unflags", "-fno-exceptions")
                nfc = await cg.get_variable(l["nfc_id"])
                cg.add(var.set_nfc_ctx(nfc))
                cg.add_define("USE_HOMEKEY")
                add_idf_component(
                    name="jsoncons",
                    repo="https://github.com/rednblkx/jsoncons.git",
                    ref="master"
                )
                add_idf_component(
                    name="HK-HomeKit-Lib",
                    repo="https://github.com/rednblkx/HK-HomeKit-Lib.git",
                    ref="esphome"
                )
    if "switch" in config:
        for l in config["switch"]:
            cg.add(var.add_switch(await cg.get_variable(l['id'])))
    if "climate" in config:
        for l in config["climate"]:
            cg.add(var.add_climate(await cg.get_variable(l['id'])))