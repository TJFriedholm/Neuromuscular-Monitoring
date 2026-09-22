# Algorithms are pure; the UI reads state by snapshot

**Status:** accepted
**Deviates from:** Design Specifications Rev 1, §5.7.8 (Key Variables, Types, Data Structures)

Core algorithms (`tof_compute`, calibration, detection) take their inputs as
arguments and return results. They read no global state. Device state lives in one
owned struct mutated only through functions, and the UI obtains it by copying the
whole struct under a brief lock and rendering from its private copy.

## Why

§5.7.8 specifies "one global instance" that "all UI and algorithm interactions will
target". That is sound under §5.7.3's single 5 ms superloop, where only one piece of
code runs at a time. ADR-0001 moved stimulation off that loop, so the assumption no
longer holds and the same design becomes a data race.

The failure is specific. The EMG path finishes a train and begins writing
`twitches[4]`. After T1 and T2 are written, the UI task wakes and renders the four
magnitude bars — taking T1 and T2 from the new train and T3 and T4 from the previous
one, then displaying a TOF Ratio computed across two different measurements. This is
a torn read. It is timing-dependent, so it reproduces rarely, survives testing, and
appears in use. The FMEA rates "Incorrect TOF ratio calculation → misleading clinical
interpretation" at RPN 32, the highest in the document.

Copying the struct is cheap: §5.7.9 budgets state structures at under 1 KB. The lock
is held for a `memcpy` and released before any rendering, so LVGL never stalls the
acquisition path.

The second reason is testability. An algorithm that reads a global cannot be
table-driven — every case must arrange global state, and cases cannot run in
isolation. The MATLAB golden-vector workflow depends on feeding `tof_compute` arrays
directly.

## Considered and rejected

**Global struct plus a mutex.** The smallest change from the written spec. Rejected
because correctness then rests on every caller remembering to take the lock — one
omission anywhere reintroduces the torn read silently — and because it leaves the
algorithms coupled to global state and untestable on the host.

**Full message passing, no shared struct.** Races become structurally impossible
rather than merely avoided. Rejected as further from §5.7.8 than the problem
requires, and it needs a second queue for settings flowing from UI to stimulator.

## Consequences

- §5.7.8's "all UI and algorithm interactions will target this struct" is no longer
  accurate and should be reworded for Rev 2.
- The state struct needs accessor functions rather than `extern` linkage, so the
  lock cannot be bypassed.
- `components/tof/` and the other core components link and run on the host with no
  ESP-IDF headers, which is what makes the MATLAB-to-C vector comparison possible.
