# Monitor page wording, and the exponent shown in Settings only

**Status:** accepted — the wording is a usability risk to test with clinicians
**Amends:** ADR-0006, Consequences ("the exponent must be … shown on screen beside
the corrected value"). **Affects:** Design Specifications Rev 1, §5.2 (Monitor
screen content).

The Monitor page was simplified at the developer's request (2026-10-27):

| Element | Before | After |
|---|---|---|
| Ratio card heading | "Peripheral-equivalent ratio" | "Train of Four ratio" |
| Correction exponent beside the Facial ratio | "n 2.0" | not shown |
| Twitches card, T1 amplitude | "CMAP T1 1.84 mV" | not shown |
| Count card heading | "TOF count" | "Twitch count" |

What the page *computes and decides* is unchanged: the large number is still the
Peripheral-Equivalent TOF Ratio, the Facial TOF Ratio is still shown beneath it,
and every tone, chip and withheld-value rule in `monitor_model` is as before.

## Why

The home page is read at a glance from across the bed. The exponent and the T1
amplitude are setup and diagnostic values, not readings a clinician acts on, and
they competed with the ratio and count for attention. The twitch bars carry the
amplitude pattern; exact per-twitch millivolts remain on the Signal page.

## Where the exponent now appears

ADR-0006 wanted the exponent visible so a reader can tell what correction was
applied. It is still visible, one tap away: **Menu → Measurement → Correction
exponent**. It is no longer on the same screen as the value it produced.

## Risks

- **"Train of Four ratio" reads as the measured T4/T1.** The number under it is
  the facial ratio raised to the exponent, not T4/T1. A clinician comparing it
  with a TOF ratio from another monitor, or from the literature, is comparing
  different quantities. The measured value is the smaller "Facial" figure below.
  With the exponent also gone from this page, nothing on it says a correction was
  applied.
- Both risks fall on the same decision the clinician makes: whether the 0.90
  threshold has been reached.

Mitigations to consider for Rev 2 (not done): a qualifier in the heading
("Train of Four ratio (adjusted)"), or a one-line note under the value. Usability
testing should check whether clinicians read the large number as the measured
ratio.
