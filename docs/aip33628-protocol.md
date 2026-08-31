# AiP33628 driver

Two drivers sit behind the display, one per two-wire bus. Everything below is
summarized from the vendor datasheet, `AiP33628-AX-XS-B091`, which is in
`../reference/AiP33628_icbase.pdf` 

It is not a TM1640 or TM1638. The chip is a 30-bit shift register in front of an output latch, so every
frame you send is the complete output state. All scanning, all brightness
ramping and all color mixing happen in the MCU.

## Part

8 x 16 common-anode constant-current matrix driver, up to 128 LEDs. Eight
high-side COM outputs, sixteen constant-current SEG sinks, 3.0V to 5.5V, up to
40mA per SEG. Wuxi I-CORE. The clock board uses SSOP28.

## Pinout, SOP28 and SSOP28

| Pin | Signal | Function |
| --- | --- | --- |
| 1-4 | COM3-COM0 | Anode outputs, active high, drives VDD |
| 5-12 | SEG15-SEG8 | Constant-current cathode sinks, active low |
| 13 | DIN | Serial data |
| 14 | CLK | Serial clock |
| 15 | GND | Ground |
| 16-23 | SEG7-SEG0 | Constant-current cathode sinks, active low |
| 24-27 | COM7-COM4 | Anode outputs, active high, drives VDD |
| 28 | VDD | Supply |


## Frame format

Thirty bits per frame, LSB first.

| Bits | Field | Meaning |
| --- | --- | --- |
| 0-15 | SS[15:0] | SEG15 to SEG0 on or off. 1 turns the current sink on. |
| 16-23 | CS[7:0] | COM7 to COM0 on or off. 1 drives that anode to VDD, 0 leaves it high impedance. |
| 24-27 | IS[3:0] | Constant-current level, applies to every SEG |
| 28-29 | Reserved | Must be 0 |

`SS` only gates the sink. It does not scale current.

**The current field is bit reversed against the rest of the frame.** The frame
carries `IS[0]` at bit 27 and `IS[3]` at bit 24, while `IS[3]` is the most
significant bit of the current value. A current step of 1 goes on the wire as
`0x8` and a step of 2 goes on as `0x4`.

| Step | Wire | Step | Wire | Step | Wire | Step | Wire |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0x0 | 4 | 0x2 | 8 | 0x1 | 12 | 0x3 |
| 1 | 0x8 | 5 | 0xA | 9 | 0x9 | 13 | 0xB |
| 2 | 0x4 | 6 | 0x6 | 10 | 0x5 | 14 | 0x7 |
| 3 | 0xC | 7 | 0xE | 11 | 0xD | 15 | 0xF |

The mapping is its own inverse, so the same table converts either way.

Steps 0 and 15 are mirrors, which means firmware that gets this wrong still
looks right at both ends of the brightness range and only scrambles the order
in the middle. Of everything in this document, this is the field most likely to
cause problems with brightness ramping.

## Constant-current levels

`IS[3:0]` sets the instantaneous current on every SEG output.

| IS | mA | IS | mA |
| --- | --- | --- | --- |
| 0x0 | 2.5 | 0x8 | 22.7 |
| 0x1 | 5.1 | 0x9 | 25.3 |
| 0x2 | 7.6 | 0xA | 27.8 |
| 0x3 | 10.1 | 0xB | 30.3 |
| 0x4 | 12.6 | 0xC | 32.8 |
| 0x5 | 15.2 | 0xD | 35.4 |
| 0x6 | 17.7 | 0xE | 37.9 |
| 0x7 | 20.2 | 0xF | 40.4 |

Average current through an LED is the level divided by the scan duty. At 1/8
duty, `IS=0xF` gives 40.4 / 8 = 5.05mA average.

## Latching

There is no strobe pin. The latch is driven by edges on DIN, and which edge
means what depends on the level of CLK.

| Event | Effect |
| --- | --- |
| Rising edge on CLK | Shift the current DIN level into the shift register |
| Rising edge on DIN while CLK is high | Load shift register into the output latch, apply new COM, SEG and current |
| Rising edge on DIN while CLK is low | Leave latch mode, latch holds, next frame can start shifting |

DIN must never change while CLK is high during a shift, or the chip latches a
half-written frame. Change data on the falling edge of CLK and hold it through
the rising edge.

Power-on reset clears the shift register and the latch. Do not talk to the chip
for the first 200us after power comes up, since anything sent in that window is
discarded.

## Timing

| Parameter | Limit |
| --- | --- |
| CLK frequency | 30MHz maximum |
| CLK high time | 16ns minimum |
| CLK low time | 16ns minimum |

No minimum clock rate is specified.

## What this means for the firmware

- The MCU runs the multiplex scan itself. Pick a COM pattern, send SEG data
  for it, latch, move on. Application note 3 in the datasheet shorts COM pairs
  to run 1/4 duty on a 4-row array, which is what the clock board does.
- Color comes from time, not from the chip. Each RGB LED is three SEG lines,
  so a color is a per-channel on-time inside the scan frame. Global brightness
  can lean on `IS[3:0]`, but per-LED color cannot.
- Refresh rate, scan duty and color depth trade against each other.

## On the clock board

The COM and SEG to segment table is in `display-map.md`, along with the scan
rate, the brightness mechanism and the color mixing. Short version:

- LEDs occupy SEG triples offset by one, so LED1 is SEG1 through SEG3 and SEG0
  is unused. Channel is the SEG index modulo 3, where 1 is blue, 2 is green
  and 0 is red.
- COM lines are driven in shorted pairs, 1/4 duty, four pairs per driver.
- Each digit is one driver plus one pair of COM pairs, ten positions. Driver 1
  holds the hour digits, driver 2 the minute digits.
- Thirty-three of the forty addressable positions are populated.

Brightness is `IS[3:0]` alone and never touches timing. A saturated color is a
single static frame. Anything else is dithered by sending a COM pair more than
once per cycle.
