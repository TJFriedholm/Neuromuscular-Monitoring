# The shield gets its own I²C bus

**Status:** accepted — wired on the bench 2026-11-24; to verify with `scan`
**Supersedes:** ADR-0013
**Relates to:** ADR-0007, ADR-0010, ADR-0014

ADR-0013 put the shield on the Waveshare board's I²C bus (J1 pins 4/6 =
GPIO7/8) and found "no address clashes". It missed one: the board's **ES7210**
microphone ADC is on that bus at **0x40** (BSP 1.0.4,
`esp32_p4_wifi6_touch_lcd_5.c`: `ES7210_CODEC_DEFAULT_ADDR` = 0x80, 8-bit).
The shield's ADS122C04 has A0 = A1 = ISO_GND, which is also **0x40**.

The ES7210 answers its address whether or not firmware starts it. With both on
one bus, a scan finds "0x40" even with the shield unplugged, writes reach both
chips, and reads return the wired-AND of two chips' data. Firmware cannot fix
this: the ISO1640 passes addresses through unchanged, and the ADS122C04's
address is set by pins.

## Decision

The shield's `HOST_I2C_SDA`/`HOST_I2C_SCL` are wired to **Waveshare header
pins 16/20 = GPIO28/29**, not to header pins 4/6, which stay unconnected.
Firmware runs that bus on the P4's **other I²C controller (port 0)**; the BSP
keeps port 1 on GPIO7/8. The ADS122C04 (0x40) and AD5241 (0x2C) are the only
devices on it.

This is a wiring change between the boards, not a change to the shield, which
stays frozen (ADR-0010). The ISO1640 and both ground domains are unchanged.

GPIO28/29 are on the header, are not strapping pins (34–38), are not USB, and
are not claimed by the BSP (I²C 7/8, I²S 9–13, LCD 26/27, SD 39–44, PA 53).

## What carries over from ADR-0013

- Every shield transaction has a timeout (`kI2cTimeoutMs`, 5 ms).
- A timeout or NACK gets one bus recovery attempt, then FAULT (ADR-0005), which
  disables output by GPIO (ADR-0011) without needing the bus.
- Shield traffic stays away from pulse edges where possible (ADR-0014).
- The ADS122C04 is reset with its addressed RESET command, never the I²C
  general call.

## What changes

- **A stuck shield bus no longer takes touch with it.** ADR-0013's main risk,
  and its reason to check touch during stimulation, goes away. The ADR-0007
  startup touch check still applies.
- **No second set of pull-ups.** The shield's host side has only R2/R4
  (3.3 kΩ, about 1 mA low-level current, inside the ISO1640's 3.5 mA side-1
  limit). The bench also enables the P4's weak internal pull-ups so an unwired
  bus does not float.
- **Speed is the shield's to choose.** Bring-up runs at 100 kHz. EMG streaming
  needs 400 kHz: one RDATA poll is about 66 SCL periods, 660 µs at 100 kHz,
  longer than the 400 µs poll period (`polling_fits()`). The AD5241's 400 kHz
  rating is the ceiling.
- **Identity is checked, not assumed.** An ACK at 0x40 proves only that
  something answered. `Ads122c04::reset()` sends RESET and requires all four
  registers to read back as their power-on value of 0; the bench calls nothing
  else on the ADC until that passes.
- The device build creates this bus itself (`i2c_new_master_bus` on port 0).
  It still uses `bsp_i2c_get_handle()` for anything on the board bus.

## Considered and rejected

**Shared bus, as ADR-0013 had it.** Not workable: two devices at 0x40.

**Change the ADS122C04's address straps.** A0/A1 can select 16 addresses, but
they are tied to ISO_GND on the board. That is a solder change to frozen
hardware, and there is no rework support.

**Hold the ES7210 in reset.** It has no reset line that firmware controls on
this board.

## Rev 2

Strap the ADS122C04 to an address that is free on the Waveshare bus, or give
the shield its own header pins for I²C in the schematic.

## To verify on the board

- ~~`scan board` with the shield disconnected shows 0x40 (ES7210), 0x18, and
  0x5D or 0x14.~~ **Done 2026-11-24** (bench no-stim build, shield unplugged):
  0x18, 0x40, 0x5D. The clash is real on this unit; GT911 is at 0x5D.
- **Done 2026-11-24** (no-stim bench, shield powered, CN1 empty):
  - the boot wiring check sees the shield's pull-ups on GPIO28/29;
  - `scan shield` shows exactly 0x2C and 0x40, and the board bus is unchanged;
  - `adc_reset` passes (all four registers read back 0x00);
  - `emg_stats` at 400 kHz: 1952 SPS, 0 missed, 0 bus errors.
- An earlier run that day found 0x2C on the *board* bus and nothing on GPIO28/29,
  with the shield wired to header pins 4/6. The boot wiring check and the scans
  caught it.
