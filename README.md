<p align="center"><a href="README_ZH.md">中文</a> · <strong>English</strong></p>

# STM32 HAL SPI SD + FatFs Driver

A reusable SPI-mode SD card driver with a FatFs `diskio` bridge and a complete
STM32F103C8T6 example. The protocol core uses a context-based `SD_IO` interface:
it does not include STM32 HAL headers, assume a fixed SPI peripheral, own a CS
pin, or depend on an OLED. The repository can be opened directly in VS Code and
flashed with `F5`.

![Version](https://img.shields.io/badge/version-v1.0.0-blue)
![STM32 HAL](https://img.shields.io/badge/STM32-HAL-03234B)
![FatFs](https://img.shields.io/badge/filesystem-FatFs-7B61FF)
![License](https://img.shields.io/badge/license-MIT-green)

## What is included

- SDSC v1/v2 and SDHC/SDXC initialization in SPI mode.
- Single/multiple block read and write, erase, status, CID, CSD, SCR, and online detection.
- Data CRC16 checking, retries, and low/high SPI speed switching.
- `SD_IO` with a user context and callbacks for SPI, CS, timing, speed, deinitialization, and critical sections.
- STM32 HAL adapter accepting any SPI handle, GPIO port, CS pin, and prescalers.
- FatFs R0.15 bridge with `SD_FatFs_Attach()` and FAT12/FAT16/FAT32 support.
- Optional non-destructive raw-block self-test without display dependencies.

## Quick start

1. Download or clone the repository and open its root directory in VS Code.
2. Install the recommended official STM32 extension and allow Bundle Manager to install its tools.
3. Connect the SD module, USART1 TX if wanted, and ST-Link. Select `Debug` or `Release` and press `F5`.
4. Choose `SD/FatFs demo: Build, flash and debug`; continue from `main()`.

| Micro SD module | STM32F103C8T6 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SCK | PA5 / SPI1 SCK |
| MISO | PA6 / SPI1 MISO |
| MOSI | PA7 / SPI1 MOSI |
| CS | PB0 |
| Optional log TX | PA9 / USART1 TX, 115200-8-N-1 |

> [!WARNING]
> The demo creates or overwrites `/FS_TEST.TXT`. It does not format the card or
> run the optional raw-sector self-test.

## Reusing the driver

Bind the hardware adapter and FatFs drive before mounting:

```c
SD_STM32_HAL adapter = {
    .spi = &hspi1,
    .cs_port = GPIOB,
    .cs_pin = GPIO_PIN_0,
    .low_prescaler = SPI_BAUDRATEPRESCALER_256,
    .high_prescaler = SPI_BAUDRATEPRESCALER_16,
};

SD_STM32_HAL_Attach(&g_sd_card, &adapter);
SD_FatFs_Attach(&g_sd_card);
f_mount(&fs, "", 1);
```

The core can instead be used on another MCU by filling `SD_IO` and calling
`SD_Card_BindIO()`. See [API](docs/API.md) and [Porting](docs/PORTING.md).

After `FR_DISK_ERR`, `FR_NOT_READY`, or card removal, close files, unmount the
volume, call `SD_DeInit_Card()`, reinitialize the platform peripheral if needed,
reattach it, and mount again. Do not reuse old `FIL` or `DIR` objects.

## Used in a complete project

This driver was separated from [SD Card OVID Player](https://github.com/akasa828/SD_Card_OVID_Player),
where it provides SD card access, FatFs file browsing, capacity scanning, and
card-removal recovery for an STM32F103 OLED video player.

## Repository layout

- `Core/Micro_SD/` — platform-neutral SD SPI protocol core.
- `Core/Port/` — STM32 HAL adapter and optional compatibility wrappers.
- `Core/fatfs/` — FatFs R0.15 and the attachable `diskio` bridge.
- `Core/Src/main.c` — mount, directory listing, and file write/read demo.
- `Drivers/` — STM32F1 HAL and CMSIS used by the example.
- `.vscode/`, `.settings/`, `cmake/` — portable VS Code, ST-Link, and build setup.

Project-owned code uses the [MIT License](LICENSE). FatFs, STM32 HAL, and CMSIS
retain their own terms; see [third-party notices](THIRD_PARTY_LICENSES.md).
