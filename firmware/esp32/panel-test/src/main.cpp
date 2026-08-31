// AiP33628 panel reference firmware for the HU-058D clock board.
// Drives both display buses from an ESP32-WROOM-32, with no dependency on
// anything beyond the Arduino core. Everything specific to this panel sits in
// the tables near the top, so a port to another MCU comes down to replacing
// the four GPIO writes inside sendFrame.
//
// Mapping and scan parameters come from docs/display-map.md.

#include <Arduino.h>
#include <esp_timer.h>
#include <string.h>

// ESP32 GPIO -> MCU socket pin on the clock board. Ground through socket pin 8.
static const int PIN_CLK = 22;     // socket pin 14, driver 1 clock
static const int PIN_DATA = 21;    // socket pin 5,  driver 1 data
static const int PIN_CLK_1 = 19;   // socket pin 1,  driver 2 clock
static const int PIN_DATA_1 = 18;  // socket pin 2,  driver 2 data

static const uint32_t M_CLK = 1UL << PIN_CLK;
static const uint32_t M_DATA = 1UL << PIN_DATA;
static const uint32_t M_CLK_1 = 1UL << PIN_CLK_1;
static const uint32_t M_DATA_1 = 1UL << PIN_DATA_1;

// Stock is 2407us per cycle, four COM pairs, 415.5Hz refresh.
static const uint8_t COM_SEQ[4] = {0x30, 0x0C, 0x03, 0xC0};

// Global current level, 0x0 = 2.5mA through 0xF = 40.4mA.
// Stock uses 0xD at normal brightness and 0xF at its brightest.
static volatile uint8_t g_current = 0xD;

// FRAME BUFFER:
// Per driver, per COM pair, up to two sub-frames. A COM pair only gets a
// second sub-frame when something in it sits at half duty, which is how the
// stock firmware mixes colors.
static volatile uint16_t g_ss[2][4][2];
static volatile uint8_t g_nsub[2][4];

// Channel levels, indexed [driver][com pair][led 1..5][channel].
// 0 = off, 1 = half duty, 2 = full. The stock engine puts its boundary at a
// third or two thirds rather than the middle, which this does not reproduce.
// The ESPHome component uses four binary weighted sub-frames instead and gets
// sixteen levels. See docs/display-map.md.
static uint8_t g_level[2][4][6][3];

enum { CH_BLUE = 0, CH_GREEN = 1, CH_RED = 2 };

// Roughly 100ns at 240MHz. The part accepts 30MHz and a 16ns pulse, so this
// is deliberately slow and leaves plenty of margin on a 3.3V drive.
static inline void IRAM_ATTR delayTick() {
  for (int i = 0; i < 12; i++) {
    __asm__ __volatile__("nop");
  }
}

// Emit one 30-bit frame and latch it.
// Bits are LSB first: SS[15:0], CS[7:0], IS[3:0], then two reserved zeros.
// Data only ever changes while CLK is low. The latch is a DATA rising edge
// while CLK is held high after the last bit, which is what the 8051 does.
static void IRAM_ATTR sendFrame(bool bus2, uint16_t ss, uint8_t cs, uint8_t is) {
  const uint32_t mclk = bus2 ? M_CLK_1 : M_CLK;
  const uint32_t mdat = bus2 ? M_DATA_1 : M_DATA;

  uint32_t frame = (uint32_t)ss | ((uint32_t)cs << 16) | ((uint32_t)(is & 0xF) << 24);

  GPIO.out_w1tc = mclk | mdat;
  delayTick();

  for (int i = 0; i < 30; i++) {
    if ((frame >> i) & 1) {
      GPIO.out_w1ts = mdat;
    } else {
      GPIO.out_w1tc = mdat;
    }
    delayTick();
    GPIO.out_w1ts = mclk;  // rising edge shifts the bit in
    delayTick();
    if (i < 29) {
      GPIO.out_w1tc = mclk;
      delayTick();
    }
  }

  // CLK is still high after bit 29. Pulse DATA to latch.
  GPIO.out_w1tc = mdat;
  delayTick();
  GPIO.out_w1ts = mdat;  // rising edge while CLK high latches the frame
  delayTick();
  GPIO.out_w1tc = mdat;
  delayTick();
  GPIO.out_w1tc = mclk;
  delayTick();
}

// PANEL MAP:
// Each digit is one driver plus one pair of COM pairs, ten LED positions.
// Driver 1 carries the hours, driver 2 the minutes.
struct Block {
  uint8_t drv;
  uint8_t comLo;
  uint8_t comHi;
};
static const Block BLOCKS[4] = {
    {0, 0x30, 0xC0},  // digit 1, hour tens,   annunciator AM
    {0, 0x03, 0x0C},  // digit 2, hour ones,   annunciator colon
    {1, 0x30, 0xC0},  // digit 3, minute tens, annunciator date dash
    {1, 0x03, 0x0C},  // digit 4, minute ones, annunciator degree mark
};

// Segment to position within a block. Side 0 is COM low, side 1 is COM high.
struct SegPos {
  uint8_t side;
  uint8_t led;
};
// Order: A B C D E F G, then the annunciator and the block 2 second colon dot.
static const SegPos SEGMAP[9] = {
    {1, 5},  // A top
    {1, 4},  // B top right
    {1, 3},  // C bottom right
    {1, 2},  // D bottom
    {0, 5},  // E bottom left
    {0, 4},  // F top left
    {0, 3},  // G middle
    {1, 1},  // annunciator
    {0, 1},  // second annunciator, block 2 only
};
enum { SEG_ANNUN = 7, SEG_ANNUN2 = 8 };

// Seven segment font, bit 0 = A through bit 6 = G.
static uint8_t glyph(char c) {
  switch (c) {
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
    case 'F': return 0b1110001;
    case 'C': return 0b0111001;
    default:  return 0;
  }
}

static int comIndex(uint8_t cs) {
  for (int i = 0; i < 4; i++) {
    if (COM_SEQ[i] == cs) return i;
  }
  return 0;
}

// SEG0 is not part of any LED and nothing on the panel responds to it. The
// stock lamp test drives it anyway, so this flag exists only to reproduce
// that quirk.
static bool g_seg0 = false;

static void clearPanel() {
  memset(g_level, 0, sizeof(g_level));
  g_seg0 = false;
}

// Light one position at the given per-channel levels.
static void setPos(uint8_t blockIdx, uint8_t segIdx, uint8_t r, uint8_t g, uint8_t b) {
  const Block &blk = BLOCKS[blockIdx];
  const SegPos &sp = SEGMAP[segIdx];
  uint8_t cs = sp.side ? blk.comHi : blk.comLo;
  int ci = comIndex(cs);
  g_level[blk.drv][ci][sp.led][CH_RED] = r;
  g_level[blk.drv][ci][sp.led][CH_GREEN] = g;
  g_level[blk.drv][ci][sp.led][CH_BLUE] = b;
}

// Write one character into a digit at the given color.
static void setDigit(uint8_t blockIdx, char c, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t bits = glyph(c);
  for (int s = 0; s < 7; s++) {
    if (bits & (1 << s)) {
      setPos(blockIdx, s, r, g, b);
    } else {
      setPos(blockIdx, s, 0, 0, 0);
    }
  }
}

static void setAM(uint8_t r, uint8_t g, uint8_t b) { setPos(0, SEG_ANNUN, r, g, b); }
static void setDash(uint8_t r, uint8_t g, uint8_t b) { setPos(2, SEG_ANNUN, r, g, b); }
static void setDegree(uint8_t r, uint8_t g, uint8_t b) { setPos(3, SEG_ANNUN, r, g, b); }

static void setColon(uint8_t r, uint8_t g, uint8_t b) {
  setPos(1, SEG_ANNUN, r, g, b);
  setPos(1, SEG_ANNUN2, r, g, b);
}

// Collapse the level array into sub-frames. A COM pair gets two sub-frames
// only if something in it is at half duty.
static void render() {
  for (int d = 0; d < 2; d++) {
    for (int ci = 0; ci < 4; ci++) {
      uint16_t full = 0, half = 0;
      bool needsHalf = false;
      for (int led = 1; led <= 5; led++) {
        int base = 3 * led - 2;  // LED1 is SEG1..SEG3, LED5 is SEG13..SEG15
        for (int ch = 0; ch < 3; ch++) {
          uint8_t lv = g_level[d][ci][led][ch];
          if (lv == 0) continue;
          uint16_t bit = 1u << (base + ch);
          half |= bit;             // on in the first sub-frame
          if (lv >= 2) full |= bit; // on in both
          if (lv == 1) needsHalf = true;
        }
      }
      if (g_seg0) {
        half |= 1;
        full |= 1;
      }
      noInterrupts();
      if (needsHalf) {
        g_ss[d][ci][0] = half;
        g_ss[d][ci][1] = full;
        g_nsub[d][ci] = 2;
      } else {
        g_ss[d][ci][0] = half;
        g_nsub[d][ci] = 1;
      }
      interrupts();
    }
  }
}

// SCAN:
// A periodic esp_timer, not a busy loop in a task. A task that never yields
// starves whichever core it is pinned to, and on core 1 that is the Arduino
// loop itself.
//
// Eight ticks per cycle, two per COM pair. A pair with a single sub-frame
// simply holds its latched output through the second tick, so the frame count
// per cycle comes out at four when nothing is dithered and six when two pairs
// are, which is what the 8051 does.
static const uint32_t TICK_US = 301;  // 8 x 301 = 2408us, 415.3Hz
static volatile uint8_t g_tick = 0;

static void IRAM_ATTR scanTick(void *arg) {
  (void)arg;
  uint8_t t = g_tick;
  g_tick = (uint8_t)((t + 1) & 7);

  int ci = t >> 1;   // COM pair, each gets two consecutive ticks
  int sub = t & 1;   // sub-frame within that pair
  uint8_t cs = COM_SEQ[ci];

  // A driver with fewer sub-frames stays latched rather than being re-sent.
  if (sub < g_nsub[0][ci]) sendFrame(false, g_ss[0][ci][sub], cs, g_current);
  if (sub < g_nsub[1][ci]) sendFrame(true, g_ss[1][ci][sub], cs, g_current);
}

// TEST PATTERNS:
static void patternLampTest() {
  clearPanel();
  for (int d = 0; d < 2; d++)
    for (int ci = 0; ci < 4; ci++)
      for (int led = 1; led <= 5; led++)
        g_level[d][ci][led][CH_RED] = 2;
  g_seg0 = true;  // stock drives the unwired spare sink too
  // The stock power-on test runs at IS 0xF, about 485mA against a 250mA board
  // rating, and gets the drivers hot within a minute. Stock only holds it for
  // a moment. Raise this with '+' if you want to see it, and do not leave it
  // there.
  g_current = 0x5;
  render();
}

static void pattern212am() {  // 2:12 AM in red, colon lit
  clearPanel();
  setDigit(0, ' ', 0, 0, 0);
  setDigit(1, '2', 2, 0, 0);
  setDigit(2, '1', 2, 0, 0);
  setDigit(3, '2', 2, 0, 0);
  setAM(2, 0, 0);
  setColon(2, 0, 0);
  g_current = 0xD;
  render();
}

static void pattern1234() {  // four digits, four colors
  clearPanel();
  setDigit(0, '1', 2, 0, 0);  // red
  setDigit(1, '2', 0, 2, 0);  // green
  setDigit(2, '3', 0, 0, 2);  // blue
  setDigit(3, '4', 2, 0, 1);  // purple, blue at half duty
  g_current = 0xF;
  render();
}

static void patternDash6() {  // the stock week display, -6-
  clearPanel();
  setDigit(1, '-', 0, 0, 2);
  setDigit(2, '6', 0, 0, 2);
  setDigit(3, '-', 0, 0, 2);
  g_current = 0xD;
  render();
}

// Walk one position at a time. Not a stock pattern. This is the tool for
// checking the map against a real panel, one LED per step, and the fastest way
// to find out which positions a given board actually has fitted.
static volatile bool g_walk = false;
static int g_walkStep = 0;

static void patternWalkStep() {
  clearPanel();
  int n = g_walkStep % 40;
  int d = n / 20;
  int ci = (n % 20) / 5;
  int led = (n % 5) + 1;
  g_level[d][ci][led][CH_RED] = 2;
  g_level[d][ci][led][CH_GREEN] = 2;
  g_level[d][ci][led][CH_BLUE] = 2;
  render();
  Serial.printf("walk %2d: driver %d  COM 0x%02X  LED%d\n", n, d + 1, COM_SEQ[ci], led);
}

static void help() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  l  lamp test, all red, IS 0x5");
  Serial.println("  t  2:12 AM red with colon");
  Serial.println("  d  1234 in red green blue purple");
  Serial.println("  6  week display -6- in blue");
  Serial.println("  w  toggle single position walk, one step per second");
  Serial.println("  n  next walk step");
  Serial.println("  +  raise IS, -  lower IS");
  Serial.println("  ?  this list");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_CLK_1, OUTPUT);
  pinMode(PIN_DATA_1, OUTPUT);
  GPIO.out_w1tc = M_CLK | M_DATA | M_CLK_1 | M_DATA_1;

  // The part discards anything sent in the first 200us after power up.
  delay(10);

  clearPanel();
  render();

  esp_timer_create_args_t targs = {};
  targs.callback = &scanTick;
  targs.arg = nullptr;
  targs.dispatch_method = ESP_TIMER_TASK;
  targs.name = "scan";
  esp_timer_handle_t scanTimer = nullptr;
  esp_timer_create(&targs, &scanTimer);
  esp_timer_start_periodic(scanTimer, TICK_US);

  Serial.println("HU-058D panel test");
  pattern212am();
  help();
}

void loop() {
  static uint32_t lastWalk = 0;

  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'l': patternLampTest(); Serial.println("lamp test"); break;
      case 't': pattern212am(); Serial.println("2:12 AM red"); break;
      case 'd': pattern1234(); Serial.println("1234 multicolor"); break;
      case '6': patternDash6(); Serial.println("-6- blue"); break;
      case 'w': g_walk = !g_walk; Serial.printf("walk %s\n", g_walk ? "on" : "off"); break;
      case 'n': g_walkStep++; patternWalkStep(); break;
      case '+': if (g_current < 0xF) g_current++; Serial.printf("IS 0x%X\n", g_current); break;
      case '-': if (g_current > 0) g_current--; Serial.printf("IS 0x%X\n", g_current); break;
      case '?': help(); break;
      default: break;
    }
  }

  if (g_walk && millis() - lastWalk > 1000) {
    lastWalk = millis();
    g_walkStep++;
    patternWalkStep();
  }
}
