# EMG and impedance share one polled ADC

**Status:** proposed — impedance method needs bench validation
**Deviates from:** Design Specifications Rev 1, §5.1.2 (Impedance Detection: "must
continuously measure skin-electrode impedance"), §5.6.1 (ADS1299 lead-off detection)
**Relates to:** ADR-0010, ADR-0015

Shield V1.0 has one ADC, an ADS122C04 on I²C. Its inputs (schematic sheet 3):
AIN0 = `ADC_SIGNAL` (front-end output), AIN1 = `ISO_VCM` (the 2.5 V midpoint
that output is centred on), AIN2 = `IMP_ADC_IN` (stimulation-voltage divider),
AIN3 unconnected. Its DRDY pin is not connected.

## EMG

- **2000 SPS, turbo mode, continuous conversion.** §5.1.2 requires ≥ 1000 Hz
  and 24 bits; the ADS122C04 meets both. The front end low-passes at 408 Hz, so
  2000 SPS leaves margin above Nyquist.
- **Polled, not interrupt-driven.** With no DRDY line, a task reads the result
  on a hardware timer at the conversion rate and checks the ADC's data-ready bit.
  Each sample carries the time it was read. A sample that is not ready, or is
  read twice, is counted. Too many in a CMAP window marks that window invalid
  and does not feed a TOF result.
- **Differential, AIN0 − AIN1, against the internal 2.048 V reference.**
  (Revised 2026-11-24; this said single-ended against AVDD.) ISO_VCM is wired
  to AIN1, so measuring against it removes the 2.5 V midpoint in the ADC
  instead of in software, and the result does not move with ISO_5V. Range is
  ±2.048 V at the ADC, about ±40 mV at the electrodes, which is well above a
  CMAP. A stimulus artifact can exceed it. Full-scale codes are counted as
  clipped, and the CMAP detection blanks the artifact anyway. PGA bypassed
  (gain 1), so an artifact near the rails cannot overdrive the PGA.
  AVDD is still measured at start as a health check of the analog side.
- **Scaling:** mV at the electrodes = ADC volts ÷ 50.2, from the schematic's
  nominal gain. Measure it on the bench and replace the nominal value.

## Impedance

What the board can measure is the voltage on `STIM_OUT` during a stimulus. With
the commanded current, that gives a load estimate Z ≈ V / I. This deviates from
the spec in three ways:

1. **It measures the stimulating pair, not the recording electrodes.** The
   recording electrodes (EMG_AFE_P/N) have no impedance path at all. The team
   accepted this for the V1.0 build on 2026-11-18; closing the gap is tracked
   in issue #1.
2. **It is not continuous.** It exists only during a pulse, so at most four
   times per TOF train.
3. **It uses commanded current, not measured current.** The voltage across R29
   is not routed to the ADC.

The pulse (200–300 µs) is shorter than one conversion at 2000 SPS (500 µs), and
the divider needs about 100 µs to settle. So the ADC cannot sample a settled
pulse voltage directly. The proposed method:

- For the **first pulse of each train**, switch the mux to AIN2, start a
  single-shot conversion, fire the pulse inside the conversion window, then
  switch back to EMG. The conversion averages the pulse with the time around
  it. Convert the averaged result to impedance with a **bench-measured table**
  against known resistors (500 Ω, 1 kΩ, 2 kΩ, 5 kΩ, 50 kΩ) at each width and
  current.
- Switching the mux costs EMG samples during the stimulus artifact, which the
  CMAP detection blanks anyway. Confirm the CMAP onset is after the mux is back.
- **Compliance check, same reading.** If `STIM_OUT` is near the +80 V rail, the
  source could not deliver the commanded current. That is a fault, not an
  impedance warning, since the patient received less current than the
  calibration assumed.

The 50 kΩ upper bound is unreachable as a working load: at 30 mA the source
saturates above about 2.6 kΩ. Above that, the reading means "too high to
stimulate", whatever the true value.

## Considered and rejected

**Sample the pulse voltage directly with a faster conversion.** The ADS122C04's
fastest rate is 2000 SPS. There is no faster mode.

**Measure impedance between trains with a small test pulse.** Any pulse is a
stimulus to the facial nerve, and a test pulse between trains would add twitches
the TOF train does not account for. Rejected unless it can be kept below the
motor threshold, which would then be too small to measure on this divider.

## Consequences

- `IImpedance::read_ohm()` returns the most recent per-train estimate, with its
  age. The UI shows when it was measured, not a continuously live value.
- The UI and IFU must say which electrodes the impedance refers to.
- The recording electrodes have no impedance check. A detached recording
  electrode shows up only as a flat or noisy EMG. That is a signal-quality
  condition (still undefined, see CLAUDE.md), not an impedance one.
- Rev 2: restate §5.1.2 Impedance Detection to what this board measures, or
  require a lead-off path on the recording electrodes (issue #1).

## To verify on the board

- ~~The input mapping~~ **Done 2026-11-24** (`adc_inputs`, CN1 empty): AVDD
  5.035 V; AIN0 2.513 V, AIN1 2.513 V, AIN0 − AIN1 −0.11 mV; AIN2 1.649 V, i.e.
  STIM_OUT ≈ 77 V open-circuit (the meter read about 76 V); AIN3 floats (0.25 V);
  ADC offset 4 µV. Still to do: a meter at TP12 against `stim_out_dc`.
- Sample rate: **measured 1952 SPS**, not 2000 (the ADC's own oscillator, about
  2.4 % slow; 0 missed). Timing must use the measured rate, not the nominal one.
- EMG noise floor, **measured 2026-11-24** (user-reported): CN1 pins 4, 5 and 6
  shorted, **23 µV rms at the electrodes** (nominal gain), about 1.2 mV rms at
  the ADC. With CN1 open it read 0.46 mV rms (pickup on floating inputs).
  Spectrum of a 4.2 s capture (Mac on battery, 25 µV rms): **mains is only 3 %
  of the power** (4.2 µV rms at 60 Hz and harmonics), so a notch will not fix
  it. The rest sits inside the front end's 10–408 Hz passband (12.5 µV in
  10–60 Hz, 18.9 µV in 60–200 Hz, 9.6 µV in 200–408 Hz), with tones at
  non-mains frequencies (15.2, 115, 130, 145 Hz). That points at interference
  entering before the front end (switching converters, common mode through
  EMG_REF's 1 MΩ), possibly aliased; not yet confirmed. **Accepted for now**
  (2026-11-24) so bring-up can continue; the two tests that would separate the
  causes (short to ISO_GND instead of EMG_REF; sample at 1000 SPS and see if
  the tones move) are deferred. Then the gain, with a known signal.
- Missed-sample rate at 400 kHz on the shield bus (ADR-0015).
- The impedance table across loads and widths, and its repeatability.
- That CMAP onset falls after the mux returns to EMG.
