# Home Assistant reference

Needs ESPHome 2026.8.0 or newer.

## Naming

The device is `wifi-clock`, so entities are `<domain>.wifi_clock_<object_id>`
and actions are `esphome.wifi_clock_<name>`.

## Actions

In YAML they are called with `action:` and `data:`.

| Action | Parameters |
| --- | --- |
| `show_seconds` | `duration` int, ms |
| `show_number` | `value` int, `unit` string, `duration` int ms |
| `set_digit_color` | `digit` int, `red` `green` `blue` int 0-255 |
| `set_position_color` | `position` int 1-33, `red` `green` `blue` int 0-255 |
| `set_gradient` | `red1` `green1` `blue1`, `red2` `green2` `blue2` int 0-255, `angle` float |
| `set_effect` | `mode` `spread` int, `speed` `angle` `hue_span` `flash_fade` float |
| `clear_digit_colors` | none |

### show_seconds

Shows `:SS` in the minute positions behind a steady colon, then goes back to
the time on its own.

`duration` is milliseconds, clamped to between 100 and 600000. Nothing can
leave the clock stuck not being a clock.

### show_number

Right aligned digits with an optional unit letter in the rightmost position,
so `78F` and `-5C` both fit. An empty `unit` gives the number all four
positions.

There is no decimal point on the panel, so round before sending.

Only these letters render unambiguously on seven segments, and anything else
is ignored: `A b C c d E F H h L n o P r t U u y`.

Out of range values clamp rather than wrap, because a wrapped temperature is
a wrong reading and a clamped one is visibly pinned.

On this board, a number wide enough to reach the leftmost position renders 0,
2, 6 and 8 broken there, since that digit is missing its lower left segment.

### set_digit_color

`digit` is 0 for the leftmost through 3 for the rightmost, or -1 for all four
at once.

Components are not rescaled against each other, so `128,0,0` really is a
dimmer red than `255,0,0`, not the same red. A digit can be dimmer than its
neighbors as well as a different hue.

Each digit carries its own annunciator. The colon takes the color of the hour
ones digit, the AM mark the hour tens, and the date dash the minute tens.

### set_position_color

One LED, by the id on the board layout map. Ids and coordinates are in
`led-layout.md`.

Briefly: 1-7 left digit, 8 the AM mark, 9-15 second digit, 16 and 18 the colon
dots, 17 the date dash, 19-25 third digit, 26 the degree mark, 27-33 right
digit.

Within a digit the walk is A, F, G, E, D, C, B, so the first id of each digit
is its top segment.

Ids 4 and 26 have no LED fitted on this board. Setting them is harmless and
does nothing visible.

### set_gradient

A linear ramp between two colors across the whole panel. Both colors land
exactly on the outermost LEDs whichever way the ramp points.

### set_effect

Sets every effect setting at once. It drives the entities below rather than
the component, so the Home Assistant UI ends up showing what the panel is
actually doing.

Values outside their range are clamped.

| Parameter | Values |
| --- | --- |
| `mode` | 0 off, 1 color cycle, 2 flash |
| `spread` | 0 whole panel, 1 per digit, 2 per LED |
| `speed` | Seconds for one full cycle, 0.2 to 600 |
| `angle` | Degrees, -180 to 180 |
| `hue_span` | Degrees of the color wheel across the panel, 0 to 360 |
| `flash_fade` | Seconds per flash transition, 0 to 30, 0 snaps |

### clear_digit_colors

Hands the whole panel back to the `Display` light, per position colors and
gradients included.

## Entities

| Entity | Type | Values |
| --- | --- | --- |
| `light.wifi_clock_display` | Light | RGB, on/off, brightness, color |
| `switch.wifi_clock_12_hour_time` | Switch | On for 12h, off for 24h |
| `switch.wifi_clock_blink_colon` | Switch | Off means steady, not dark |
| `select.wifi_clock_effect` | Select | None, Color Cycle, Flash |
| `select.wifi_clock_effect_spread` | Select | Whole Panel, Per Digit, Per LED |
| `number.wifi_clock_effect_speed` | Number | 0.2 to 600 s, step 0.2 |
| `number.wifi_clock_effect_angle` | Number | -180 to 180 deg, step 15 |
| `number.wifi_clock_effect_hue_span` | Number | 0 to 360 deg, step 5 |
| `number.wifi_clock_flash_fade` | Number | 0 to 30 s, step 0.05 |
| `binary_sensor.wifi_clock_button_1` | Binary sensor | Top button on the case |
| `binary_sensor.wifi_clock_button_2` | Binary sensor | Bottom button |
| `button.wifi_clock_show_seconds` | Button | Five seconds of `:SS` |
| `button.wifi_clock_diagonal_gradient` | Button | Green to yellow, -45 |
| `button.wifi_clock_digit_color_demo` | Button | Four digits, four colors |
| `button.wifi_clock_clear_digit_colors` | Button | Back to the Display light |
| `number.wifi_clock_test_number` | Number | Test handle for `show_number` |
| `select.wifi_clock_test_unit` | Select | None, F, C |

The step on the number entities only affects the arrows in the UI. An
automation, or `set_effect`, can pass any value in range.

Effect settings restore across a reboot, and the effect resumes.

The color tiers do not. Digit, position and gradient colors are runtime state,
and a reboot hands the panel back to the Display light.

## Angles

Shared by `set_gradient` and by the effect spread.

| Angle | Runs |
| --- | --- |
| 0 | Left to right |
| 45 | Top left to bottom right |
| 90 | Top to bottom |
| 135 | Top right to bottom left |
| 180 | Right to left |
| -45 | Bottom left to top right |
| -90 | Bottom to top |
| -135 | Bottom right to top left |

Any angle works, not just these.

## How color resolves

Three tiers. The most specific wins, and setting a broader one drops the
finer ones inside it.

| Tier | Set by | Clears |
| --- | --- | --- |
| One LED | `set_position_color`, `set_gradient` | nothing |
| One digit | `set_digit_color` | per LED colors in that digit |
| Whole panel | The `Display` light's color | both finer tiers |

Changing only the light's brightness clears nothing, because a transition
calls it on every step and would otherwise wipe a gradient mid fade.

A running effect writes the per LED tier itself, so it sits on top of
everything until it is turned off.

Turning it off clears that tier rather than uncovering what was under it, so a
gradient set before a flash is gone once the flash ends. Digit colors survive,
the effect never touches that tier.

Global brightness is always the `Display` light. Effects and colors set duty
cycle levels, brightness sets drive current, and the two are independent.

## Hue span

A full 360 crams the entire spectrum into four digits, reads as noise, and
lands the leftmost and rightmost LEDs on the same hue. Narrow is the useful
range.

Only Color Cycle uses the span, and only when Spread is not Whole Panel, which
puts every LED on the same phase.

The wheel is full saturation, so a cycle ignores the color set on the Display
light.

| Span | Panel shows |
| --- | --- |
| 0 | One color everywhere, still cycling |
| 40 | Red to orange, a tight slice |
| 90 | Red through amber to yellow green |
| 180 | Half the wheel, red to cyan |
| 360 | Everything, and both ends the same |

## Flash

`speed` is the period and `flash_fade` is the transition time, independently.

| speed | flash_fade | Result |
| --- | --- | --- |
| 1.0 | 0 | 1Hz, snaps hard |
| 1.0 | 0.1 | 1Hz with a 100ms ramp each way |
| 1.0 | 0.5 or more | 1Hz triangle, never sits still |
| 0.5 | 0 | 2Hz strobe |
| 4.0 | 2.0 | Slow breathe |

The ramps eat into the on and off halves rather than stretching the period, so
changing the fade never changes the rate. A fade longer than half the period is
capped there.

Flash multiplies whatever is already on the panel rather than replacing it, so
it flashes the current colors.

## Examples

Show a temperature for ten seconds:

```yaml
- action: esphome.wifi_clock_show_number
  data:
    value: "{{ states('sensor.outside_temperature') | round(0) | int }}"
    unit: F
    duration: 10000
```

Flash red three times at the front door, then go back to normal:

```yaml
- action: esphome.wifi_clock_set_digit_color
  data:
    digit: -1
    red: 255
    green: 0
    blue: 0
- action: esphome.wifi_clock_set_effect
  data:
    mode: 2
    spread: 0
    speed: 0.6
    angle: 0
    hue_span: 90
    flash_fade: 0
- delay: "00:00:02"
- action: esphome.wifi_clock_set_effect
  data:
    mode: 0
    spread: 0
    speed: 10
    angle: 0
    hue_span: 90
    flash_fade: 0
- action: esphome.wifi_clock_clear_digit_colors
```

A slow diagonal color cycle in the evening:

```yaml
- action: esphome.wifi_clock_set_effect
  data:
    mode: 1
    spread: 2
    speed: 45
    angle: -45
    hue_span: 60
    flash_fade: 0
```

A static sunset gradient, no animation:

```yaml
- action: esphome.wifi_clock_set_gradient
  data:
    red1: 255
    green1: 40
    blue1: 0
    red2: 60
    green2: 0
    blue2: 120
    angle: 0
```

Dim at night without disturbing the colors:

```yaml
- action: light.turn_on
  target:
    entity_id: light.wifi_clock_display
  data:
    brightness_pct: 15
```

Top button cycles through the effects:

```yaml
- triggers:
    - trigger: state
      entity_id: binary_sensor.wifi_clock_button_1
      to: "on"
  actions:
    - action: select.select_next
      target:
        entity_id: select.wifi_clock_effect
      data:
        cycle: true
```

## Things worth knowing

Temporary displays expire on their own. `show_seconds` and `show_number` both
carry a deadline capped at ten minutes, so a failed automation cannot park the
panel on a stale number.

There is no RTC. A cold boot with no network shows four dashes until the time
arrives.

The firmware polls from the ESPHome NTP default servers every 15 minutes: 0.pool.ntp.org, 1.pool.ntp.org, 2.pool.ntp.org. The timezone comes from Home Assitant. 

The upper colon dot drops while the network is down. It is not known how much time will drift while NTP servers can't be reached.

