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

## To verify on the board

- The reset state of the two PULSE / ENABLE GPIOs on the ESP32-P4 (GPIO46/47 if
  ADR-0010's pinout holds). They must not be strapping pins or default high.
- The ISO7720F's OUTA/OUTB with the ESP32 held in reset (inputs floating). If
  either goes high, the shield needs a pull-down on the host side for Rev 2.
