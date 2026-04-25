# Example 20 — Hardware-timer GPIO bit-bang under `tiku_crit`

Drive a recognizable 16-bit pattern on a GPIO pin once per second,
with every edge fired from the hardware-timer ISR while a critical
window masks every other peripheral interrupt. This is the canonical
demo for `tiku_htimer` + `tiku_bitbang` + `tiku_crit`, and the
substrate that backscatter / software-UART / software-SPI build on.

## Hardware

| Signal | FR5969 LaunchPad | Notes |
|---|---|---|
| Bit-bang output | **P1.4** | BoosterPack header **J1.5** |
| Probe ground | GND | **J1.20** (next to J1.5) |
| Heartbeat | LED1 (red, P4.6) | Toggles once per packet |

Override the pin at compile time:
```
make ... EXTRA_CFLAGS="-DTIKU_BOARD_BSCAT_PORT=3 -DTIKU_BOARD_BSCAT_PIN=4"
```

## Build & flash

```bash
make MCU=msp430fr5969 HAS_EXAMPLES=1 TIKU_EXAMPLE_BITBANG=1
make flash MCU=msp430fr5969
```

`TIKU_EXAMPLE_BITBANG=1` auto-enables `TIKU_BITBANG_ENABLE=1`; you do
not need to pass it separately.

## Expected scope / logic-analyzer capture

- **Period**: one packet every 1.0 s.
- **Per-packet duration**: 16 bits × 100 µs/bit = **1.6 ms**.
- **Pattern (MSB-first)**:
  - byte 0 = 0xA5 = `1 0 1 0 0 1 0 1`
  - byte 1 = 0x5A = `0 1 0 1 1 0 1 0`
  - then idle low.
- **Bit period**: 100 µs ± edge jitter. With `TIKU_CRIT_PRESERVE_HTIMER`
  no other ISR can preempt the bit clock, so the jitter floor is the
  htimer ISR entry latency (a few CPU cycles ≈ < 1 µs at 8 MHz).

If you only see one or two edges and then nothing, the pin is most
likely held high or low because the htimer rejected the first
schedule (e.g. `bit_time_ticks` smaller than the htimer guard time).

## Success criteria

1. **Visual**: LED1 toggles once per second.
2. **Logic analyzer / scope on P1.4**:
   - Bursts repeat every 1.0 s.
   - Each burst is exactly 16 bits long.
   - Each bit is 100 ± 1 µs wide.
   - The bit pattern matches `1010 0101 0101 1010` (MSB first) before
     returning to idle.
3. **Concurrency check** (optional): hold `make monitor` open while
   the example runs. Shell input is paused for ~5 ms per packet
   because UART RX IE is masked inside the crit window. After
   `tiku_crit_end()` everything resumes; you should see no overall
   shell hang, just micro-pauses.

## What this example exercises

- `tiku_htimer` rescheduled drift-free from inside its own ISR via
  `next_edge += bit_time_ticks`.
- `tiku_htimer_set_no_guard` for back-to-back rescheduling at periods
  that fall under the standard guard time.
- `tiku_bitbang_t` config with MSB-first bit ordering and a
  caller-defined idle level.
- `tiku_crit_begin(max_us, TIKU_CRIT_PRESERVE_HTIMER)` masking
  Timer A0 (system tick), UART, ADC, I²C, watchdog interval and GPIO
  edge ISRs while leaving the bit clock alive.
- `on_done` callback running in ISR context, signalling completion
  to the foreground protothread by setting a `volatile uint8_t`.

## Related files

- `kernel/timers/tiku_bitbang.h` / `.c` — engine
- `kernel/timers/tiku_crit.h` / `.c` — critical window
- `kernel/timers/tiku_htimer.h` / `.c` — hardware timer (with
  `tiku_htimer_set_no_guard`)
- `interfaces/gpio/tiku_gpio.h` — raw pin API used by the engine
- `arch/msp430/boards/tiku_board_fr5969_launchpad.h` —
  `TIKU_BOARD_BSCAT_PORT` / `TIKU_BOARD_BSCAT_PIN`
