"""A persisted display timezone, independent of the time source's timezone."""
from aioesphomeapi.posix_tz import parse_posix_tz
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select, time as time_
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG

from . import Aip33628Panel, aip33628_ns

DEPENDENCIES = ["aip33628", "time"]
TimezoneSelect = aip33628_ns.class_("TimezoneSelect", select.Select, cg.Component)
CONF_PANEL = "aip33628_id"
CONF_TIMEZONES = "timezones"
DEFAULT_TIMEZONES = [
    "Etc/UTC", "Pacific/Honolulu", "America/Anchorage", "America/Los_Angeles",
    "America/Phoenix", "America/Denver", "America/Chicago", "America/New_York",
    "America/Halifax", "America/St_Johns", "America/Sao_Paulo", "Europe/London",
    "Europe/Paris", "Europe/Helsinki", "Africa/Johannesburg", "Asia/Dubai",
    "Asia/Kolkata", "Asia/Kathmandu", "Asia/Bangkok", "Asia/Shanghai",
    "Asia/Tokyo", "Australia/Perth", "Australia/Adelaide", "Australia/Brisbane",
    "Australia/Sydney", "Pacific/Auckland",
]


def validate_zone(value):
    value = cv.string_strict(value)
    if not value or value == "Home Assistant":
        raise cv.Invalid("Specify a timezone such as America/Chicago")
    time_.validate_tz(value)
    return value


CONFIG_SCHEMA = select.select_schema(
    TimezoneSelect, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:map-clock"
).extend({
    cv.GenerateID(CONF_PANEL): cv.use_id(Aip33628Panel),
    cv.Optional(CONF_TIMEZONES, default=DEFAULT_TIMEZONES): cv.All(
        cv.ensure_list(validate_zone), cv.Length(min=1),
    ),
}).extend(cv.COMPONENT_SCHEMA)


def rule_expression(rule):
    return "{%d, %d, time::DSTRuleType(%d), %d, %d, %d}" % (
        rule.time_seconds, rule.day, rule.type, rule.month, rule.week, rule.day_of_week,
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    zones = list(dict.fromkeys(config[CONF_TIMEZONES]))
    await select.register_select(var, config, options=["Home Assistant", *zones])
    cg.add(var.set_panel(await cg.get_variable(config[CONF_PANEL])))
    cg.add_define("USE_TIME_TIMEZONE")
    cg.add_define("USE_AIP33628_TIMEZONE_SELECT")
    for zone in zones:
        tz = parse_posix_tz(time_.validate_tz(zone))
        cg.add(var.add_timezone(cg.RawExpression(
            "time::ParsedTimezone{%d, %d, %s, %s}" % (
                tz.std_offset_seconds, tz.dst_offset_seconds,
                rule_expression(tz.dst_start), rule_expression(tz.dst_end),
            )
        )))
