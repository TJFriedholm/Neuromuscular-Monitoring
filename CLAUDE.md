# NMT Device Firmware

Firmware for a neuromuscular monitoring (NMT) device that measures neuromuscular
blockade using Train-of-Four (TOF) stimulation and surface EMG (CMAP) recording
via periorbital electrodes. Medical device software developed with IEC 62304 in
mind: favor clarity, testability, and traceability over cleverness.

The developer is newer to embedded systems (background in Python, C#, MATLAB).
Explain embedded-specific concepts briefly when introducing them.

## Source of truth

`Design Specifications (3).pdf` (Rev 1, 04/17/2026) is the controlled document and
wins over this file. Where the firmware deviates from it, an ADR in `docs/adr/`
records the deviation and the reasoning; those ADRs are the input to Rev 2.
`CONTEXT.md` holds the domain glossary.

Do not change clinical behavior without either matching the spec or writing an ADR.

## Hardware

- Board: Waveshare ESP32-P4-WIFI6-Touch-LCD-5 (in hand since 2026-10-23)
  - ESP32-P4 dual-core, silicon **v3.2** (400 MHz); 32 MB flash, 32 MB PSRAM
  - Flash/serial over the **UART** USB-C port (WCH CH343, appears as
    `/dev/cu.usbmodem…` on macOS; no driver needed)
  - Display: 5", 720x1280 portrait-native, MIPI-DSI 2-lane, HX8394 controller,
    30 Hz, RGB565, ~294 ppi (11.6 px/mm). Run landscape via 90° rotation (ADR-0007)
  - Touch: GT911, 5-point. I²C address is 0x5D **or** 0x14 depending on reset pin
    levels, which the BSP doesn't drive — probe both (the BSP does); no touch means
    no STOP, so no stimulation (ADR-0007). This unit came up at 0x5D.
  - ESP32-C6 Wi-Fi co-processor: never started; `esp_hosted` is not linked (ADR-0007)
- Shield: **NME_Shield_Board V1.0** (EasyEDA). **The hardware is frozen for this
  build**: it differs from the spec's parts, and every difference is an ADR
  (0010–0014), never a hardware change. Hardware fixes go under "Rev 2".
- Host connection: Waveshare 40-pin header (J1). I²C on pins 4/6 = GPIO7/8,
  **the same bus as GT911 touch (0x5D/0x14) and the ES8311 codec (0x18)**; use the
  BSP's bus handle, never a second master (ADR-0013). STIM_PULSE and
  STIM_OUTPUT_ENABLE on pins 35/37 = GPIO46/47 (to verify). Nothing comes back
  from the patient side except I²C: no DRDY, no fault line, no current readback.
- EMG: AD8226 + OPA2197 front end (gain ≈ 50.2, 10.6 Hz HP, 408 Hz LP) into a TI
  **ADS122C04** — 24-bit, **I²C** (0x40, to verify), ≤ 2000 SPS, one EMG input.
  DRDY not wired, so it is polled (ADR-0014)
- Stimulator: **AD5241** digital pot over I²C (0x2C, to verify) sets the amplitude
  command; **TS5A3159** switch gated by STIM_PULSE; **PA441** improved-Howland
  current source on +80 V (LT8365) and −15 V (MC34063). **0–30 mA** (spec says 60;
  ADR-0010), 200–300 µs pulses. mA → pot setting comes from a bench table.
  **Two independent control lines: STIM_PULSE and STIM_OUTPUT_ENABLE** (drives the
  pot's SHDN, forcing the command to 0 V).
- **The +80 V rail is always on**; firmware cannot switch it. "Output disabled"
  (PULSE low, ENABLE low, pot at 0) replaces "HV off" (ADR-0011)
- **No hardware pulse-width limit on the shield.** The bound is the ESP32's one-shot
  peripheral, backed by enabling output only during a sequence and by
  watchdogs (ADR-0012)
- Isolation: ISO1640 (I²C) + ISO7720**F** (PULSE, ENABLE; default-low outputs);
  REC5A-0505SW/H2 makes ISO_5V, 2 kVDC functional isolation only
- Impedance: no lead-off detection. Estimated from STIM_OUT (divider into ADC
  AIN3) and the commanded current, once per train, **stimulating electrodes only**
  (ADR-0014, proposed). Window 500 Ω – 50 kΩ; the source saturates above ≈ 2.6 kΩ
  at 30 mA
- WiFi/BLE are disabled during stimulation and recording (§5.6.5)

## Framework and language

- ESP-IDF **v5.5.3+** (the BSP needs 5.4, but rev v3.x silicon needs 5.5.3 with
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n`; ADR-0007), not Arduino/PlatformIO
- **C++17** throughout (§5.7.5). Waveshare's BSP and examples are C; their headers
  are callable from C++.
- Display stack (ADR-0007), from the Espressif component registry:
  - `waveshare/esp32_p4_wifi6_touch_lcd_5` 1.0.4 (BSP: panel, backlight, touch, audio)
  - `waveshare/esp_lcd_hx8394`, `espressif/esp_lcd_touch_gt911`
  - `espressif/esp_lvgl_adapter` 0.6.x, **not** `esp_lvgl_port`; the BSP registers
    the display through it
  - `lvgl/lvgl` **9.5.0**, the version the BSP pins
  - `espressif/esp_codec_dev` (speaker, for audible alarms)
- Pin exact dependency versions (no ^ ranges); commit dependencies.lock (SOUP record).
  Waveshare writes ranges; resolve and pin them.
- Waveshare reference: github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5
  (`examples/esp-idf/08_lvgl_demo_v9` is the LVGL starting point)

## Architecture rules

- Layering: drivers -> hal interfaces -> core logic (tof, calib, correction,
  safety) -> ui. Maps onto the spec's four layers in §5.7.2.
- Core logic and UI must NOT include ESP-IDF headers, so they build and test on
  the host.
- All hardware access goes through interfaces in `components/nmt_hal/` (headers
  still `hal/*.hpp`); `hal_mock/` provides a fake stimulator and synthetic CMAP
  data for host development. The directory is not `hal/` because ESP-IDF names
  components after their directory and has a core component called `hal`; ours
  would replace it. Never name a component after an ESP-IDF one.
- **Pulse width is bounded in hardware, never in software** (ADR-0001). The HAL
  exposes `fire_pulse(width_us)` — a one-shot with a width. Do not add a settable
  gate; that would let a CPU hang drive sustained DC into the patient.
- Cadence (500 ms between stimuli) is a software `esp_timer`. Width is a hardware
  one-shot.
- **Core algorithms are pure** (ADR-0002): arguments in, result out, no globals.
  Device state is one owned struct; the UI reads it by snapshot copy under a brief
  lock, never by direct reference.
- Stimulator output (`IStimulator::enable_output`) is on only in the stimulating
  states (`safety::output_allowed`), never between trains (ADR-0011).
- Any state can transition to FAULT: output disabled, stimulation disabled, alarm raised.
  **Faults latch** and are cleared only by clinician acknowledgement (ADR-0005).

## UI

- Mockup: Claude Design canvas "NMT Monitor UI Mockup"
  (https://claude.ai/artifact/SgNGFp2Lt87PK6oBho7Nu6), 1280x720 landscape. It
  predates ADR-0006 and ADR-0008: its Main and Settings screens still show a "× 0.80"
  multiplier, and its sizes are desktop pixels. Treat it as a layout sketch, not a spec.
- **Sizes come from millimetres and viewing distance, not pixels** (ADR-0008).
  Minimum font sizes: 77 px for TOF Count / peripheral ratio / fault title, 46 px for
  secondary values, 32 px for labels and button text. Touch targets ≥ 116 px
  (10 mm), ≥ 58 px (5 mm) apart (§5.2.3).
- Fonts: IBM Plex Sans/Mono converted with `lv_font_conv` at fixed sizes by
  `components/ui/fonts/gen_fonts.sh`; must include Ω θ Δ × — · µ ≤ ≥ (Montserrat
  lacks Ω θ Δ). Plex Mono has no Greek at all, so the Mono fonts take Ω θ Δ from
  Plex Sans. A new size or weight means a new line in the script and a rerun.
- What a page shows is decided by pure, host-tested presenters (`monitor_model`,
  `menu_model`, `settings`); `ui::App` and the page classes only lay it out with
  LVGL. The UI asks for changes through `ui::Controller`; the device re-checks
  every value before applying it.
- LVGL 9.5 limits: no dashed box outlines (straight dashed lines are fine), letter
  spacing in whole px, icons from the symbol font or converted images.
- The simulator renders at 1280x720 in RGB565 and opens at the panel's physical
  size (it reads the Mac display's size from CoreGraphics); judge it that way.

## Repo layout

```
components/   tof, calib, correction, safety, nmt_hal, hal_mock, ui,
              sim_device (simulated patient for sim/ and the demo firmware),
              shield (drivers for the NME shield: ADS122C04, AD5241, RMT pulse
              one-shot; ShieldStimulator and ShieldEmg implement the HAL)
firmware/     ESP-IDF project for the ESP32-P4 (currently the demo build)
bench/        ESP-IDF bench firmware: serial commands that exercise each shield
              driver on its own. No UI, no clinical logic. Dummy loads only.
sim/          desktop LVGL simulator using SDL2
tests/        host unit tests (Unity 2.6.0, vendored in tests/vendor/unity)
docs/adr/     architecture decision records
```

Each component's CMakeLists.txt supports both builds:
`if(ESP_PLATFORM)` -> idf_component_register, `else()` -> add_library.

## Build and test (host, macOS)

```
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

If configuring fails with `tapi error: malformed file ... unknown architecture
arm64e.x1`, the installed linker (ld-1267) is older than the macOS 27 SDK. Until
Xcode and the Command Line Tools are updated, build against the 26.5 SDK by
adding `-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk`
to the first `cmake` line (same for `sim/`).

Simulator (needs `brew install sdl2 pkgconf`; downloads LVGL 9.5.0, pinned by SHA-256):

```
cmake -S sim -B sim/build
cmake --build sim/build
./sim/build/nmt_sim            # opens at the panel's physical size; --zoom 1 = full pixels
./sim/build/nmt_sim --page menu --snapshot out.bmp   # save a page as an image
```

Keys in the simulator: B blockade level, I impedance, F fault, A acknowledge.

## Build and flash (board)

ESP-IDF v5.5.5 is installed at `~/esp/esp-idf`; it builds fine with CMake 4.x.
Its Python environment was made for Python 3.12, and Homebrew's `python3` is
now 3.14, so the export script needs to be pointed at it:

```
export IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.12_env
. ~/esp/esp-idf/export.sh
idf.py -C firmware build
idf.py -C firmware -p /dev/cu.usbmodem5C931595651 flash monitor   # Ctrl+] quits
```

`firmware/` is currently the **demo build** (`NMT_DEMO_BUILD` in
`firmware/CMakeLists.txt`): the real UI and core logic driven by `sim_device`,
as the simulator is. It is exempt from the correction-exponent check (ADR-0006)
and must stop being so before any driver that stimulates is linked, so it sets
`EXCLUDE_COMPONENTS shield`. The bench keys (B, I, F, A) are typed into the
serial monitor.

`bench/` is where the shield drivers run. It stimulates: **resistive dummy loads
on CN1 only, never a person.** Type `help` at the `bench>` prompt; the
bring-up order is in the header of `bench/main/bench_main.cpp`.

```
idf.py -C bench build
idf.py -C bench -p /dev/cu.usbmodem5C931595651 flash monitor
```

It creates the I²C bus itself (same port, pins and settings as the BSP), because
it does not link the BSP. The device build must use `bsp_i2c_get_handle()`
instead (ADR-0013).

Settings live in `firmware/sdkconfig.defaults` (LVGL's equivalent of
`sim/lv_conf.h`; keep the two in step). `sdkconfig` is generated and ignored:
delete it after editing the defaults. The factory firmware is backed up in
`~/esp/backups/`.

## Signal processing workflow

Algorithms (filtering, artifact blanking, CMAP detection) are developed and
validated in MATLAB first, then ported to C++. MATLAB exports golden test vectors
(CSV); host unit tests must reproduce them within tolerance. The pure-core rule
(ADR-0002) is what makes this possible.

## Clinical logic

- **TOF count** = number of twitches above the detection threshold θ, counted
  position-independently, 0–4 (§5.7.11 Algorithm 1).
- **TOF ratio** = T4/T1, reported when T1 and T4 are both detectable. Carries a
  separate `ratio_valid` flag and is **not** clamped to [0,1] (ADR-0004).
- **θ is in millivolts.** The §5.7.8 variable table says mA; that is wrong.
- TOF timing: 4 pulses at 2 Hz (500 ms apart).
- Supramaximal calibration is mandatory before monitoring: ramp current until the
  response plateaus, then ×1.3 (§5.1.2, §5.7.11 Algorithm 2, corrected by ADR-0003).
- Peripheral-equivalent ratio = facial ratio **raised to** a user-adjustable
  exponent bounded to [1.0, 4.0] (ADR-0006). Must satisfy f(1)=1, or the 0.90
  threshold becomes unreachable. The exponent's default is still outstanding.

## Status

- Done: `tof`, `calib`, `correction`, `safety` (fault latch + TOF state machine),
  `nmt_hal` interfaces, `hal_mock`, `ui` (Monitor, Signal, Menu and settings
  pages, keypad entry, Fault), SDL simulator. 120 host unit tests passing.
  Demo firmware runs on the board (2026-10-26). Shield drivers and bench
  firmware written and built (2026-11-18), not yet run on the shield.
- Open:
  - Correction exponent default value and citation (blocks the device build by
    design — see ADR-0006)
  - θ (detection threshold, mV) and Δ (plateau threshold, mV) need bench values
  - Signal quality indicator: required by §5.3.2/§5.3.3, undefined
  - PTC and DBS modes: required by §5.1.2, no algorithms specified
  - EMG DSP: 60 Hz notch + bandpass, MATLAB first
  - UI: calibration progress page; alarm sound; volume reaches no hardware yet
    (brightness is wired in the demo, not yet confirmed on the panel)
  - On the board: touch orientation under rotation, tearing, GT911 address
    across power cycles (ADR-0007 "To verify")
  - Rev 2 inputs: 30 Hz vs 60 fps (§5.7.10), memory table (§5.7.9), greyscale
    (§5.2.1), whether IEC 60601-1-8 applies (ADR-0008)
  - Calibration still **clamps** a ×1.3 target above `i_max_ma` (30 mA); ADR-0010
    says it must fault instead. Needs a calib status, FaultCause and UI text.
  - Shield bring-up with `bench/`: J1 pins/GPIOs, I²C addresses, which ADC input
    carries ADC_SIGNAL, front-end gain, missed-sample rate, mA → pot table,
    impedance table (ADR-0010–0014 "To verify")
  - Not yet written: impedance driver (`IImpedance`) waits on the bench table;
    wiring the drivers into a device build waits on ADR-0006
- Next: shield bring-up on the bench, remaining UI screens, CI
