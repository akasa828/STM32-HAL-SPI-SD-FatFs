# Host tests

These tests exercise the platform-neutral SD protocol core, FatFs `diskio`
validation, and the STM32 HAL adapter without target hardware.

```powershell
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

The scripted SPI backend verifies command frames, data tokens, CRC handling,
multi-block termination, initialization speed, capacity parsing, and range
checks. Hardware timing and signal integrity still require a target-board test.
