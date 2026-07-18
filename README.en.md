# verify-dac7311idckr-demo

STM32F103 + RT-Thread based DAC7311IDCKR SPI DAC verification project for pump control / analog output hardware validation.

## Overview

This project drives the TI DAC7311IDCKR 14-bit single-channel DAC via software SPI (GPIO Bit-Bang), supporting voltage setting, raw value write, percentage control, and power-down modes. RT-Thread Shell is used for interactive debugging and verification.

## Hardware

| Item | Description |
|------|-------------|
| MCU | STM32F103 (72MHz, Cortex-M0) |
| DAC | TI DAC7311IDCKR (14-bit, single-channel, SPI) |
| VREF | 5.0V (VDD) |
| Output Range | 0 ~ 5.0V (linear, verified) |

### Pin Connections

| STM32 Pin | DAC7311 Pin | Function |
|-----------|-------------|----------|
| PB7 | SYNC | Chip Select (active low) |
| PB8 | SCLK | SPI Clock |
| PB9 | DIN | SPI Data Input |

### DAC7311 SPI Protocol

- **Mode**: SPI Mode 0 (CPOL=0, CPHA=0), MSB First
- **Frame**: 16-bit `[X X PD1 PD0 D13 D12 ... D0]`
- **Full Scale**: 0x3FFF (16383)
- **Output Formula**: `VOUT = VREF × D / 16384`

## Project Structure

```
verify-dac7311idckr-demo/
├── applications/
│   ├── main.c                    # App entry + RT-Thread Shell commands
│   └── macSYS/
│       ├── Inc/
│       │   ├── bsp_sys.h         # BSP system config
│       │   └── dac7311.h         # DAC7311 driver header
│       └── Src/
│           ├── bsp_sys.c         # BSP system implementation
│           └── dac7311.c         # DAC7311 driver (GPIO Bit-Bang SPI)
├── docs/
├── images/
│   └── dac7311_schemtic.png      # Schematic
├── rt-thread/                    # RT-Thread kernel + components
├── LICENSE                       # Apache 2.0
└── README.md
```

## Driver API

```c
/* Initialize DAC7311 GPIO, output 0V */
void dac7311_init(void);

/* Set output voltage (0.0 ~ 5.0V), auto-clamped */
void dac7311_set_voltage(float voltage);

/* Set raw 14-bit value (0 ~ 16383) */
void dac7311_set_raw(uint16_t value);

/* Set percentage (0 ~ 100%), maps to 0 ~ VREF */
void dac7311_set_percent(uint8_t percent);

/* Set power-down mode (Normal / 1k / 100k / HiZ) */
void dac7311_power_down(uint8_t mode);

/* Get current output voltage */
float dac7311_get_voltage(void);
```

## Shell Debug Commands

Connect at 115200 baud, then use:

```bash
# Set voltage
dac volt 2.5          # Output 2.5V

# Set raw value
dac raw 8191          # Output ~2.5V (16383/2)

# Set percentage
dac pct 50            # Output 50% = 2.5V

# Power-down modes
dac pd 0              # Normal operation
dac pd 1              # 1k pull-down power-down
dac pd 2              # 100k pull-down power-down
dac pd 3              # High impedance

# Show status
dac info
```

### Example Output

```
[DAC7311] Initialized (open-drain + 5V pull-up), output=0.00V
[main] DAC7311 verification demo started
[main] Use shell command: dac <voltage|raw|pct|pd> <value>
[main] DAC output set to 2.500V
msh >dac info
[DAC7311 Status]
  Voltage: 2.500V
  Pinout:  PB7=SYNC, PB8=SCLK, PB9=DIN
  VREF:    5.0V (VDD)
  Mode:    SPI SW Bit-Bang, Mode 0, MSB first
```

## Schematic

![DAC7311 Schematic](images/dac7311_schemtic.png)

## Build Environment

- **IDE**: RT-Thread Studio / Keil MDK
- **RTOS**: RT-Thread (Standard)
- **Chip**: STM32F103 Series

## License

[Apache License 2.0](LICENSE)
