# The +80 V rail is always on; "HV off" means output disabled

**Status:** accepted
**Amends:** ADR-0005 (Faults latch until acknowledged: "de-energises the
high-voltage stage"). **Relates to:** ADR-0010

On shield V1.0 the LT8365's EN/UVLO pin is tied to ISO_5V, so the +80 V rail is
up whenever the board has power. Firmware cannot turn it off. Where the design
says "HV off", the firmware instead puts the stimulator in its **output-disabled
state**:

1. `HOST_STIM_OUTPUT_ENABLE` driven low. This pulls the AD5241's SHDN low, which
   opens its top terminal and shorts the wiper to ground, so `STIM_LEVEL` is 0 V
   whatever setting is stored. One GPIO write; it cannot fail on a stuck bus,
   so it goes first.
2. `HOST_STIM_PULSE` low (TS5A3159 connects `STIM_CMD` to ground). The firmware
   never holds this line: only the RMT one-shot drives it, and each pulse ends
   by itself within 300 µs (ADR-0012).
3. The AD5241 setting written to 0 and read back, when the bus is available.

Any one of these makes the commanded current zero.

## Why

The hardware cannot change for this build. What ADR-0005 needs is that FAULT
stops current reaching the patient. The three steps meet that with two
independent controls that work without the I²C bus. They do not meet the
stronger property of no stored high-voltage energy on the patient side. The
PA441 sits on the +80 V rail and its output is still connected to the
electrodes, held at about 0 mA by the current-source loop. A fault inside the
PA441 or its feedback network could still drive the electrodes, and firmware
cannot prevent that.

## Considered and rejected

**Keep `enable_hv()` and have it do nothing.** Rejected. A method named
`enable_hv(false)` that returns success while the rail stays up is a false
statement in the safety path, and a reviewer reading the state machine would
believe HV is off.

## Consequences

- `IStimulator::enable_hv(bool)` / `hv_enabled()` are renamed to
  `enable_output(bool)` / `output_enabled()`, and documented as above.
  `hal_mock`, its tests and `safety` change with them. The same physical step
  is now named for what it does.
- `enable_output(true)` happens only for the duration of a stimulation
  sequence (TOF train or calibration ramp), not for the whole monitoring
  session. Between trains the output is disabled, so a stuck PULSE line between
  trains delivers nothing.
- The FMEA row for "excessive electrical stimulation" should list
  output-disable as the control, not HV removal.
- Rev 2: either add a firmware-controlled enable for the HV converter or state
  that the rail is always energised.

## To verify on the board

- With OUTPUT_ENABLE low and PULSE held high, the current through a 1 kΩ load
  is effectively zero. Measure across R29 (TP11 to TP10).
- The ISO7720F outputs low when the host side is unpowered or the ESP32 is in
  reset (F suffix = default-low output). Check what it does when its input pin
  floats while the ESP32's GPIOs are in their reset state.
