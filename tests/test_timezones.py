"""Run with venv/bin/python -m unittest discover -s tests.

Compare the firmware's generated rules and C++ conversion against zoneinfo,
including both DST transitions and fractional-hour offsets.
"""
import importlib
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import tempfile
import unittest
from zoneinfo import ZoneInfo

import esphome
import esphome.components

ROOT = Path(__file__).resolve().parents[1]
esphome.components.__path__.append(str(ROOT / "firmware/esphome/components"))
select = importlib.import_module("esphome.components.aip33628.select")


class TimezoneTests(unittest.TestCase):
    def test_invalid_timezone(self):
        for zone in ("", "Home Assistant", "Not/A_Timezone"):
            with self.subTest(zone=zone), self.assertRaises(select.cv.Invalid):
                select.validate_zone(zone)

    def test_firmware_rules_against_zoneinfo(self):
        structs = []
        for zone in select.DEFAULT_TIMEZONES:
            tz = select.parse_posix_tz(select.time_.validate_tz(zone))
            structs.append("{%d, %d, %s, %s}" % (
                tz.std_offset_seconds, tz.dst_offset_seconds,
                select.rule_expression(tz.dst_start), select.rule_expression(tz.dst_end),
            ))
        source = '''
#include "esphome/components/time/posix_tz.h"
#include <iostream>
using namespace esphome;
int main() {
  time::ParsedTimezone zones[] = {ZONES};
  size_t zone;
  long long epoch;
  while (std::cin >> zone >> epoch) {
    struct tm result{};
    if (!time::epoch_to_local_tm(epoch, zones[zone], &result)) return 1;
    std::cout << result.tm_year + 1900 << ' ' << result.tm_mon + 1 << ' '
              << result.tm_mday << ' ' << result.tm_hour << ' '
              << result.tm_min << '\\n';
  }
}
'''.replace("ZONES", ",\n".join(structs))
        start = int(datetime(2026, 1, 1, tzinfo=timezone.utc).timestamp())
        finish = int(datetime(2027, 1, 1, tzinfo=timezone.utc).timestamp())
        # Hourly samples at :00 and :30 cover half-hour DST boundaries too.
        epochs = range(start, finish, 1800)
        inputs, expected = [], []
        for index, zone in enumerate(select.DEFAULT_TIMEZONES):
            for epoch in epochs:
                local = datetime.fromtimestamp(epoch, ZoneInfo(zone))
                inputs.append(f"{index} {epoch}\n")
                expected.append(f"{local.year} {local.month} {local.day} {local.hour} {local.minute}")
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            defines = temp / "esphome/core/defines.h"
            defines.parent.mkdir(parents=True)
            defines.write_text("#define USE_TIME_TIMEZONE\n")
            main = temp / "test.cpp"
            main.write_text(source)
            package = Path(esphome.__file__).parent.parent
            subprocess.run([
                "c++", "-std=c++17", "-DUSE_TIME_TIMEZONE", "-I", str(temp),
                "-I", str(package), str(main),
                str(package / "esphome/components/time/posix_tz.cpp"),
                "-o", str(temp / "test"),
            ], check=True, capture_output=True)
            result = subprocess.run([str(temp / "test")], input="".join(inputs),
                                    capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.splitlines(), expected)


if __name__ == "__main__":
    unittest.main()
