# The display runs landscape on the Waveshare BSP at 30 Hz, RGB565

**Status:** accepted — not yet verified on hardware
**Deviates from:** Design Specifications Rev 1, §5.7.9 (Memory Usage), §5.7.10
(Critical Timings: "LVGL Update 60 fps"), §5.2.1 Screen 1.c ("16 bit range of
greyscale")

The display is brought up through Waveshare's board support package
(`waveshare/esp32_p4_wifi6_touch_lcd_5`) and Espressif's `esp_lvgl_adapter`, as
Waveshare's own LVGL example does. The UI is laid out in landscape (1280 × 720) and
rotated 90° onto the natively portrait (720 × 1280) panel. The panel refreshes at
30 Hz in RGB565.

## Why

**Use the vendor stack.** The panel is a MIPI-DSI HX8394 with a GT911 touch
controller. The BSP already carries the board's reset, backlight, lane and timing
configuration, and Waveshare's README says display behaviour needs testing on the
board itself before any of it is changed. Writing our own bring-up would mean redoing
that testing for no clinical gain. The BSP registers the display through
`esp_lvgl_adapter`, not `esp_lvgl_port`, so we use the adapter too; running both
would mean two owners of the same LVGL display.

**Landscape.** The conceptual prototype's screen box (§5.12) and the UI mockup are
both landscape. The panel
hardware is portrait, so the image has to be rotated. On MIPI-DSI the adapter only
permits rotation with a tear-avoidance mode, and 90° rotation requires **three
full-frame buffers** (`TRIPLE_PARTIAL`, the BSP's default mode).

**30 Hz, not 60 fps.** The BSP configures the panel with
`HX8394_720_1280_PANEL_30HZ_DPI_CONFIG`. LVGL can render faster than that, but
the glass shows at most 30 new frames per second. The clinical requirement is the
≤ 1.0 s latency from the last stimulus to the displayed result (§5.1.2, §5.7.10);
30 Hz adds at most about two frames (~67 ms) to that. A 30 Hz EMG trace
scrolls visibly smoothly. 60 fps is not achievable on this panel configuration and
buys nothing the device claims.

**RGB565.** The BSP's DPI configuration hard-codes RGB565 even when its Kconfig
selects RGB888, so RGB888 is not a supported path without changing the BSP. RGB565
stores 5 bits of red and blue and 6 of green: a true grey has 32 levels, not 65 536.
§5.2.1 1.c's "16 bit range of greyscale" does not describe this panel.

## Memory

Replaces the display rows of the §5.7.9 table:

| Item | Size | Where |
|---|---|---|
| DPI frame buffers | 3 × 720 × 1280 × 2 B ≈ **5.3 MB** | PSRAM |
| LVGL draw buffer | 720 × 50 rows × 2 B ≈ 70 KB (BSP default) | PSRAM |
| LVGL heap | as §5.7.9, 256 KB | PSRAM |

The §5.7.9 figure of 180 KB for "720 × 62 rows at 2 bytes per pixel" is
arithmetically about two such buffers (87 KB each); the table does not account for
full-frame buffers at all. 5.3 MB of 32 MB PSRAM leaves ample room.

## Considered and rejected

**Portrait layout, no rotation.** Needs one frame buffer instead of three and
removes the rotation cost. Rejected because it contradicts the physical design and
the mockup, and memory is not constrained. Revisit if rendering performance on the
board is poor.

**Rotation with PPA (the P4's 2D accelerator).** The BSP sets
`enable_ppa_accel = false`, and the adapter ships a patch for a PPA hang. Not worth
the risk before software rotation is measured on the board.

**Writing a display driver without the BSP.** See Why.

## Consequences

- `firmware/` depends on the BSP, `esp_lcd_hx8394`, `esp_lcd_touch_gt911`,
  `esp_lvgl_adapter` and LVGL **9.5.0**, the version the BSP pins. Waveshare writes
  the others as ranges (`^1.0.4`, `~0.6`); we pin exact versions and commit
  `dependencies.lock` (SOUP record). The BSP requires **ESP-IDF ≥ 5.4**.
- The host simulator (`sim/`) must run at 1280 × 720 and RGB565 so it shows what the
  panel will show. Near-identical dark greys can merge after 16-bit rounding;
  surface colours are chosen to differ by at least one RGB565 step.
- **Touch must be confirmed at startup.** The GT911 picks its I²C address (0x5D
  or 0x14) from pin levels at reset, and the BSP does not drive those pins
  (Waveshare issue #9). STOP is a touch control, so if neither address answers the
  device must not stimulate. How that is surfaced is for the fault design.
- **The Wi-Fi co-processor is never started.** The factory ESP32-C6 firmware is too
  old for current `esp_hosted` hosts and conflicts with the SD card on the shared
  SDMMC controller (Waveshare issue #13). The device has no use for radios (§5.6.5,
  §5.7.6), so `esp_hosted` is not linked.
- Rev 2 should change §5.7.10 to "display refresh 30 Hz; result latency ≤ 1.0 s",
  replace the §5.7.9 display rows with the table above, and restate §5.2.1 1.c in
  terms the panel can meet.

## To verify on the board

- ESP32-P4 silicon revision (Waveshare defaults to rev 3.x; a rev 1.3 profile exists).
- Rendering time of a full-screen redraw with 90° software rotation.
- That the 30 Hz timing is stable and free of tearing in `TRIPLE_PARTIAL`.
- Which GT911 address the unit comes up on, across several power cycles.
