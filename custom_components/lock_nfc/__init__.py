import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.components import pn532_spi, pn532, spi
from esphome.const import PLATFORM_ESP32, CONF_ID

MULTI_CONF = True
DEPENDENCIES = ['esp32']
CODEOWNERS = ["@rednblkx"]

nfc = cg.esphome_ns.namespace('nfc')
LockNFC = nfc.class_('LockNFC', pn532_spi.PN532Spi)

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID() : cv.declare_id(LockNFC),
    cv.Required("nfc_id") : cv.use_id(pn532_spi.PN532Spi)
}),
cv.only_on([PLATFORM_ESP32]),
cv.only_with_esp_idf)

async def to_code(config):
    nfc = await cg.get_variable(config["nfc_id"])
    var = cg.new_Pvariable(config[CONF_ID], nfc)
    # await cg.register_parented(var, config["nfc_id"])