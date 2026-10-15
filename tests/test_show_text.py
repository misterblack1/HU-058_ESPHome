"""Run with venv/bin/python -m unittest discover -s tests.

Compile the panel's text layout on the host and check truncation, right
alignment, and the underscore for characters outside the font.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "firmware/esphome/components/aip33628"

SOURCE = r'''
#include "glyph.h"
#include <iostream>
using namespace esphome::aip33628;
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    uint8_t out[4];
    layout_text(line, out);
    std::cout << int(out[0]) << ' ' << int(out[1]) << ' ' << int(out[2]) << ' '
              << int(out[3]) << '\n';
  }
}
'''

# Segment bits, bit 0 = A through bit 6 = G, as in glyph.h.
BLANK = 0
UNDERSCORE = 0b0001000
H, E, L, A, B = 0b1110110, 0b1111001, 0b0111000, 0b1110111, 0b1111100
R, D, T, UPPER_C, LOWER_C = 0b1010000, 0b1011110, 0b1111000, 0b0111001, 0b1011000
G = 0b0111101  # segments A, C, D, E, F
UPPER_O, LOWER_O, LOWER_I = 0b0111111, 0b1011100, 0b0000100
ONE, TWO, THREE, FOUR, FIVE = 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101
DASH = 0b1000000

CASES = {
    "HELL": [H, E, L, L],
    "HELLO": [H, E, L, L],          # cut to the first four
    "1234567": [ONE, TWO, THREE, FOUR],
    "Hi": [BLANK, BLANK, H, LOWER_I],  # right aligned, i is segment C alone
    "1": [BLANK, BLANK, BLANK, ONE],
    "": [BLANK, BLANK, BLANK, BLANK],
    "A b": [BLANK, A, BLANK, B],    # a space is blank, not an underscore
    "SOI": [BLANK, FIVE, UPPER_O, ONE],  # S, O and I share the digit shapes
    "read": [R, E, A, D],           # one case in the font, the other case maps to it
    "READ": [R, E, A, D],
    "Cc": [BLANK, BLANK, UPPER_C, LOWER_C],  # both cases in the font, each keeps its own
    "Oo": [BLANK, BLANK, UPPER_O, LOWER_O],
    "GEt": [BLANK, G, E, T],
    "g": [BLANK, BLANK, BLANK, G],
    "EXIt": [E, UNDERSCORE, ONE, T],  # X is in the font in neither case
    "x-": [BLANK, BLANK, UNDERSCORE, DASH],
    "é1": [BLANK, BLANK, UNDERSCORE, ONE],  # one underscore per UTF-8 character
}


class ShowTextTests(unittest.TestCase):
    def test_layout_text(self):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            main = temp / "test.cpp"
            main.write_text(SOURCE)
            subprocess.run(
                ["c++", "-std=c++17", "-I", str(COMPONENT), str(main), "-o", str(temp / "test")],
                check=True, capture_output=True,
            )
            texts = list(CASES)
            result = subprocess.run(
                [str(temp / "test")], input="".join(t + "\n" for t in texts).encode(),
                capture_output=True, check=True,
            )
        lines = result.stdout.decode().splitlines()
        self.assertEqual(len(lines), len(texts))
        for text, line in zip(texts, lines):
            with self.subTest(text=text):
                self.assertEqual([int(x) for x in line.split()], CASES[text])


if __name__ == "__main__":
    unittest.main()
