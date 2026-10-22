import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import time as time_
from esphome.const import CONF_ID, CONF_NUMBER, PLATFORM_ESP32

CODEOWNERS = ["@misterblack1"]
ESP_PLATFORMS = [PLATFORM_ESP32]

aip33628_ns = cg.esphome_ns.namespace("aip33628")
Aip33628Panel = aip33628_ns.class_("Aip33628Panel", cg.Component)

CONF_CLK_PIN = "clk_pin"
CONF_DATA_PIN = "data_pin"
CONF_CLK2_PIN = "clk2_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_TIME_ID = "time_id"
CONF_MAX_CURRENT = "max_current"
CONF_TWELVE_HOUR = "twelve_hour"
CONF_BLINK_COLON = "blink_colon"


def _low_bank_pin(value):
    """Both buses are clocked from one write to the low GPIO output register.

    That register only reaches GPIO0 to GPIO31. A pin above that would need a
    second register and a second store per edge, which is most of the reason
    the frame send is fast enough to subdivide a COM slot at all.
    """
    pin = value[CONF_NUMBER]
    if pin >= 32:
        raise cv.Invalid(
            f"GPIO{pin} is above GPIO31, and the display scan drives all four "
            "lines from the low output register. Pick a pin below GPIO32."
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Aip33628Panel),
        cv.Required(CONF_CLK_PIN): cv.All(
            pins.internal_gpio_output_pin_schema, _low_bank_pin
        ),
        cv.Required(CONF_DATA_PIN): cv.All(
            pins.internal_gpio_output_pin_schema, _low_bank_pin
        ),
        cv.Required(CONF_CLK2_PIN): cv.All(
            pins.internal_gpio_output_pin_schema, _low_bank_pin
        ),
        cv.Required(CONF_DATA2_PIN): cv.All(
            pins.internal_gpio_output_pin_schema, _low_bank_pin
        ),
        cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        # A fixed ceiling on IS[3:0]. It must not depend on what is on screen,
        # or the whole panel changes brightness whenever the colon blinks.
        cv.Optional(CONF_MAX_CURRENT, default=15): cv.int_range(min=0, max=15),
        # Power on defaults only. Home Assistant owns both at run time
        # through the template switches in clock.yaml.
        cv.Optional(CONF_TWELVE_HOUR, default=True): cv.boolean,
        cv.Optional(CONF_BLINK_COLON, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    clk2 = await cg.gpio_pin_expression(config[CONF_CLK2_PIN])
    data2 = await cg.gpio_pin_expression(config[CONF_DATA2_PIN])
    cg.add(var.set_pins(clk, data, clk2, data2))

    if CONF_TIME_ID in config:
        rtc = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(rtc))

    cg.add(var.set_max_current(config[CONF_MAX_CURRENT]))
    cg.add(var.set_twelve_hour(config[CONF_TWELVE_HOUR]))
    cg.add(var.set_blink_colon(config[CONF_BLINK_COLON]))
