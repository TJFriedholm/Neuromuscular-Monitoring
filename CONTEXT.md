# Neuromuscular Monitoring

Firmware for a facial-nerve neuromuscular monitor. It stimulates the facial nerve
through periorbital electrodes, records the evoked muscle response, and reports how
deeply a patient is paralysed so clinicians know when reversal is adequate.

The controlled source of truth for this vocabulary is *Design Specifications*,
Rev 1, 04/17/2026. Where this file deviates from that document, an ADR in
`docs/adr/` records the deviation.

## Language

### Stimulation

**Stimulus**:
A single constant-current square pulse delivered to the facial nerve, defined by an
amplitude in mA and a pulse width in µs.
_Avoid_: shock, zap, fire

**Train-of-Four (TOF)**:
A sequence of exactly four stimuli delivered 500 ms apart (2 Hz). The unit of
measurement for routine monitoring.
_Avoid_: train, burst, sequence

**Post-Tetanic Count (PTC)**:
A deep-block measurement: a 5-second 50 Hz tetanus, then single stimuli at 1 Hz, up
to 20. Used when a Train-of-Four evokes no response at all.

**Supramaximal Calibration**:
The startup procedure that raises stimulus amplitude until the evoked response stops
growing, then sets the operating amplitude 30% above that point. Guarantees every
nerve fibre is recruited, so later amplitude changes reflect blockade rather than
inconsistent stimulation.
_Avoid_: calibration (ambiguous — the device also zeroes its baseline)

**Supramaximal Threshold**:
The amplitude at which the evoked response plateaus. The input to Supramaximal
Calibration, not the operating amplitude itself.

### Response

**CMAP** (Compound Muscle Action Potential):
The muscle's electrical response to one stimulus, recorded differentially at the
periorbital electrodes. Measured as a peak-to-peak amplitude in mV.
_Avoid_: EMG signal (that is the continuous recording; a CMAP is one evoked event)

**Twitch**:
The CMAP evoked by one stimulus within a Train-of-Four, identified by its position:
T1, T2, T3, T4.
_Avoid_: response, contraction, beat

**Detectable Twitch**:
A twitch whose peak-to-peak amplitude exceeds the detection threshold. Detection is
an absolute test in mV against a fixed threshold, not a comparison against T1.

**Detection Threshold**:
The fixed peak-to-peak amplitude in mV above which a twitch counts as detectable.
Denoted θ. A response-side quantity — it is never expressed in mA.

**Baseline Offset**:
The mean of the EMG signal recorded with no stimulation, subtracted from samples
before amplitudes are measured. Removes DC offset; unrelated to blockade.

### Measurement outputs

**TOF Count**:
How many of the four twitches are detectable, 0 to 4. Falls as blockade deepens,
because twitches disappear in reverse order — T4 first, then T3, T2, T1.

**TOF Ratio**:
T4 amplitude divided by T1 amplitude, expressed 0 to 1. Quantifies fade. Only
meaningful when both T1 and T4 are detectable.
_Avoid_: T4/T1, fade ratio

**Facial TOF Ratio**:
A TOF Ratio measured at the facial nerve. Recovers earlier than the peripheral
equivalent, so it is optimistic — it is not directly comparable to the clinical
0.90 extubation threshold.

**Peripheral-Equivalent TOF Ratio**:
A Facial TOF Ratio adjusted to estimate what the adductor pollicis would have read.
The value a clinician compares against 0.90.
_Avoid_: corrected ratio, true ratio

**Ratio Validity**:
Whether a TOF Ratio could be computed at all. Tracked separately from the ratio
value, because 0.0 is a legitimate ratio (profound fade) and cannot double as
"unavailable".

### Safety and signal quality

**Electrode Impedance**:
Skin-to-electrode impedance in ohms, measured continuously. Clinically acceptable
range is 500 Ω to 50 kΩ; outside it, recordings are untrustworthy.

**Fault**:
A condition in which stimulation must stop, the high-voltage stage must be
de-energised, and the clinician must be alerted. Reachable from any device state.
_Avoid_: error, alarm (an alarm informs the clinician; a fault stops the device)

**Alarm**:
A clinician-facing notification that a monitored value left its acceptable range —
TOF Ratio below the set limit, or Electrode Impedance out of range. Does not by
itself stop stimulation.
