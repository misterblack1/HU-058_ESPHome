# Hardware

Board marking: HU-058D, V1.0.240516. PCB 145 x 44mm, 5V, 250mA. (ESP8266 version.)
Maker: Dongguan Chuanglong Electronics.

Included schematic, `images/schematic-hu058d.jpg`, and
cross-checked against the redrawn schematic for the non-WiFi HU-058 in
`images/schematic-hu058.jpg`. 

## Architecture

An STC8G1K17 8051 MCU runs everything: it scans the display, reads the two
buttons, reads the NTC for room temperature and the LDR for ambient light, and
drives the buzzer.

The ESP-01S is a WiFi coprocessor on its own socket. All it does is feed time
to the 8051 over a serial link.

```
USB 5V -> AMS1117-3.3 -> 3V3 -> ESP-01S (P5 socket)
                                  | GPIO2 (U1TX) --330R--> P3.6  MCU RX
                                  | GPIO3 (RXD)  <-330R--- P3.7  MCU TX
STC8G1K17-38I-DIP16 -> P1 4-pin display connector: CLK/DATA and CLK_1/DATA_1
```

The serial protocol between the two is undocumented, including its baud rate.
I didn't bother to capture these signals, since they were irrelevant.

## STC8G1K17-38I-DIP16 pinout

| Pin | Port | Net | Function |
| --- | --- | --- | --- |
| 1 | P1.0/ADC0/CCP1/RxD2 | CLK_1 | Display bus 2 clock |
| 2 | P1.1/ADC1/CCP0/TxD2 | DATA_1 | Display bus 2 data |
| 3 | P1.6/ADC6 | R_T | NTC divider, R2 10K to +5V, R4 NTC to GND |
| 4 | P1.7/ADC7 | R_P | LDR divider, R1 10K to +5V, R3 LDR to GND |
| 5 | P5.4/RST/MCLKO | DATA | Display bus 1 data |
| 6 | VCC | +5V | Supply |
| 7 | P5.5 | P5.5 | Not connected |
| 8 | GND | GND | Ground |
| 9 | P3.0/ADC8/RXD/INT4 | S1 | Button SW2 to GND, also ISP RX |
| 10 | P3.1/ADC9/TXD | S2 | Button SW1 to GND, also ISP TX |
| 11 | P3.2/ADC10/INT0 | R_B | Buzzer, R5 10K to Q1 S8550 base |
| 12 | P3.3/ADC11/INT1 | - | No connection |
| 13 | P3.4/ADC12/T0 | - | No connection |
| 14 | P3.5/ADC13/T2 | CLK | Display bus 1 clock |
| 15 | P3.6/ADC14/INT2 | RXD_2 | From ESP GPIO2 through R7 330R |
| 16 | P3.7/INT3 | TXD_2 | To ESP RXD through R6 330R |

The hardware UART2 pins, P1.0 and P1.1, are spent on the display, so the link
to the ESP sits on P3.6 and P3.7 instead. Those two are INT2 and INT3, which
is what a software UART uses for start-bit detection.

## ESP-01S socket P5

| Pin | Signal | Net |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | IO2 | U1TX, to MCU P3.6 |
| 3 | IO0 | No labeled net |
| 4 | RXD | U0RX, from MCU P3.7 |
| 5 | VCC | 3V3 |
| 6 | RST | 3V3 |
| 7 | CH_PD | 3V3 |
| 8 | TXD | Stub, no net |

The ESP talks out of GPIO2 as UART1 TX and listens on GPIO3 as UART0 RX,
which is the usual trick for keeping ROM bootloader chatter on GPIO1 out of
the MCU's receiver. GPIO1 is not brought out, so nothing on the board can hear
the ESP's UART0 output.

## Other parts

| Part | Value | Role |
| --- | --- | --- |
| U3 | AMS1117-3.3 | 5V to 3V3 for the ESP, C4 and C5 10uF |
| Q1 | S8550 PNP | Buzzer driver, LS1 magnetic buzzer |
| USB1 | Micro USB 2P | 5V in, power only, no data pins |
| P3 | Header 4 | +5V, S2, S1, GND |
| P1 | Display connector | CLK, DATA, CLK_1, DATA_1 |

There is no RTC and no battery on this revision, so every power cut loses the
time until the network comes back.

## Display

Four digits with a colon, AM indicator, degree indicator, and a dash in the middle of the color built from 33 common-anode 0603 RGB LEDs. Opposite side of the board sit two **AiP33628** drivers in SSOP-28, U1 and U2, one per two-wire bus. Neither appears on the HU-058D schematic, because both live on the front
side of the board, which ships preassembled. The marking reads `AiP 33628`
over lot code `31AD503`.

The AiP33628 is an 8 x 16 common-anode constant-current matrix driver from
Wuxi I-CORE. Clock and data only, no strobe and no command set. It is a 30-bit
shift register in front of an output latch, so the MCU owns the scan, the
brightness and the color mixing. Frame format, latch behavior and current
levels are in `aip33628-protocol.md`.

AiP is Wuxi I-CORE, and most of their catalog is TM-series clones. The
AiP33628 is not one of them. It shares nothing with the TM1640 command set.

The two buses are independent, so the drivers can be written in parallel or
one after the other, whichever suits the firmware.

Driver 1 carries the two hour digits and driver 2 the two minute digits. The
full COM and SEG to segment table is in `display-map.md`.

### Panel layout

| Group | LEDs | Notes |
| --- | --- | --- |
| Digits 1 to 4 | 28 | Seven segments each |
| Colon | 2 | Between digits 2 and 3 |
| Dash | 1 | Same gap as the colon, the date separator in `01-01` |
| AM | 1 | Inside the top of digit 1 |
| Degree | 1 | Single period, the degree mark for the temperature |

That is 33 positions. The drivers can address 40, so seven addressable
positions are unwired. The HU-058 parts list says 36 RGB LEDs, which matches
neither figure.

Only one indicator exists for 12 hour mode, am AM indicator. 

Physical coordinates for every position are in `led-layout.md`.

## HU-058D versus HU-058

The HU-058 is the same clock without WiFi. Same STC8G1K17-38I in a DIP-16 socket, same
buttons, buzzer, LDR, NTC and display.

What the D revision changes:

| HU-058 | HU-058D | Effect |
| --- | --- | --- |
| DS1302N RTC on P3.3, P3.4, P3.5 | Deleted | Timekeeping is software plus NTP |
| Y1 32.768kHz crystal, C5, C6 22pF | Deleted | - |
| CR1220 cell and holder | Deleted | Time is lost on every power cut |
| Display CLK on P3.7 | Moved to P3.5 | Frees P3.6 and P3.7 |
| P3.6, P3.7 unused | ESP-01S link | The WiFi half |
| No regulator | AMS1117-3.3, C4, C5 10uF | 3V3 for the ESP |

## Why neither stock chip gets used

The obvious cheaper plan is to keep the 8051 and reflash it.

**The stock 8051 firmware cannot be dumped.** The STC ISP protocol has no read
command. That is a missing feature rather than a fuse someone left set, so
STC's own Windows tool cannot read a chip either, and neither can `stcgal` or
`stc8prog`.

Flashing the MCU erases the stock firmware permanently. Develop on a spare if
you want to keep an original working. The part is a socketed DIP-16 and costs
about a dollar.

A universal programmer will not touch it either. The STC8G has no parallel or
SPI programming mode, and the UART bootloader is the only way in, so a socket
programmer has nothing to talk to. The XGecu T48 cannot program these MCUs.
