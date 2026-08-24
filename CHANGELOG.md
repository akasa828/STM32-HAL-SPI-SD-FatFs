# Changelog

## Unreleased

- Rework the English and Chinese project pages around the portable storage
  stack, runnable demo, minimal integration path, and verified behavior.
- Add an architecture diagram, contribution guide, issue forms, and pull
  request checklist.

## v1.0.2 - 2026-08-25

- Reject CMD58, CMD8, and ACMD41 error responses instead of treating them as
  card presence, legacy-card detection, or initialization progress.
- Publish card metadata only after capacity, identity, and the final high-speed
  switch have all succeeded.
- Reject STM32 HAL TI mode and hardware CRC configurations that change SD SPI
  wire semantics.
- Add deterministic CRC/status boundary coverage and exercise both successful
  and failed self-test restoration paths.
- Add GitHub Actions for host regression tests, static analysis, and
  STM32F103 Debug/Release builds.
- Clarify that the raw-sector self-test restores data only when execution reaches
  the restore step and is not safe against power loss.

## v1.0.1 - 2026-08-23

- Linked the original OVID player as a complete usage example.
- Validate SPI speed modes, initialization clock, OCR readiness, CSD capacity,
  card block ranges, HAL prescalers, and FatFs buffer arguments.
- Check CRC16 for CSD, CID, SCR, and data blocks through one shared packet reader.
- Handle the CMD12 stuff byte and propagate multi-block read/write shutdown errors.
- Serialize status, presence, CID, and SCR queries with all other card transactions.
- Preserve transport-specific write errors and reject runtime-corrupted HAL prescalers.
- Add explicit SPI command gaps for every command and reject unsupported
  STM32 HAL SPI modes before the driver is attached.
- Fail initialization cleanly when the final high-speed switch cannot be applied.
- Refactor initialization, packet transfer, range checking, and status formatting
  into focused helpers with consistent cleanup paths.
- Add host regression tests for initialization, CRC retries, single/multi-block
  transfers, SDSC byte addressing, command CRC, OCR readiness, query locking,
  timeouts, FatFs guards, 64-bit LBA boundaries, command timing, speed-switch
  failures, and the STM32 HAL adapter.

## v1.0.0 - 2026-08-21

- First independent release of the SPI SD + FatFs driver.
- Added a context-based, platform-neutral `SD_IO` interface and configurable STM32 HAL adapter.
- Added attachable FatFs drive integration and a FAT12/FAT16/FAT32 file demo.
- Removed fixed SPI/CS and OLED dependencies from the protocol core.
- Added English and Chinese documentation, CI builds, and third-party notices.
