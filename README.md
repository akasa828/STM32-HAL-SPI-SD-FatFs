<p align="center"><a href="README_ZH.md">中文</a> · <strong>English</strong></p>

<h1 align="center">STM32 HAL SPI SD + FatFs</h1>

<p align="center">
  A portable SPI-mode SD card driver, a FatFs bridge, and a ready-to-flash STM32F103 example.
</p>

<p align="center">
  <a href="https://github.com/akasa828/STM32_HAL-SPI_SD-FatFs/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/akasa828/STM32_HAL-SPI_SD-FatFs?sort=semver"></a>
  <a href="https://github.com/akasa828/STM32_HAL-SPI_SD-FatFs/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/akasa828/STM32_HAL-SPI_SD-FatFs/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="STM32 HAL" src="https://img.shields.io/badge/STM32-HAL-03234B">
  <img alt="FatFs" src="https://img.shields.io/badge/FatFs-R0.15-7B61FF">
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

<p align="center">
  <a href="#run-the-demo">Run the demo</a> ·
  <a href="#use-the-driver-in-another-project">Reuse the driver</a> ·
  <a href="docs/API.md">API</a> ·
  <a href="docs/PORTING.md">Porting</a> ·
  <a href="https://github.com/akasa828/STM32_HAL-SPI_SD-FatFs/issues">Issues</a>
</p>

<p align="center">
  <img src="docs/assets/sd-stack.svg" width="900" alt="Application, FatFs, SD protocol core and hardware adapter data flow">
</p>

If you need FatFs on an SPI SD card but do not want the protocol code tied to
one SPI peripheral, one CS pin, or even STM32 HAL, this repository is the part
you can lift out. The same core is used by the
[SD Card OVID Player](https://github.com/akasa828/SD_Card_OVID_Player), where it
handles file browsing, capacity scanning, and card-removal recovery.

## Why this repository exists

Many small SD examples stop after one successful mount. This one keeps the
layers separate and includes the less visible parts needed by a real
application:

| | What you get |
|---|---|
| Portable core | `SD_IO` callbacks carry a user context; no HAL type, fixed SPI, GPIO, or UI dependency leaks into the protocol core. |
| Complete storage path | SDSC/SDHC/SDXC SPI initialization, block I/O, CRC16, FatFs `diskio`, and a FAT12/FAT16/FAT32 demo. |
| Recoverable lifecycle | Explicit deinitialization, card-presence checks, transaction guards, and documented remount behavior. |
| Reproducible example | STM32F103C8T6 project, CubeMX file, CMake presets, ST-Link launch configuration, and VS Code `F5` workflow. |
| Tests that run without a board | Scripted SPI responses cover command frames, addressing, retries, CRC, timeouts, and adapter validation. |

## Supported operations

- SDSC v1/v2 and SDHC/SDXC initialization in SPI mode.
- Single and multiple block read/write, erase, status, CID, CSD, and SCR.
- Data CRC16 verification, bounded retries, and low/high SPI speed switching.
- Multiple card instances through independent `SD_Card` and `SD_IO` contexts.
- FatFs R0.15 attachment through `SD_FatFs_Attach()`.
- Optional restore-on-completion raw-sector self-test.

## Run the demo

1. Download or clone the repository and open its **root directory** in VS Code.
2. Install the recommended official STM32 extension and accept the Bundle
   Manager tool installation.
3. Connect the module, insert a FAT-formatted card, and connect ST-Link.
4. Select `Debug` or `Release`, press `F5`, then choose
   `SD/FatFs demo: Build, flash and debug`.

| Micro SD module | STM32F103C8T6 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SCK | PA5 / SPI1 SCK |
| MISO | PA6 / SPI1 MISO |
| MOSI | PA7 / SPI1 MOSI |
| CS | PB0 |
| Optional log TX | PA9 / USART1 TX, 115200-8-N-1 |

The demo waits for a card, mounts the volume, prints the root directory, writes
and reads back `/FS_TEST.TXT`, and then waits for removal before remounting.

> [!WARNING]
> The demo creates or overwrites `/FS_TEST.TXT`. It does not format the card or
> run the optional raw-sector self-test.

> [!CAUTION]
> The optional raw-sector self-test restores the original block only when the
> test reaches its restore step. A reset or power loss after the test write can
> still corrupt data. Use a disposable card or a block known to be unused.

## Use the driver in another project

The smallest platform-neutral integration needs:

```text
Core/Micro_SD/SD_reader.c
Core/Micro_SD/SD_reader.h
```

Add `Core/Port/sd_stm32_hal.c/.h` for STM32 HAL, and add `Core/fatfs/` when
FatFs is required. Bind the adapter and filesystem before mounting:

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

For another MCU, implement the callbacks in `SD_IO` and call
`SD_Card_BindIO()`. The [porting guide](docs/PORTING.md) lists the SPI contract;
the [API reference](docs/API.md) covers lifecycle, return values, and concurrency.

After `FR_DISK_ERR`, `FR_NOT_READY`, or card removal, close open objects,
unmount, call `SD_DeInit_Card()`, restore the peripheral if necessary, then
reattach and mount. Old `FIL` and `DIR` objects must not be reused.

## Verification

Every push and pull request checks:

| Check | Current coverage |
|---|---|
| Host regression tests | Protocol core, command CRC, FatFs guards, self-test restoration, STM32 HAL adapter |
| Static analysis | `cppcheck` warning, performance, and portability checks |
| Firmware builds | STM32F103 Debug and Release with GNU Arm Embedded Toolchain |

These checks make protocol regressions repeatable; electrical timing, module
level shifting, signal integrity, and individual SD-card behavior still require
testing on the target board.

## Project map

- `Core/Micro_SD/` — platform-neutral SD SPI protocol core.
- `Core/Port/` — STM32 HAL adapter and optional compatibility wrapper.
- `Core/fatfs/` — FatFs R0.15 and the attachable `diskio` bridge.
- `Core/Src/main.c` — mount, directory listing, write/read, and reinsertion demo.
- `tests/` — host-side protocol and adapter regression tests.
- `docs/` — [API](docs/API.md) and [porting](docs/PORTING.md) references.

Contributions and hardware reports are welcome; see
[CONTRIBUTING.md](CONTRIBUTING.md). If the driver saved you time, a star helps
other STM32 developers find it too.

Project-owned code uses the [MIT License](LICENSE). FatFs, STM32 HAL, and CMSIS
keep their own terms; see [third-party notices](THIRD_PARTY_LICENSES.md).
