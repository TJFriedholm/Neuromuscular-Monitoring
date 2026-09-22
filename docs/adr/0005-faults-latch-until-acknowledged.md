# Faults latch until a clinician acknowledges them

**Status:** accepted
**Extends:** Design Specifications Rev 1, §5.7.3 (Main Program Flowchart), which has no fault path

Any state can transition to FAULT. Entering FAULT disables stimulation and
de-energises the high-voltage stage immediately and raises an alarm. FAULT is
latched: it persists after the underlying condition clears, and only an explicit
clinician acknowledgement returns the device to IDLE. The cause is retained for
display.

## Why

The alternative is auto-clearing, which is less disruptive in an operating room and
needs no interaction during a case. It was rejected on one scenario. An electrode
sitting near the impedance limit drifts across 50 kΩ repeatedly. Under auto-clear
the device faults, resumes, faults, resumes — stimulation chatters, the trend
acquires gaps, and no one is told. The clinician sees a plot that looks continuous
and is not.

Latching makes the resumption of current into a patient always a human decision.
That is the property worth paying for: no combination of drifting sensor readings
can cause the device to start stimulating on its own.

ADR-0003 introduced `CAL_NO_RESPONSE` and `CAL_NO_PLATEAU`, both of which mean
monitoring must not proceed. They route here.

## Considered and rejected

**Split by severity** — hardware faults latch, signal-quality conditions suspend and
auto-resume. This maps most closely onto the alarm/fault distinction in CONTEXT.md
and remains the likely eventual design. Deferred because it doubles the failure
paths through the state machine while the machine is still being written, and a
condition routed to the wrong tier is a silent safety defect. Revisit once the
single-tier machine has full transition coverage.

## Consequences

- The state machine needs an acknowledge input from the UI, so the UI becomes part
  of the safety path and cannot be omitted from the simulator build.
- Latching means a fault during an unattended period stops monitoring until someone
  attends to it. That is intended.
- §5.7.3's flowchart needs a fault path added for Rev 2.
