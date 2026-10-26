"""Run with venv/bin/python -m unittest discover -s tests.

Check that clock.yaml wires the Seconds Mode switch to the component setter.
"""
from pathlib import Path
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]


class _Loader(yaml.SafeLoader):
    pass


# clock.yaml uses !secret, and the values are not needed here.
_Loader.add_constructor("!secret", lambda loader, node: loader.construct_scalar(node))


class SecondsModeTests(unittest.TestCase):
    def test_clock_yaml_wiring(self):
        text = (ROOT / "firmware/esphome/clock.yaml").read_text()
        config = yaml.load(text, Loader=_Loader)
        switches = [s for s in config["switch"] if s.get("name") == "Seconds Mode"]
        self.assertEqual(len(switches), 1)
        switch = switches[0]
        self.assertEqual(switch["restore_mode"], "RESTORE_DEFAULT_OFF")
        self.assertEqual(
            switch["turn_on_action"], [{"lambda": "id(panel)->set_seconds_mode(true);"}]
        )
        self.assertEqual(
            switch["turn_off_action"], [{"lambda": "id(panel)->set_seconds_mode(false);"}]
        )


if __name__ == "__main__":
    unittest.main()
