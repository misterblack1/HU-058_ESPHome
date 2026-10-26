#pragma once

// The seven segment font and text layout. No ESPHome or ESP-IDF includes, so
// tests/test_show_text.py can compile this on the host.

#include <cstdint>
#include <string>

namespace esphome {
namespace aip33628 {

// Seven segment font, bit 0 = A through bit 6 = G. The letters are the subset
// that reads unambiguously on seven segments in text. S, O and I share the
// shapes of 5, 0 and 1, so they read as letters only next to other letters,
// not as a unit after a number. i is segment C alone.
inline uint8_t glyph(char c) {
  switch (c) {
    case 'A': return 0b1110111;
    case 'b': return 0b1111100;
    case 'C': return 0b0111001;
    case 'c': return 0b1011000;
    case 'd': return 0b1011110;
    case 'E': return 0b1111001;
    case 'F': return 0b1110001;
    case 'G': return 0b0111101;
    case 'H': return 0b1110110;
    case 'h': return 0b1110100;
    case 'L': return 0b0111000;
    case 'n': return 0b1010100;
    case 'o': return 0b1011100;
    case 'P': return 0b1110011;
    case 'r': return 0b1010000;
    case 't': return 0b1111000;
    case 'U': return 0b0111110;
    case 'u': return 0b0011100;
    case 'y': return 0b1101110;
    case 'S': return 0b1101101;
    case 'O': return 0b0111111;
    case 'I': return 0b0000110;
    case 'i': return 0b0000100;
    case '0': return 0b0111111;
    case '1': return 0b0000110;
    case '2': return 0b1011011;
    case '3': return 0b1001111;
    case '4': return 0b1100110;
    case '5': return 0b1101101;
    case '6': return 0b1111101;
    case '7': return 0b0000111;
    case '8': return 0b1111111;
    case '9': return 0b1101111;
    case '-': return 0b1000000;
    default:  return 0;
  }
}

// The exact character if the font has it, otherwise the other case of the same
// letter, so a and A both show as A. Letters with both cases in the font keep
// their own shape.
inline uint8_t glyph_any_case(char c) {
  uint8_t bits = glyph(c);
  if (bits != 0)
    return bits;
  if (c >= 'a' && c <= 'z')
    return glyph((char) (c - 'a' + 'A'));
  if (c >= 'A' && c <= 'Z')
    return glyph((char) (c - 'A' + 'a'));
  return 0;
}

// Segment D alone, for a character the font does not have.
static const uint8_t GLYPH_UNSUPPORTED = 0b0001000;

// Four positions of text, right aligned, leftmost first. Text longer than four
// characters is cut to its first four. Letters fall back to their other case.
// A space is blank, and any other character outside the font shows as an
// underscore. A UTF-8 character takes
// one position, so it shows as one underscore rather than one per byte.
inline void layout_text(const std::string &text, uint8_t out[4]) {
  char chars[4];
  int n = 0;
  for (char c : text) {
    if (((uint8_t) c & 0xC0) == 0x80)
      continue;  // UTF-8 continuation byte
    if (n == 4)
      break;
    chars[n++] = c;
  }
  for (int i = 0; i < 4; i++)
    out[i] = 0;
  for (int i = 0; i < n; i++) {
    uint8_t bits = glyph_any_case(chars[i]);
    out[4 - n + i] = bits ? bits : chars[i] == ' ' ? 0 : GLYPH_UNSUPPORTED;
  }
}

}  // namespace aip33628
}  // namespace esphome
