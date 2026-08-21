# API 说明

**中文** · [English](API.md)

`SD_Card_BindIO(card, io)` 会检查并复制带上下文的硬件回调表。`spi_byte`、`receive`、`send`、`cs_low`、`cs_high`、`set_speed`、`get_sck_hz` 和 `tick_ms` 必须提供；反初始化和临界区回调可以按需实现。

`SD_Init_Card()` 成功时返回正数 `SD_TYPE_*`，失败时返回负数 `SD_*` 错误码。块读写返回 `SD_OK` 或负数错误码。`SD_Card_IsPresent_Card()` 对已经初始化的卡执行只读 CMD58/OCR 在线探测。

`SD_FatFs_Attach()` 指定 FatFs 物理盘 0 使用的 `SD_Card`，必须在 `f_mount()` 前调用。旧 HAL 风格包装单独放在 `Core/Port/SD_reader_compat.h`，不会污染平台无关核心。
