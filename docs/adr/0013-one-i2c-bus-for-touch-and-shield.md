# Touch, audio and the shield share one I²C bus

**Status:** accepted
**Relates to:** ADR-0007 (touch must be confirmed or the device does not stimulate),
ADR-0010

Shield V1.0 takes its I²C from J1 pins 4/6, which Waveshare wires to GPIO7/8.
That is the same bus as the GT911 touch controller (0x5D or 0x14) and the
ES8311 audio codec (0x18). The shield adds the ADS122C04 (0x40, to verify) and
the AD5241 (0x2C, to verify) behind an ISO1640 isolator. There are no address
clashes.

The firmware uses **one bus handle**, the one the BSP creates, and adds the
shield devices to it with `i2c_master_bus_add_device`. It never opens a second
master on GPIO7/8.

## Why

Two master drivers on the same pins would fight over them. ESP-IDF's
`i2c_master` driver already serialises transactions on one bus handle across
tasks, so sharing the BSP's handle is the supported way to do it.

The risk is shared failure. If something on the patient side holds SDA low,
for example the ISO1640 latching during a stimulus transient, every device on
the bus stops answering, including touch. Without touch there is no STOP
(ADR-0007).

## Decisions

- **Every shield transaction has a timeout** (a few ms; the longest legitimate
  transfer is a 3-byte ADC read).
- **A timeout or NACK from a shield device is a fault.** The driver first tries
  bus recovery once (clock out SCL pulses, issue a STOP). If the bus still does
  not answer, the device goes to FAULT (ADR-0005), which disables output by
  GPIO (ADR-0011) and does not need the bus.
- **Touch is checked during stimulation.** If touch reads fail while
  stimulating, that is a fault, extending ADR-0007's startup check.
- **Shield traffic happens away from the pulse edges where possible.** A
  stimulus is the most likely moment to upset the isolator. The ADC schedule
  (ADR-0014) leaves the bus idle for the duration of each pulse.

## Considered and rejected

**Bit-bang a second I²C bus on other header GPIOs.** Not possible: the shield's
I²C is wired to pins 4/6, and the hardware is fixed.

## Consequences

- Touch read rate and ADC polling compete for the bus. At 400 kHz an ADC sample
  read takes about 150 µs, so 2000 SPS uses roughly 30 % of the bus. Touch
  polling must fit in what remains; measure it.
- Bus speed is whatever the BSP sets (check, 400 kHz expected). The ISO1640
  supports up to 1.7 MHz, so it is not the limit.
- The host side now has two sets of pull-ups on SDA/SCL, Waveshare's and the
  shield's R2/R4 (3.3 kΩ). Check that the combined low-level current stays
  inside the ISO1640's side-1 limit (3.5 mA).

## To verify on the board

- An I²C scan with the shield fitted shows 0x5D (or 0x14), 0x18, 0x40, 0x2C.
- Touch keeps working during a TOF train at maximum current.
- Bus recovery works when SDA is held low on the patient side.
