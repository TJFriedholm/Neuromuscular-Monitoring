# The facial-to-peripheral correction is a power law, not a multiplier

**Status:** accepted — exponent value pending
**Fills a gap in:** Design Specifications Rev 1 §5.1.1 and §5.1.2, which require a
"validated correction algorithm" and a displayed peripheral-equivalent ratio but
define neither. **Extends:** §5.7.4 (Settings Flowchart), which has no home for it.

The Peripheral-Equivalent TOF Ratio is the Facial TOF Ratio raised to an exponent
n, where n >= 1. The exponent is a user-adjustable setting bounded to [1.0, 4.0].

## Why

Facial muscles recover from neuromuscular blockade faster than the adductor
pollicis, so a Facial TOF Ratio overstates recovery relative to the 0.90 peripheral
threshold the extubation guideline refers to. Some correction is needed for the
device's clinical claim to hold.

Any valid correction f must satisfy **f(1) = 1**. At full recovery fade has
disappeared at every site, so both ratios converge on 1.0. The correction's job is
to compress the middle of the range, where the facial site genuinely reads
optimistically, while leaving the endpoints alone.

A power law does exactly that with one parameter. It also preserves the safety
direction: for x in [0,1] and n >= 1, x^n <= x, so the correction can only ever
lower the reported value. A larger exponent is more conservative.

At n = 2, a facial ratio of about 0.949 yields a peripheral 0.90 — a demanding but
attainable target.

## Considered and rejected

**A multiplicative scalar, peripheral = facial x k.** This was the original
decision here and it is unusable. Because f(1) = k, the displayed value can never
exceed k. Certifying at 0.90 would require a facial ratio of 0.90/k — at k = 0.80,
a facial ratio of 1.125, meaning T4 exceeding T1 by 12.5%, which is not recovery.
Since k > 1 is forbidden (it would raise the reading, the unsafe direction) and
k = 1 is no correction at all, **every permissible scalar makes the extubation
threshold unreachable.** The device would report "below threshold" indefinitely,
including on a fully recovered patient. The flaw is in the shape of the model, not
the choice of constant.

**Offset and rescale, (x - a)/(1 - a).** Also anchored at f(1) = 1 and sound.
Rejected as less faithful: it is linear, implying a uniform bias, whereas the
discrepancy between sites is largest mid-recovery and vanishes at both ends.

**Dropping the transform and raising the threshold instead** — display the measured
facial ratio and compare it against a site-specific threshold near 0.95. Honest,
and it invents nothing. Rejected because it removes the peripheral-equivalent
readout §5.1.2 explicitly requires, which is a change to the device's clinical
claim and not firmware's alone to make.

## Open

The exponent's value and citation are outstanding. The spec's internal reference
list cites "Neuromuscular Monitoring Project Research Summary — Senior Design Team
Documentation, University of Utah, 2026", which is not in this repository. Until it
is supplied the default is left undefined so the device build fails rather than
shipping a placeholder indistinguishable from a sourced value.

A one-parameter power law is a plausible shape, not a validated model. §5.1.1
describes the algorithm as *validated*; it is not, and that claim should be
softened for Rev 2 unless data exists to support it.

## Consequences

- A Facial TOF Ratio above 1.0 returns `AnomalousInput` rather than a corrected
  value. Raising an already-suspect reading toward 1.00 would present it as full
  recovery. tof::compute does not clamp (ADR-0004), so this case does reach here.
- `facial_needed_for(target, n)` exposes the inverse, so the settings screen can
  show what the current exponent demands of the facial reading.
- §5.7.4's Settings Flowchart gains a group for measurement settings.
- The exponent must be persisted across power cycles and shown on screen beside
  the corrected value, so a reader can tell what was applied.
