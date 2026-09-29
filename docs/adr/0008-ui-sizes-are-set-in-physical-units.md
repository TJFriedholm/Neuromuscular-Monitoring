# UI sizes are set in millimetres and viewing angle, not pixels

**Status:** accepted — the viewing distances are assumptions to test in usability work
**Fills a gap in:** Design Specifications Rev 1, §5.2.1 Buttons 1.a ("font size is
large enough for user to read from distance") and §5.2.4 ("visible … from typical
working distances"), which set no number. **Applies:** §5.2.3 (10 mm × 10 mm
touch targets, 5 mm spacing).

Every text size and touch target in the UI is derived from a physical requirement
and converted to pixels for this panel. No size is chosen by eye on a desktop
monitor.

## Why

The 5-inch 720 × 1280 panel packs about **294 pixels per inch: 11.6 px/mm, or
0.086 mm per pixel** (from the nominal 5.0 in diagonal; confirm against the panel
datasheet's active area). A desktop monitor is about 110 ppi, so a design drawn in
CSS pixels shrinks to roughly **40 %** of its apparent size on the device.

The UI mockup was drawn that way. Its 11–13 px labels come out at about 1 mm tall on
the device, and its 22 px button text at under 2 mm. Neither can be read at a
clinical working distance. The mockup's 116 px buttons are exactly 10 mm, which
meets §5.2.3, but its 16 px gap between START and STOP is 1.4 mm, against the
required 5 mm.

## The rule

Text is sized so its **cap height subtends at least 16 arcminutes** at the distance
it must be read from. 16′ is the minimum character height commonly given in
human-factors guidance (ANSI/AAMI HE75, which the spec cites, and ANSI/HFES 100).
The exact HE75 clause needs confirming against a copy of the standard, which is not
in this repository.

    cap height (mm) = distance (mm) × 0.004654      (16′ in radians)
    cap height (px) = cap height (mm) × 11.6
    font size  (px) = cap height (px) / 0.70         (IBM Plex cap height ≈ 0.70 em)

| Tier | What | Read from | Min cap height | Min font size |
|---|---|---|---|---|
| 1 | TOF Count, Peripheral-Equivalent TOF Ratio, fault title | 1.0 m, across the table | 4.7 mm / 54 px | **77 px** |
| 2 | Facial TOF Ratio, CMAP amplitude, T1–T4 values, impedance, device state | 0.6 m, at the workstation | 2.8 mm / 32 px | **46 px** |
| 3 | Labels, button text, explanatory text | 0.4 m, while touching the screen | 1.9 mm / 22 px | **32 px** |

The distances are assumptions. Changing one changes its row through the formula;
the formula stays.

Touch targets are **≥ 116 px (10 mm)** in both directions with **≥ 58 px (5 mm)**
between adjacent targets (§5.2.3). A slider counts as its draggable knob plus its
extended hit area, not its visible track.

## Considered and rejected

**Scale the mockup up uniformly.** Scaling by 2.5× fixes legibility but overflows
a 1280 × 720 screen several times over. The screen cannot hold the mockup's amount
of text at a legible size. What is shown has to be cut, not just enlarged.

**Fix sizes by eye on the device when it arrives.** That leaves no traceable
reason for any number, and the board is not in hand yet. The formula can be applied
now and checked against the board later.

## Consequences

- The mockup has to be redrawn, not restyled. Most captions ("responses above
  threshold", "as measured", "mV peak-to-peak" as a separate line) go, or merge into
  the value's unit. The elements §5.1.2 requires still have to fit: raw EMG waveform,
  four twitch bars, TOF Count, Facial TOF Ratio, Peripheral-Equivalent TOF Ratio and
  CMAP amplitude.
- The simulator renders at 1280 × 720 and should be viewed at device scale, or
  measured with the constants above, never judged at desktop size.
- Fonts are compiled into the firmware at fixed sizes, one per tier and weight
  actually used, and each must include **Ω θ Δ × — · µ ≤ ≥** (LVGL's built-in
  Montserrat has none of Ω θ Δ). IBM Plex is licensed under OFL 1.1, which permits
  embedding; record it in the SOUP list.
- Constraints on the redraw that come from LVGL 9.5, not from this rule: box
  outlines cannot be dashed (straight dashed lines can); letter spacing is set
  in whole pixels, not em; icons come from LVGL's built-in symbol font or
  converted images, not SVG.

## Open

- **Alarm visibility.** IEC 60601-1-8 (alarm systems) sets its own distances for
  seeing that an alarm is active and reading its message; as recalled, 4 m for
  seeing the priority indicator and 1 m for the text, to be confirmed against the
  standard. The spec does not list 60601-1-8 among its standards. Rev 2 should
  decide whether it applies. If it does, the fault and alarm indicators need their
  own row.
- **Screen size.** If usability testing moves Tier 1 beyond 1 m, a 5-inch panel may
  not hold the §5.1.2 content legibly. That would be a hardware question for Rev 2,
  not something the firmware can solve.
