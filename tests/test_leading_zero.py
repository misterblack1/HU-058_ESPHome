"""Run with venv/bin/python -m unittest discover -s tests.

Check that the leading_zero option validates, defaults to off, reaches the
component setter, and is wired to a switch in clock.yaml.
"""
import asyncio
import importlib
from pathlib import Path
import unittest
from unittest import mock

import esphome
import esphome.components
import yaml

ROOT = Path(__file__).resolve().parents[1]
esphome.components.__path__.append(str(ROOT / "firmware/esphome/components"))
aip33628 = importlib.import_module("esphome.components.aip33628")


def schema_entry(name):
    for key, validator in aip33628.CONFIG_SCHEMA.schema.items():
        if str(key) == name:
            return key, validator
    raise KeyError(name)


class _Loader(yaml.SafeLoader):
    pass


# clock.yaml uses !secret, and the values are not needed here.
_Loader.add_constructor("!secret", lambda loader, node: loader.construct_scalar(node))


class LeadingZeroTests(unittest.TestCase):
    def test_default_is_off(self):
        key, _ = schema_entry(aip33628.CONF_LEADING_ZERO)
        self.assertIs(key.default(), False)

    def test_accepts_booleans(self):
        _, validator = schema_entry(aip33628.CONF_LEADING_ZERO)
        for value, want in ((True, True), (False, False), ("true", True), ("off", False)):
            with self.subTest(value=value):
                self.assertIs(validator(value), want)

    def test_rejects_other_values(self):
        _, validator = schema_entry(aip33628.CONF_LEADING_ZERO)
        for value in ("maybe", 2):
            with self.subTest(value=value), self.assertRaises(aip33628.cv.Invalid):
                validator(value)

    def test_to_code_calls_setter(self):
        for value in (True, False):
            with self.subTest(value=value):
                var = mock.MagicMock()
                config = {
                    aip33628.CONF_ID: "panel",
                    aip33628.CONF_CLK_PIN: None,
                    aip33628.CONF_DATA_PIN: None,
                    aip33628.CONF_CLK2_PIN: None,
                    aip33628.CONF_DATA2_PIN: None,
                    aip33628.CONF_MAX_CURRENT: 15,
                    aip33628.CONF_TWELVE_HOUR: True,
                    aip33628.CONF_BLINK_COLON: True,
                    aip33628.CONF_LEADING_ZERO: value,
                }
                with mock.patch.object(aip33628, "cg") as cg:
                    cg.new_Pvariable.return_value = var
                    cg.register_component = mock.AsyncMock()
                    cg.gpio_pin_expression = mock.AsyncMock()
                    asyncio.run(aip33628.to_code(config))
                var.set_leading_zero.assert_called_once_with(value)
                cg.add.assert_any_call(var.set_leading_zero.return_value)

    def test_clock_yaml_wiring(self):
        text = (ROOT / "firmware/esphome/clock.yaml").read_text()
        config = yaml.load(text, Loader=_Loader)
        self.assertIs(config["aip33628"][aip33628.CONF_LEADING_ZERO], False)

        switches = [s for s in config["switch"] if s.get("name") == "Leading Zero"]
        self.assertEqual(len(switches), 1)
        switch = switches[0]
        self.assertEqual(
            switch["turn_on_action"], [{"lambda": "id(panel)->set_leading_zero(true);"}]
        )
        self.assertEqual(
            switch["turn_off_action"], [{"lambda": "id(panel)->set_leading_zero(false);"}]
        )


if __name__ == "__main__":
    unittest.main()
