# Pulse width is bounded by hardware, not software

**Status:** accepted
**Deviates from:** Design Specifications Rev 1, §5.7.3 (Main Program Flowchart), §5.7.10 (Critical Timings)

Stimulation timing is split by tolerance. The 500 ms cadence between the four
stimuli of a Train-of-Four is produced by a software `esp_timer`; the 200–300 µs
pulse width is produced by a hardware one-shot (MCPWM/LEDC) driving the
pulse-enable gate. Software never times a pulse edge.

## Why

§5.7.3 shows a single 5 ms superloop calling `Signal Sequence` inline, which implies
the CPU times the pulse itself. Three problems:

1. **The pulse width would not be bounded.** If the CPU hangs between opening and
   closing the gate, the pulse never ends and the patient receives sustained DC
   current into the facial nerve. The FMEA rates "excessive electrical stimulation →
   nerve or tissue damage" at severity 9 and names "fixed pulse duration" as its
   mitigation. That mitigation is only real if the duration is enforced in silicon;
   in software it is an intention, not a bound.
2. **The resolution is not there.** The default FreeRTOS tick under ESP-IDF is 10 ms
   — 40× longer than one pulse. A task cannot resolve 250 µs.
3. **The stop control would be dead during stimulation.** A Train-of-Four spans
   1.5 s of wall time. Called inline, the loop cannot reach `Check for start/stop`
   until the train completes.

The 500 ms cadence has the opposite character: ±50 µs of software jitter on 500 ms
is 0.01%, far below anything clinically or physiologically meaningful. It does not
need hardware and does not justify the cost of one.

## Considered and rejected

**Whole train in the RMT peripheral.** Clocks out all four pulses from a symbol list
with zero CPU involvement, making spacing hardware-exact as well as width. Rejected
as more mechanism than the problem needs: 500 ms exceeds one RMT symbol's range, so
each gap requires roughly sixteen chained symbols, and the added complexity buys
precision on the one axis that is already tolerant. Worth revisiting if PTC's 50 Hz
tetanus proves harder to schedule than expected.

## Consequences

- `components/hal/` must expose the pulse-enable gate as a *one-shot with a width*,
  not as a settable pin. A HAL that offers `gate_set(bool)` would let callers
  reintroduce software-timed pulses; the interface has to make that unexpressible.
- §5.7.3's flowchart no longer describes the firmware. It should be redrawn for
  Rev 2 with stimulation off the main loop.
- Stimulation continues correctly while the UI is blocked, and stop remains
  responsive during a train.
