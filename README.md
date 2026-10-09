# ESP32-C3 LF Trigger for TPMS (Tire Pressure Monitoring System) - 125 kHz OOK
  Use to modify cheap EL-50448 which send only nonmodulated 125 kHz signal
  Default wake pattern is for Continental or some VDO sensors (e.g. S180084730Z) 
  
  Diferent paterns can be easely defined in code.
  uint8_t wakePattern[] = {0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39}; 
    
    tesla
    0xAA, 0xAA, 0x7E, 0x69, 0xB1, 0x90, 0x07, 0x01

    cybertruck
    0xAA, 0xAA, 0x7E, 0xB7, 0x8D, 0x37, 0x39, 0x01

    ford
    0x5A, 0x5A, 0xAB, 0xAA

    VDO
    0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39


## Inspired by:

https://mrblahhhh.github.io/car/tech/2026/07/08/tesla-tpms-125khz-wake-protocol.html
https://github.com/MrBlahhhh/PyroTC
https://www.fordescape.org/threads/tpms-sensors-in-ford-and-the-relearn-process.127315



## Default Pins
#define LF_COIL_PIN 4
#define POWER_PIN 3
#define LED_PIN 5
#define LOW_BATT_LED_PIN 6
#define BATT_VOLTAGE_PIN 1


## Build / flash (PlatformIO)
Flashes over the CH343P USB-UART; plain Serial works.
```
pio run -t upload
pio device monitor      # 115200