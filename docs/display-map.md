# Display map

The COM and SEG to segment table for both AiP33628 drivers. This is the
document you need to drive this panel from anything.

Frame format, latch rules and current levels are in `aip33628-protocol.md`.

The map came out of logic analyzer captures of the stock firmware. Every
position was then driven from an ESP32 and checked against the panel.

## LED triples

Each RGB LED occupies three consecutive SEG lines. The triples are offset by
one, so SEG0 is not part of any LED and never carries real content.

| LED | SEG blue | SEG green | SEG red |
| --- | --- | --- | --- |
| LED1 | 1 | 2 | 3 |
| LED2 | 4 | 5 | 6 |
| LED3 | 7 | 8 | 9 |
| LED4 | 10 | 11 | 12 |
| LED5 | 13 | 14 | 15 |

Channel is the SEG index modulo 3. Remainder 1 is blue, 2 is green, 0 is red.
Five RGB LEDs per COM pair, sixteen SEG lines with SEG0 spare.

The stock power-on lamp test drives SEG0 along with every red line, so the
chip has the sink and the stock firmware knows about it. Nothing on the panel
responds.

## COM pairs

COM outputs are driven in shorted pairs, giving a 1/4 duty scan on a 4 row
array. Four pairs per driver.

| CS value | COM lines |
| --- | --- |
| 0x03 | COM0, COM1 |
| 0x0C | COM2, COM3 |
| 0x30 | COM4, COM5 |
| 0xC0 | COM6, COM7 |

## Digit blocks

Each digit is one driver plus one pair of COM pairs, ten LED positions. The
four blocks partition the panel with nothing left over.

| Block | Digit | Driver | COM low | COM high | Annunciator |
| --- | --- | --- | --- | --- | --- |
| 1 | Hour tens | 1 | 0x30 | 0xC0 | AM |
| 2 | Hour ones | 1 | 0x03 | 0x0C | Colon, both dots |
| 3 | Minute tens | 2 | 0x30 | 0xC0 | Date dash |
| 4 | Minute ones | 2 | 0x03 | 0x0C | Degree mark |

Driver 1 carries the two hour digits, driver 2 the two minute digits.

The colon and the dash both sit physically in the gap between digit 2 and
digit 3, but they are wired into different blocks, the colon into block 2 and
the dash into block 3.

## Segment map

Every block follows the same pattern. This is the whole table.

| Position | Segment |
| --- | --- |
| COM low, LED1 | Second annunciator, block 2 only |
| COM low, LED2 | Unwired |
| COM low, LED3 | G, middle |
| COM low, LED4 | F, top left |
| COM low, LED5 | E, bottom left |
| COM high, LED1 | Annunciator |
| COM high, LED2 | D, bottom |
| COM high, LED3 | C, bottom right |
| COM high, LED4 | B, top right |
| COM high, LED5 | A, top |

Only block 2 uses the COM low LED1 position, for the lower colon dot. In the
other three blocks it is unwired.

Combining the two tables gives the absolute SEG line for any segment.

Segment A is LED5, so it is SEG13 blue, SEG14 green, SEG15 red. Segment D is
LED2, so SEG4, SEG5 and SEG6. The annunciator is always LED1, SEG1 through
SEG3.

## Position budget

| Category | Count |
| --- | --- |
| Addressable, 5 LEDs x 4 COM pairs x 2 drivers | 40 |
| Populated, 7 segments x 4 digits plus AM, colon x2, dash, degree | 33 |
| Unwired spares | 7 |

The spares are COM low LED2 in all four blocks, and COM low LED1 in blocks 1,
3 and 4. Nothing is known about whether pads exist for them.

## Scan

What the stock firmware ran, for reference. Nothing here is a requirement.

| Property | Value |
| --- | --- |
| Cycle period | 2407us |
| Refresh rate | 415.5Hz |
| Duty | 1/4, four COM pairs |
| Dwell per COM pair | 579 to 623us, not uniform across the four slots |
| Base frames per cycle | 4 per driver |

The stock COM order is 0x30, 0x0C, 0x03, 0xC0. Both drivers run off the same
scan timer, driver 1 first and driver 2 about 20us later.

## Brightness

Global, through `IS[3:0]` only. Timing never changes with brightness, so the
scan loop does not need to know the brightness setting.

| Stock setting | Wire | Step | Current |
| --- | --- | --- | --- |
| Maximum dim | 0x0 | 0 | 2.5mA |
| Normal | 0xD | 11 | 30.3mA |
| Brightest | 0xF | 15 | 40.4mA |

Wire and step differ because the current field is bit reversed in the frame.
See `aip33628-protocol.md`.

## Color

**Channel select.** A saturated color needs nothing but the right SEG lines.
Red, green, blue and any full-on combination such as white or yellow are a
single static frame.

**Sub-frame dithering.** For a mix that is not full on, send a COM pair more
than once per cycle with different SEG data, and light the channel in only
some of those sub-frames.

Sub-frame allocation is per driver and per COM pair, not global. A cycle can
run 4 frames on one driver and 6 on the other, and the two drivers can dither
different COM pairs in the same cycle.

Total frames per cycle is whatever the current content needs.

The stock firmware never sends a COM pair more than twice, and it does not
split the slot down the middle. The boundary lands at either a third or two
thirds, so the stock engine gets four duty cycle levels per channel from two
sub-frames: 0, 1/3, 2/3 and 1.

## How the ESPHome component does it

Each COM slot is subdivided into four binary weighted sub-frames of 40, 80,
160 and 320us, so the slot is 600us and a full cycle is 2400us, 416.7Hz.

A channel wanting duty level L is lit in sub-frame k whenever bit k of L is
set. Nothing requires a dimmer channel to be a subset of a brighter one, so
every level from 0 to 15 is reachable.

Four sub-frames buy sixteen levels that way, rather than the five that four
equal ones would.

Color is per digit at no cost. A driver and a COM pair together belong to
exactly one block, because the two blocks on a driver sit on different COM
pairs, so a slot only ever holds one block's color per driver, and the two
drivers carry their own SS word regardless.

Adjacent sub-frames carrying identical data merge into one longer step before
the schedule reaches the scan interrupt. A saturated color, where all four
sub-frames match, collapses to a single 600us step per COM pair, which is
exactly what a two level scan costs.

Implementation notes:

- The timer runs at a fixed 40us and the interrupt counts ticks.
  Reprogramming the alarm per step would take fewer interrupts, but an alarm
  set shorter than the counter has already reached never matches, so one late
  interrupt would freeze the panel until reboot. A fixed auto-reload alarm
  cannot do that.
- Both drivers are clocked from a single pass down the bits, writing the GPIO
  output registers directly. CS and IS are common to the two and only SS
  differs. A frame to both drivers takes 6.4us that way, against 28.0us for
  two passes through ESPHome's `ISRInternalGPIOPin`, which is what makes a
  40us sub-frame practical. It requires all four pins to sit below GPIO32,
  since one store only reaches the low output register, and the component
  rejects a higher pin at config time.

The scan runs from a `gptimer` interrupt rather than an `esp_timer`. The
esp_timer task dispatch path runs at task priority on core 0 next to the WiFi
task, which preempts it and stretches whichever COM slot is lit at the time. A
full duty slot is 600us and rides that out, but a 40us sub-frame does not, and
the jitter reads as uneven digits and a visible pulse on any color that is not
saturated.

## Current and heat

The drivers are constant-current sinks, so `IS` sets the current per lit SEG
regardless of how many are lit. Total board current is the number of lit sinks
times that current, and it does not fall with the scan duty, because some COM
pair is always active.

| State | Sinks per driver | Both drivers |
| --- | --- | --- |
| Lamp test, all red, IS 0xF | 6 | 485mA |
| Full white, IS 0xF | 15 | 1.2A |
| Typical time display, step 11 | 2 to 4 | 121 to 242mA |

The vendor rates the board at 250mA. The lamp test is already about double
that and full white is roughly five times.

The dissipation lands in the driver, not the LED. Each sink drops VDD minus
the LED forward voltage, so a red LED at 5V VDD puts about 3V across the sink.

Six red sinks at IS 0xF is around 0.73W in an SSOP-28, and full white is
around 1.4W. The lamp test gets the board noticeably hot within a minute. At
IS 0x5 the same lamp test drops to about 0.27W per driver.

The stock firmware did not respect the 250mA label either. It ran 404mA with a
white digit lit and 485mA during its power-on test.

### Set the current from brightness alone

Never derive `IS` from the lit sink count. Doing that re-dims the entire panel
every time the colon blinks, which reads as a one hertz pulse, and it makes
picking a color change the brightness of everything. It also stalls part way
through a fade.

Use a fixed ceiling instead and let the user lower it.

The stock firmware held `IS` at 0xD across colon on and colon off, and set it
from the user's brightness setting alone, never from what was on screen.

Treat any all-on state as momentary. The stock firmware only lights everything
during its power-on test.
