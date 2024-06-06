import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import hap_component_ns, HAPRootComponent, CONF_HAP_ID
ResetButton = hap_component_ns.class_("ResetButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_HAP_ID): cv.use_id(HAPRootComponent),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        ResetButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
}

async def to_code(config):
    hap_component = await cg.get_variable(config[CONF_HAP_ID])
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_HAP_ID])
        cg.add(hap_component.set_reset_button(b))