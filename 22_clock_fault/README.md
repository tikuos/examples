# Example 22 — Clock-source fault diagnostic

Reports the `tiku_clock_fault()` state visually on LED1.

## Why this example exists

On FR5969 / FR5994 the system tick is sourced from XT1 (the 32.768
kHz crystal on the LaunchPad). If the crystal never starts — broken
solder joint, missing load caps, dead crystal, brown-out during
init — the arch silently re-routes ACLK to VLO (≈ 10 kHz) and
keeps booting. Every software timer then expires roughly **three
times slower** than the code thinks, because `TIKU_CLOCK_SECOND` is
still configured against 32.768 kHz.

That fallback used to be invisible at the application layer.
`tiku_clock_fault()` (added alongside the bit-bang work) makes it
queryable. This example demonstrates the API and gives you a
boot-time pass/fail signal without needing a serial console.

## Hardware

Any FR-series LaunchPad. LED1 = P4.6 on the FR5969 LaunchPad.

## Build & flash

```bash
make MCU=msp430fr5969 HAS_EXAMPLES=1 TIKU_EXAMPLE_CLOCK_FAULT=1
make flash MCU=msp430fr5969
```

## Expected behaviour

| Condition | LED1 |
|---|---|
| XT1 running, `fault == TIKU_CLOCK_ARCH_FAULT_NONE` | 1 Hz blink (calm) |
| XT1 failed, `fault == TIKU_CLOCK_ARCH_FAULT_LFXT_VLO` | 8 Hz blink (panic) |

A normal-looking FR5969 LaunchPad with the on-board crystal
populated should show 1 Hz. To force the fault path on purpose:
de-solder the crystal, disable the LFXT path in `tiku_timer_arch.c`
for testing, or run the example on a board where XT1 is unwired.

## On FR2433

The FR2433 arch uses REFOCLK directly and never tries XT1, so
`tiku_clock_fault()` always returns `TIKU_CLOCK_ARCH_FAULT_NONE`
on that device. The example still compiles and runs at 1 Hz,
so it can be used as a sanity-check baseline.

## Caller pattern

The same call works from any process or shell command:

```c
#include <hal/tiku_clock_hal.h>

unsigned char f = tiku_clock_fault();
switch (f) {
case TIKU_CLOCK_ARCH_FAULT_NONE:
    /* nominal */ break;
case TIKU_CLOCK_ARCH_FAULT_LFXT_VLO:
    /* tick rate is wrong; software timers are slow */ break;
}
```

The fault state is sticky and only re-evaluated on the next
`tiku_clock_arch_init()`.
