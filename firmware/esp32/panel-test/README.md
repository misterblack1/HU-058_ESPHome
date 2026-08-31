# Bare-metal panel firmware

Drives the HU-058D display from an ESP32-WROOM-32 with nothing but the Arduino
core. 

Everything specific to this board sits in tables near the top of
`src/main.cpp`, and the only hardware-dependent code is the four GPIO writes
inside `sendFrame`. It is also the fastest way to find out which LED positions
your particular board has fitted.

The mapping it uses is documented in `../../../docs/display-map.md`.

The color scheme here is the simple one: three duty levels per channel, from
at most two sub-frames per COM pair, which is close to what the stock 8051
does. The ESPHome component uses four binary weighted sub-frames and gets
sixteen levels. Start here, then read that if you want more colors.

## Wiring

Four signals and a ground, into the empty DIP-16 MCU footprint. Do not install
the 8051 or the ESP-01S, and pull them if you already did. Two devices on one
bus will fight.

Full build notes are in `../../../docs/wiring.md`.

| ESP32 | Socket pin | Net | Use |
| --- | --- | --- | --- |
| GPIO22 | 14 | CLK | Driver 1 clock |
| GPIO21 | 5 | DATA | Driver 1 data |
| GPIO19 | 1 | CLK_1 | Driver 2 clock |
| GPIO18 | 2 | DATA_1 | Driver 2 data |
| GND | 8 | GND | Common ground |

The VIN on the ESP32 can be connected directly to the USB jack on the clock, so either USB will power both components. 

## Build

```
pio run -t upload
pio device monitor
```

## Commands

Single keypresses over the serial monitor at 115200.

| Key | Pattern |
| --- | --- |
| `l` | Lamp test, all red, IS 0x5 |
| `t` | 2:12 AM in red with the colon lit |
| `d` | 1234 in red, green, blue and purple |
| `6` | Week display `-6-` in blue |
| `w` | Toggle single position walk, one step per second |
| `n` | Next walk step |
| `+` `-` | Raise or lower IS |
| `?` | This list |

Walk: It lights one of the 40 addressable positions at a
time and prints which driver, COM pair and LED it is driving, so you can read
your board's real population straight off the panel.


## Levels

The 3.3V outputs of an ESP32 drive the panel perfectly without a level shifter

If levels ever are marginal the symptom is not a dead display but an
intermittent one, because the latch triggers on a data edge while the clock is
high. Watch for flicker, a wrong digit for one frame, or garbage that clears
itself. A 74AHCT125 on the four lines is the fix.

## Current draw

The sinks are constant current, so total draw is the number of lit sinks times
`IS` and does not fall with the scan duty. The vendor rates the board at
250mA.

| State | Both drivers | Per driver dissipation |
| --- | --- | --- |
| Lamp test, all red, IS 0x5 | 180mA | 0.27W |
| Lamp test, all red, IS 0xF | 485mA | 0.73W |
| Full white, IS 0xF | 1.2A | 1.4W |

The heat lands in the driver rather than the LED, since each sink drops VDD
minus the LED forward voltage. Do not leave the panel at a high `IS` with most
of it lit.

Nothing here commands full white, but the level array can express it.
