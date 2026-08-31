# Wiring an ESP32 into the clock

The kit ships as loose parts. The display half is already assembled on the
front of the board, so what you solder is the through-hole side, and the
easiest conversion is simply not installing the parts you are replacing.

Six wires go from an ESP32 to the MCU footprint. Four for the display, two
for the buttons, plus a ground.

## What not to install

| Skip | Why |
| --- | --- |
| The DIP-16 socket and the STC8G1K17 | The ESP32 takes over its pins |
| The ESP-01S and its socket | Nothing speaks to it any more |
| The DS1302, crystal and coin cell holder, on the HU-058 | Same |
| R1 and the LDR at R3 | Light sensor, out of scope |
| R2 and the NTC at R4 | Room temperature, out of scope |
| R5, Q1 and buzzer LS1 | Out of scope, but see below if you want it |

Install everything else, including both buttons and header P3. P3 is how the
ESP32 gets its power.

If you already built the clock, or bought one assembled, pull the two chips
instead. The 8051 levers out of its socket from both ends with a small flat
screwdriver, a little at a time, or it comes out bent. The ESP-01S lifts
straight out.

Keep the 8051. A stock chip cannot be re-created, because the STC ISP protocol
has no read command and nobody has a dump.

Both have to be out before you drive anything. Two devices on one bus will
fight.

## The MCU footprint

Every signal the panel needs lands on the DIP-16 footprint.

| Pin | Net | Use |
| --- | --- | --- |
| 1 | CLK_1 | Driver 2 clock |
| 2 | DATA_1 | Driver 2 data |
| 5 | DATA | Driver 1 data |
| 8 | GND | Common ground |
| 9 | S1 | Top button, switch to ground |
| 10 | S2 | Bottom button, switch to ground |
| 14 | CLK | Driver 1 clock |

Pin 1 is the end marked on the silkscreen.

On the HU-058, driver 1 clock is on pin 16 instead of pin 14, because that
board spends pin 14 on its DS1302.

The rest of the pinout, including the buzzer, the thermistor and the light
sensor, is in `hardware.md`.

## The wires

| ESP32 | Pin | Net |
| --- | --- | --- |
| GPIO22 | 14 | CLK, driver 1 |
| GPIO21 | 5 | DATA, driver 1 |
| GPIO19 | 1 | CLK_1, driver 2 |
| GPIO18 | 2 | DATA_1, driver 2 |
| GPIO32 | 9 | S1, top button |
| GPIO33 | 10 | S2, bottom button |
| GND | 8 | GND |

Solder straight into the empty through holes. That is what fits in the case,
and the board is still reversible, since a socket costs pennies if you ever
want the 8051 back.

If you would rather keep it stock, install the socket and use a DIP-16 test
clip or a machined-pin plug. Good for bench work, too tall for the case.

## Picking different GPIOs

Those six are plain, safe pins on a WROOM-32. If you move them, avoid GPIO0,
2, 12 and 15, which are strapping pins that change boot behavior, GPIO6
through 11, which are the flash, GPIO34 through 39, which are input only, and
GPIO1 and 3, which are the USB serial port.

The four display pins must all be below GPIO32 for the ESPHome component,
which writes both buses in one store to the low GPIO output register. It
rejects a higher pin at config time rather than failing silently.

The two button pins have no such limit.

## Buttons

Nothing on the board pulls the button nets up. The stock 8051 held them high
with its own quasi-bidirectional port pullup, and that chip is gone, so the
ESP32 internal pullup is the only one.

Switch closed shorts to ground, so the input is inverted and the lines never
see 5V.

## The buzzer, if you want it

It is out of scope in my firmware, but the parts are in the bag.

Pin 11 has to be driven open drain. It feeds a 10K into the base of a PNP
whose emitter sits at +5V, so a 3.3V high still leaves 1.7V across the
base-emitter junction and the buzzer screams. High impedance is off, low is
on.

## Power

The clock and the ESP32 share a 5V rail and a ground.

In the case, power the ESP32 from header P3, the 4-pin plug carrying +5V, S2,
S1 and GND. VIN to the +5V pin, GND to GND. The devkit's own regulator makes
3.3V for the module, and the clock's micro USB then feeds everything, LEDs,
drivers and ESP32 together.

On the bench, power each board from its own USB and tie the grounds together
through pin 8. Do not feed the clock's 5V into VIN while the ESP32 is also on
USB.

Plugging USB into the devkit while it is wired into the case back-feeds 5V
onto the clock rail, so the panel lights up when you are only trying to
program it. That is expected, not a fault.

## Logic levels

The 3.3V outputs of an ESP32 drive the panel perfectly. No level shifter, no
flicker, no dropped frames.

On paper they should not. The AiP33628 runs at 5V here and asks for 0.7 x VDD,
which is 3.5V. In practice the part switches near half rail, like most of
them.

If levels ever are marginal the symptom is not a dead display but an
intermittent one, because the latch triggers on a data edge while the clock is
high. Watch for flicker, a wrong digit for one frame, or garbage that clears
itself. A 74AHCT125 on the four display lines is the fix.

## Case fit

An ESP32-WROOM-32 devkit fits the 166 x 50 x 21mm case once the pin headers
come off. Double-sided tape on the back of the clock board holds it.

The devkit carries its own micro USB connector and regulator, so the finished
build has two USB ports on it. Only the clock's own is wired to anything.

## Current

The drivers are constant-current sinks. Total board draw is the number of lit
sinks times the per-sink current, and it does not fall with the scan duty,
because some COM pair is always active.

| State | Both drivers |
| --- | --- |
| All red at IS 0xF | 485mA |
| Full white at IS 0xF | 1.2A |
| Typical time display at current step 11 | 121 to 242mA |

The vendor rates the board at 250mA. A full lamp test is already about double
that and gets the drivers noticeably hot within a minute.

The heat lands in the driver rather than the LED, since each sink drops VDD
minus the LED forward voltage. Do not park the panel at a high current step
with most of it lit.

The `max_current` setting in `clock.yaml` is the ceiling. Lowering it is how
you run cooler.

## First light

Flash the bench firmware in `../firmware/esp32/panel-test/`, open the serial
monitor at 115200 and press `l` for a lamp test. Every populated LED should
come on red.

Then press `w` to walk one position at a time, which tells you exactly which
of the 40 addressable positions your board has fitted.
