# Example 21 — Cooperative bit-bang with `tiku_crit_begin_defer`

Companion to **Example 20**. Same hardware setup, same pattern, same
1 Hz cadence — the only difference is which flavour of `tiku_crit`
wraps the bit-bang. This example uses **`tiku_crit_begin_defer()`**:
no peripheral IE bit is touched, every other ISR keeps firing, and
the only kernel suppression is the software-timer dispatcher.

## Why this example exists

`tiku_crit` ships with two modes deliberately:

| | Example 20 (`tiku_crit_begin`) | Example 21 (`tiku_crit_begin_defer`) |
|---|---|---|
| Peripheral IEs cleared | yes (selectively) | **no** |
| Bit-edge jitter floor | < 1 µs (htimer ISR latency only) | 3–5 µs per other ISR firing during packet |
| UART RX during packet | dropped (unless `PRESERVE_UART`) | **preserved** |
| ADC done during packet | deferred | **preserved** |
| GPIO edge during packet | collapsed to one | **preserved** |
| When to use | high-rate / strict-timing protocols | slow protocols where the rest of the system must keep working |

This example showcases the second column.

## Hardware

Identical to Example 20: bit-bang pin is `TIKU_BOARD_BSCAT_PORT.PIN`
(P1.4 on the FR5969 LaunchPad, header J1.5).

## Build & flash

```bash
make MCU=msp430fr5969 HAS_EXAMPLES=1 TIKU_EXAMPLE_CRIT_DEFER=1
make flash MCU=msp430fr5969
```

## Demonstrating the trade-off

The clearest A/B is around **UART RX survival**:

1. Flash Example 20. Open `make monitor` in another terminal. Type
   continuously. Roughly once per second a typed character is
   silently dropped — that's the ~1.6 ms window where UART RX IE
   is masked.
2. Reflash with Example 21. Repeat. **No characters are dropped.**

The cost shows up on the bit-edge timing if anything is firing ISRs
heavily during the window:

3. With Example 21 still loaded, hold a key in `make monitor` (the
   shell will be processing UART chars at ~1 ms intervals at 9600
   baud). Capture the bit-bang pin on a logic analyzer. Bit edges
   pick up 3–5 µs of jitter wherever a UART RX byte landed inside
   the packet.

That asymmetry is exactly the trade-off the two-mode design
captures: under `_defer` the system stays responsive at the cost
of edge precision; under `_begin` precision is bounded by the
htimer ISR alone.

## Success criteria

1. LED1 toggles once per second (same as Example 20).
2. The 16-bit pattern appears on the bit-bang pin once per second
   (same as Example 20).
3. The total packet width is **slightly noisier** than Example 20
   when the system is otherwise busy. With an idle system both
   modes look the same.
4. UART input is **never dropped** during a packet (the load-bearing
   difference vs Example 20).
