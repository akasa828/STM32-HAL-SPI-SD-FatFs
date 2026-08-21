# Porting

[中文](PORTING_ZH.md) · **English**

1. Copy `Core/Micro_SD/SD_reader.c/.h` and, when required, `Core/fatfs/`.
2. Implement every required `SD_IO` callback with the same `context` pointer.
3. Bind the table with `SD_Card_BindIO()` before initialization.
4. If FatFs is used, call `SD_FatFs_Attach()` before mounting drive 0.
5. Verify the initialization clock is at most 400 kHz, then test single and multiple blocks.
6. Test removal/reinsertion and make sure application objects are closed before remounting.

The protocol core has no STM32 header, HAL type, fixed peripheral, fixed GPIO, or UI dependency.
The included HAL adapter is an example of the boundary, not a requirement of the core.
