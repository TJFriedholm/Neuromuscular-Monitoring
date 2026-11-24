# The pulse-width bound rests on the ESP32-P4's peripheral, backed by output-enable

**Status:** accepted
**Amends:** ADR-0001 (Pulse width is bounded by hardware, not software).
**Relates to:** ADR-0010, ADR-0011

ADR-0001 says the 200–300 µs pulse width is "enforced in silicon". On shield
V1.0 the silicon doing it is the ESP32-P4's timer peripheral. The shield has no
circuit of its own to limit pulse width: `HOST_STIM_PULSE` passes straight
through the ISO7720 to the TS5A3159, and the switch stays closed for as long as
the line is high.

The firmware therefore bounds pulse width in three layers:

1. **One-shot peripheral.** `fire_pulse(width_us)` arms an MCPWM (or RMT)
   one-shot on the PULSE GPIO. The CPU never sets or clears the pin itself, so
   a hung task cannot hold it high. This is ADR-0001 unchanged.
2. **Output enable only during a sequence** (ADR-0011). Even if PULSE sticks
   high, current flows only while OUTPUT_ENABLE is also high. That is at most
   one TOF train (about 1.5 s) or one calibration ramp.
3. **Watchdogs reset the chip.** The interrupt watchdog and task watchdog are
   enabled with panic-and-reset. A reset returns both GPIOs to their reset
   state (input, no pull). ESP-IDF has no public hook that runs on panic before
   the reset, so nothing drives the lines low first; what the ISO7720F outputs
   while its inputs float is on the to-verify list.

## Why

The hardware cannot change. Layer 1 handles the failure ADR-0001 was written
for, a CPU hang mid-pulse. What it does not handle is the peripheral itself
being misconfigured. Example: a driver bug that sets the one-shot's period to
continuous. That is a software fault producing DC, which ADR-0001 aimed to rule
out. Layers 2 and 3 limit that case to a bounded time at the programmed current
instead of indefinitely.

## Considered and rejected

**Read back the PULSE line and fault if it stays high.** Nothing returns from
the patient side, and reading the host-side GPIO only shows what the ESP32 is
driving, which is the thing in question. A second GPIO and peripheral would be
checking the first with the same silicon.

## Consequences

- The FMEA row for "excessive electrical stimulation" can no longer cite a
  hardware-fixed pulse duration. It should cite layers 1–3, with this ADR as
  the reason.
- The stimulator driver must have a test, on the board, that a 300 µs request
  produces a 300 µs pulse at TP11 and nothing more (oscilloscope). The pulse
  width also includes the PA441's settling, so the delivered current pulse is
  shorter than the gate; measure both.
- PTC's 50 Hz tetanus (§5.1.2) is 250 one-shots in 5 s with OUTPUT_ENABLE held
  high throughout. Layer 2 bounds a stuck PULSE line to 5 s in that mode, not
  1.5 s. Recorded for the PTC design.
- Rev 2 should add a hardware pulse-width limit (a monostable or RC-limited
  gate on the patient side) and then restore ADR-0001's original claim.

## Bench results, 2026-11-24 (zero current)

Stimulating bench build, pot code 0 (no current commanded), CN1 open; R&S RTB24
on Waveshare header pin 35 (label "46", GPIO46), ground on a Waveshare GND pin.

- **Width: 250.0032 µs** for a 250 µs request, 5 of 5 pulses identical
  (standard deviation 0 at 1.25 GS/s). Height 3.26 V, low before and after.
- **Train:** 4 pulses, first to fourth 1.504 s (cursor resolution at
  500 ms/div is several ms; the firmware's tick gives 500 ms ± 1 ms per gap).
- **No trigger on GPIO46 at 1.5 V** through 5 resets/power-ups (flashing,
  RTS resets). GPIO47 not yet scoped.
- **Found and fixed first:** GPIO46/47 sit in the GPIO39–48 bank, whose I/O
  supply on this board comes from the P4's on-chip LDO VO4 (the BSP only turns
  it on for the SD card). Without it the pins cannot drive high: a pulse
  "fired" in firmware and nothing reached the pin. Firmware that drives these
  lines must acquire VO4 at 3.3 V after setting both pins low
  (`board.hpp`, `kStimIoLdoChannel`). Waveshare schematic not yet checked to
  confirm VDDPST_5 is VO4.
- `PulseGenerator::fire()` was fixed before this test: the RMT payload is a
  member (not stack), and a completion timeout disables the channel, returns
  the pin to plain GPIO driven low, and latches the generator off.

Still to do: the 300 µs case and TP11 (delivered current, needs a load);
GPIO47 (OUTPUT_ENABLE) on the scope through reset.

## To verify on the board

- The reset state of the two PULSE / ENABLE GPIOs on the ESP32-P4 (GPIO46/47 if
  ADR-0010's pinout holds). They must not be strapping pins or default high.
- The ISO7720F's OUTA/OUTB with the ESP32 held in reset (inputs floating). If
  either goes high, the shield needs a pull-down on the host side for Rev 2.
