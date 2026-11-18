# Firmware targets the NME shield V1.0 as built

**Status:** accepted — hardware is fixed for this build; pin and address details to verify on the board
**Deviates from:** Design Specifications Rev 1, §5.1.2 (Electrical Stimulator Circuit:
"adjustable from 0 to 60 mA"), §5.6.1 (ADS1299-4PAG over SPI), and the hardware
description in CLAUDE.md, which followed the spec

The drivers are written for the shield in `NME_Shield_Board` V1.0 (EasyEDA,
Schematic2_1, sheets dated 2026-10-16 to 2026-10-25), not for the parts the spec
names. The hardware will not change for this build, so where the two differ, the
firmware follows the board and this ADR records the difference for Rev 2.

## The board

| Function | Spec / CLAUDE.md | Shield V1.0 |
|---|---|---|
| EMG ADC | ADS1299-4PAG, SPI, 4 ch, lead-off detection | **ADS122C04**, I²C, 24-bit, ≤ 2000 SPS, after an analog front end (AD8226, G ≈ 10; OPA2197, G ≈ 5.02; 0.16 Hz / 10.6 Hz high-pass, 408 Hz low-pass) |
| Amplitude control | PWM → RC filter | **AD5241** 10 kΩ digital potentiometer, I²C, 256 steps across ISO_3V3 → `STIM_LEVEL` |
| Pulse gate | 2N7002 pulse-enable | **TS5A3159** analog switch: `ISO_STIM_PULSE` connects `STIM_LEVEL` to `STIM_CMD` |
| Current source | OPA2277 + IRF840, 0–60 mA | **PA441** improved-Howland source on +80 V (LT8365) and −15 V (MC34063); design target **30 mA** |
| Isolation | ISO7762, 6 ch (SPI, PWM, enable, DRDY) | **ISO1640** (I²C) + **ISO7720F** (2 ch, host → patient side only) |
| Impedance | ADS1299 lead-off | Divider on `STIM_OUT` (K ≈ 0.0217) into ADS122C04 AIN3 (see ADR-0014) |

The host connection is J1, the Waveshare 40-pin header:

| J1 pin | Net | ESP32-P4 | Note |
|---|---|---|---|
| 1, 3 | HOST_5V | — | |
| 2 | HOST_3V3 | — | |
| 4 | HOST_I2C_SDA | GPIO7 | shared with GT911 touch and the ES8311 codec (ADR-0013) |
| 6 | HOST_I2C_SCL | GPIO8 | |
| 35 (to verify) | HOST_STIM_PULSE | GPIO46 (to verify) | |
| 37 (to verify) | HOST_STIM_OUTPUT_ENABLE | GPIO47 (to verify) | |

Nothing returns from the patient side except I²C: no DRDY, no fault line, no
current readback.

## Why

Hardware is frozen for this build. Firmware that waits for the spec's parts has
nothing to run on. The layering (ADR-0002) means only the drivers and the HAL
constants see the difference; `tof`, `calib`, `correction`, `safety` and `ui`
are unchanged.

## The 30 mA limit

The shield's design target is 30 mA, against the spec's 60 mA. The firmware
limit follows the board: `hal::kCurrentMaxMa` becomes **30.0**, so calibration
cannot ask for a current the hardware was not designed to deliver.

Clinical consequence: supramaximal calibration ramps to the plateau and then
applies ×1.3 (ADR-0003). If the plateau current is above about 23 mA, the ×1.3
target exceeds 30 mA. That case must be a calibration failure that routes to
FAULT (ADR-0005), not a silent clamp to 30 mA, because a clamped current is not
supramaximal and every TOF count after it would be unreliable. Facial nerve
stimulation usually plateaus well below this, but that has to be confirmed on
the bench.

The actual maximum also depends on the load. At +80 V, the source can push
30 mA through about 2.6 kΩ minus amplifier headroom. A higher load cannot get
the commanded current (ADR-0014 covers detecting this).

## Considered and rejected

**Keep 60 mA in software and let the hardware saturate.** Rejected. The
calibration would believe it delivered a current it did not, and nothing on the
board reports the shortfall directly.

## Consequences

- Update `components/nmt_hal/include/hal/stimulator.hpp` (`kCurrentMaxMa`,
  header comments), `emg.hpp` and `impedance.hpp` (ADS1299 comments), and the
  Hardware section of CLAUDE.md.
- The calibration algorithm needs a "target exceeds maximum current" outcome
  that routes to FAULT, alongside `CAL_NO_RESPONSE` and `CAL_NO_PLATEAU`.
- Amplitude is set through a mA → AD5241 setting table measured on the bench.
  The schematic notes that the Howland network "requires simulation and
  experimental verification"; no closed-form value is used.
- The board's isolation (REC5A, 2 kVDC) is described on the schematic itself as
  functional, not patient-protection grade, against §5.1.2's 2 × MOPP. That is
  outside firmware's scope; it is recorded here so Rev 2 sees it.
- Rev 2 should restate §5.1.2's current range and §5.6.1's parts list to match
  the board, or state why the board is a prototype that does not meet them.

## To verify on the board

- J1 pins carrying `HOST_STIM_PULSE` and `HOST_STIM_OUTPUT_ENABLE`, and their
  GPIO numbers. The schematic labels sit between pins 33/35 and 35/37, and
  Waveshare lists pin 33 as GND. Check continuity before powering.
- ADS122C04 address (A0/A1 appear tied to ISO_GND → 0x40) and AD5241 address
  (AD0/AD1 appear tied to ISO_GND → 0x2C). Scan the bus.
- Which ADS122C04 input carries `ADC_SIGNAL` (AIN0 or AIN1), and the reference
  (see ADR-0014).
- How the shield mates: J1 is a male header and so is Waveshare's. Whatever
  cable or socket joins them must keep pin 1 on pin 1.
- mA per AD5241 setting, across 500 Ω – 2.5 kΩ loads.
