# ESP32-C3 LF Trigger for TPMS (Tire Pressure Monitoring System) - 125 kHz OOK

 Cheap EL-50448 trigger devices can transmit only a non-modulated 125 kHz signal.

This project replaces the default PIC microcontroller with an ESP32-C3 (or equivalent ESP8685) to enable the modulation of a 125 kHz signal with various wake patterns.

The default wake pattern is for Continental and some VDO sensors (for example, S180084730Z).

Different patterns can be easily defined in code:

```c
uint8_t wakePattern[] = {0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39};
```

Examples:

- Tesla: `0xAA, 0xAA, 0x7E, 0x69, 0xB1, 0x90, 0x07, 0x01`
- Cybertruck: `0xAA, 0xAA, 0x7E, 0xB7, 0x8D, 0x37, 0x39, 0x01`
- Ford: `0x5A, 0x5A, 0xAB, 0xAA`
- VDO: `0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39`

## Inspired by

- https://mrblahhhh.github.io/car/tech/2026/07/08/tesla-tpms-125khz-wake-protocol.html
- https://github.com/MrBlahhhh/PyroTC
- https://www.fordescape.org/threads/tpms-sensors-in-ford-and-the-relearn-process.127315

## Default pins

```c
#define LF_COIL_PIN 4
#define POWER_PIN 3
#define LED_PIN 5
#define LOW_BATT_LED_PIN 6
#define BATT_VOLTAGE_PIN 1
```

## Build / flash (PlatformIO)

Flashes over the CH343P USB-UART; plain Serial works.

```bash
pio run -t upload
pio device monitor  # 115200
```