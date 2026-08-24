<p align="center"><strong>中文</strong> · <a href="README.md">English</a></p>

# STM32 HAL SPI SD + FatFs 驱动

这是一套可单独移植的 SPI 模式 SD 卡驱动，包含 FatFs `diskio` 接入和可直接运行的 STM32F103C8T6 示例。协议核心通过带上下文的 `SD_IO` 调用硬件，不包含 STM32 HAL 类型，不写死 SPI 句柄或 CS 引脚，也不依赖 OLED。项目根目录可以直接由 VS Code 打开并按 `F5` 刷写。

## 主要内容

- 支持 SDSC v1/v2、SDHC/SDXC 的 SPI 模式初始化。
- 支持单/多块读写、擦除、状态、CID/CSD/SCR 和 CMD58 在线检测。
- 支持数据 CRC16、失败重试和低速握手/高速读写切换。
- `SD_IO` 为 SPI、CS、计时、速度、反初始化和临界区回调统一提供 `context`。
- STM32 HAL 适配层可传入任意 SPI 句柄、GPIO 端口、CS 引脚和分频值。
- FatFs R0.15 通过 `SD_FatFs_Attach()` 绑定卡实例，支持 FAT12/FAT16/FAT32。

## 快速开始

1. 下载仓库，用 VS Code 打开根目录。
2. 安装推荐的 ST 官方 STM32 扩展，并允许 Bundle Manager 安装工具。
3. 按下表连接 SD、可选串口和 ST-Link，选择 Debug 或 Release 后按 `F5`。
4. 选择 `SD/FatFs demo: Build, flash and debug`，停在 `main()` 后继续运行。

| Micro SD 模块 | STM32F103C8T6 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SCK | PA5 / SPI1 SCK |
| MISO | PA6 / SPI1 MISO |
| MOSI | PA7 / SPI1 MOSI |
| CS | PB0 |
| 可选日志 TX | PA9 / USART1 TX，115200-8-N-1 |

> [!WARNING]
> 示例会创建或覆盖 `/FS_TEST.TXT`，但不会格式化 SD 卡，也不会默认执行原始扇区自检。

> [!CAUTION]
> 可选的原始扇区自检会在流程结束时恢复原数据，但如果测试写入后发生断电或复位，
> 仍可能破坏数据。只能对可丢弃的卡或确认未使用的扇区执行。

## 在自己的工程中使用

先用 `SD_STM32_HAL_Attach()` 绑定硬件，再用 `SD_FatFs_Attach()` 将卡实例交给 FatFs，然后才能挂载卷。换用其他 MCU 时可自行填充 `SD_IO` 并调用 `SD_Card_BindIO()`。

详细接口见 [API 文档](docs/API_ZH.md)，移植步骤见 [移植文档](docs/PORTING_ZH.md)。

## 实际项目

这个驱动从 [SD Card OVID Player](https://github.com/akasa828/SD_Card_OVID_Player) 中拆分而来，
在该 STM32F103 OLED 播放器中负责 SD 卡访问、FatFs 文件浏览、容量扫描和拔卡恢复。

项目自有代码采用 [MIT License](LICENSE)；FatFs、STM32 HAL 与 CMSIS 保留原许可证，详见[第三方说明](THIRD_PARTY_LICENSES.md)。
