import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, pn532
from esphome.const import CONF_ID

AUTO_LOAD = ["pn532"]
CODEOWNERS = ["@OttoWinter", "@jesserockz"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

pn532_uart_ns = cg.esphome_ns.namespace("pn532_uart")
PN532Uart = pn532_uart_ns.class_("PN532Uart", pn532.PN532, uart.UARTDevice)

CONFIG_SCHEMA = cv.All(
    pn532.PN532_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN532Uart),
        }
    ).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "pn532_uart",
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await pn532.setup_pn532(var, config)
    await uart.register_uart_device(var, config)
