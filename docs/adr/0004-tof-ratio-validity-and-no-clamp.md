# TOF Ratio carries a validity flag and is not clamped

**Status:** accepted
**Deviates from:** Design Specifications Rev 1, §5.7.11 (Algorithm 1), §5.7.8 (variable table)

`tof_compute` returns the TOF Ratio alongside a separate `ratio_valid` flag, does
not clamp the ratio to `[0, 1]`, and takes its Detection Threshold θ in millivolts.

## Why

Three corrections to Algorithm 1, all on the path the FMEA rates highest —
"Incorrect TOF ratio calculation → misleading clinical interpretation", RPN 32.

**`R_TOF ← 0` collides with a real measurement.** Algorithm 1 assigns zero when T1
or T4 is undetectable. But zero is itself a legitimate TOF Ratio: a patient in
profound block with a barely-present T4 genuinely reads near zero. A clinician
shown `0.00` cannot distinguish "deeply paralysed" from "we could not compute this",
and those call for opposite actions. Validity is a separate fact from value and
needs a separate field.

**Clamping to 1.0 manufactures false reassurance.** `R_TOF ← min(1, max(0, R_TOF))`
silently converts any ratio above unity into exactly 1.00. Ratios above 1.0 occur
for real — post-tetanic facilitation, and noise near full recovery. Since 0.90 is
the extubation threshold, the clamp takes a reading that should read as anomalous
and renders it as the most confident possible "fully recovered". The raw value is
reported; the UI may format it, but the core does not discard it.

**θ is a millivolt quantity.** Algorithm 1 declares θ as "detection threshold in mV";
the §5.7.8 variable table declares `threshold` as mA. mV is correct — milliamps
describe the stimulus, millivolts the response. The table is wrong.

## Consequences

- Every caller must check `ratio_valid` before displaying or alarming on a ratio.
  The struct is ordered so the flag is not easy to ignore.
- The UI decides how to render an out-of-range ratio. Showing `1.12` with an
  anomaly indicator is acceptable; showing `1.00` is not.
- §5.7.8's `threshold` row needs its units corrected for Rev 2.
