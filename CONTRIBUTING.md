# Contributing

[中文说明](#中文说明)

Small, focused fixes are easiest to review. Please open an issue before a large
API change so existing integrations are not broken by surprise.

## Before opening a pull request

1. Keep platform code behind `SD_IO`; the protocol core must not include STM32
   HAL headers or assume a fixed peripheral and CS pin.
2. Add or update a host regression test for protocol and adapter changes.
3. Run:

   ```sh
   cmake -S tests -B tests/build
   cmake --build tests/build
   ctest --test-dir tests/build --output-on-failure
   ```

4. Build the STM32F103 example in both Debug and Release when toolchains are
   available.
5. Use a focused commit such as `fix(sd): reject invalid OCR responses`.

Hardware reports should include the MCU, SD module or level shifter, card type
and capacity, SPI clock, supply voltage, wiring length, and the exact return
code or serial log. Do not test raw writes on a card containing important data.

## 中文说明

较小、目的明确的改动更容易检查。准备修改公开 API 时，请先开 Issue 讨论，避免意外破坏
现有工程。

协议和适配层改动应补充主机回归测试，并运行上面的测试命令；条件允许时再构建 Debug 与
Release 示例。硬件问题请写明 MCU、SD 模块或电平转换、卡类型和容量、SPI 速度、供电、
线长，以及准确的返回值或串口日志。不要在存有重要数据的卡上测试原始扇区写入。
