# API reference

[中文](API_ZH.md) · **English**

`SD_Card_BindIO(card, io)` validates and copies a context-based hardware table.
`spi_byte`, `receive`, `send`, `cs_low`, `cs_high`, `set_speed`, `get_sck_hz`,
and `tick_ms` are required. Deinitialization and critical-section callbacks are optional.

`SD_Init_Card()` returns a positive `SD_TYPE_*` value on success and a negative
`SD_*` error otherwise. Block operations return `SD_OK` or a negative error code.
`SD_Card_IsPresent_Card()` performs a read-only CMD58/OCR probe on an initialized card.

`SD_FatFs_Attach()` selects the `SD_Card` instance used by FatFs physical drive 0.
Attach before `f_mount()`. The compatibility wrappers in `Core/Port/SD_reader_compat.h`
are optional and intentionally kept outside the platform-neutral core.
