# 移植说明

**中文** · [English](PORTING.md)

1. 复制 `Core/Micro_SD/SD_reader.c/.h`，需要文件系统时再复制 `Core/fatfs/`。
2. 使用同一个 `context` 实现 `SD_IO` 中的全部必需回调。
3. 初始化卡之前调用 `SD_Card_BindIO()`。
4. 使用 FatFs 时，在挂载物理盘 0 前调用 `SD_FatFs_Attach()`。
5. 确认初始化阶段 SCK 不超过 400 kHz，再验证单块与多块读写。
6. 测试拔卡和重新插卡，重新挂载前必须关闭旧的文件与目录对象。

STM32 HAL 适配层要求主机模式、双线全双工、8 位数据、MSB 优先、Motorola 帧格式、
软件 NSS，并关闭硬件 CRC。配置不兼容时会直接拒绝绑定，避免以错误的总线语义开始事务。

协议核心不包含 STM32 头文件、HAL 类型、固定外设、固定 GPIO 或 UI 依赖。仓库中的 HAL 适配层只是边界实现示例，并不是核心的必需部分。
