from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import nfc
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_FINISHED_WRITE,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_RESET_PIN,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@OttoWinter", "@jesserockz"]
AUTO_LOAD = ["binary_sensor", "nfc"]
MULTI_CONF = True

CONF_PN532_ID = "pn532_id"
CONF_HEALTH_CHECK_INTERVAL = "health_check_interval"
CONF_MAX_POLL_STALL = "max_poll_stall"
CONF_PREVENTIVE_REINIT_INTERVAL = "preventive_reinit_interval"
CONF_POWER_PIN = "power_pin"
CONF_ON_RECOVERY = "on_recovery"

pn532_ns = cg.esphome_ns.namespace("pn532")
PN532 = pn532_ns.class_("PN532", cg.PollingComponent)

PN532IsWritingCondition = pn532_ns.class_(
    "PN532IsWritingCondition", automation.Condition
)

PN532_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PN532),
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        cv.Optional(CONF_ON_FINISHED_WRITE): automation.validate_automation({}),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(nfc.NfcOnTagTrigger),
            }
        ),
        # Watchdog / recovery
        cv.Optional(
            CONF_HEALTH_CHECK_INTERVAL, default="60s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_MAX_POLL_STALL, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_PREVENTIVE_REINIT_INTERVAL, default="0s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ON_RECOVERY): automation.validate_automation({}),
    }
).extend(cv.polling_component_schema("1s"))


def CONFIG_SCHEMA(conf):
    if conf:
        raise cv.Invalid(
            "This component has been moved in 1.16, please see the docs for updated "
            "instructions. https://esphome.io/components/binary_sensor/pn532/"
        )


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_FINISHED_WRITE, "add_on_finished_write_callback"
    ),
    automation.CallbackAutomation(
        CONF_ON_RECOVERY, "add_on_recovery_callback", [(cg.int_, "level")]
    ),
)


async def setup_pn532(var, config):
    await cg.register_component(var, config)

    cg.add(var.set_health_check_interval(config[CONF_HEALTH_CHECK_INTERVAL]))
    cg.add(var.set_max_poll_stall(config[CONF_MAX_POLL_STALL]))
    cg.add(
        var.set_preventive_reinit_interval(config[CONF_PREVENTIVE_REINIT_INTERVAL])
    )
    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))
    if CONF_POWER_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
        cg.add(var.set_power_pin(pin))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (nfc.NfcTag, "tag")], conf
        )

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x"), (nfc.NfcTag, "tag")], conf
        )

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


@automation.register_condition(
    "pn532.is_writing",
    PN532IsWritingCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PN532),
        }
    ),
)
async def pn532_is_writing_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
