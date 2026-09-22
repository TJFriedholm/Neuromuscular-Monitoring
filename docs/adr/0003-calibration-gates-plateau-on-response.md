# Calibration gates plateau detection on an evoked response, and reports its outcome

**Status:** accepted
**Deviates from:** Design Specifications Rev 1, §5.7.11 (Algorithm 2, Automatic Calibration Procedure)

The supramaximal ramp does not test for a plateau until the measured response has
exceeded the Detection Threshold θ at least once. Calibration returns an explicit
outcome — plateau found, no response, ramp exhausted, or impedance out of range —
rather than a bare `(B, I_cal)` pair.

## Why

Algorithm 2 as written contains two failures that both end with the device
stimulating a patient on the strength of a calibration that never succeeded.

**Subthreshold is indistinguishable from plateau.** The loop initialises
`R_prev ← 0` and then tests `if |R_curr − R_prev| < Δ then break`. On the first
iteration this reduces to `|R_curr| < Δ` — "is the response small?" At the starting
current of 10 mA the response is usually small precisely because 10 mA is below
threshold for the facial nerve and no CMAP has been evoked. The loop breaks,
sets `I_cal = 13 mA` and reports success without ever having produced a response.

The underlying reasoning error is that "the response stopped growing" only carries
information once the response has started. Gating the plateau test on `R > θ` makes
that precondition explicit: if it is not a Detectable Twitch, it is not yet on the
recruitment curve, so there is no curve to be flat.

**Exhaustion is indistinguishable from success.** If the ramp reaches `I_max`
without plateauing — lead off, electrode misplaced, nerve not under the pad — the
`while` simply ends and `I_cal ← min(I_max, 1.3·I)` yields `I_max`. The caller
receives the same shape of result it would on success and cannot tell "supramaximal
found at 35 mA" from "gave up at 60 mA". The device then delivers maximum current
indefinitely. This is the same sentinel collision as `R_TOF ← 0` in Algorithm 1
(see ADR-0004).

## Considered and rejected

**Skip only the first iteration.** Fixes the literal off-by-one and nothing else. If
10 mA and 15 mA are both subthreshold, iteration two compares two noise samples,
finds them similar, and breaks anyway. Leaves the exhaustion case untouched.

**Average N pulses per level and confirm a plateau across two levels.** More robust
against a single noisy pulse triggering a false break. Rejected for now on startup
cost — three pulses per level at 1 Hz is roughly 35 s of stimulation before
monitoring can begin, against about 12 s for a single-pulse ramp. Revisit if bench
data shows single-pulse plateau detection is unreliable.

## Consequences

- Callers must handle a failed calibration. `CAL_NO_RESPONSE` and `CAL_NO_PLATEAU`
  both mean the device must not proceed to monitoring, and both should surface to
  the clinician as a placement or connection problem rather than as a measurement.
- θ is now an input to calibration as well as to `tof_compute`, so it must be owned
  in one place rather than defined per-module.
- §5.7.11 Algorithm 2 should be redrawn for Rev 2.
