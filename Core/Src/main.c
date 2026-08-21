#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "spi.h"

#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "SD_reader.h"
#include "sd_debug.h"
#include "sd_stm32_hal.h"
#include "sd_fatfs.h"

static FATFS s_fs;
static SD_STM32_HAL s_sd_hal;
volatile uint32_t g_fatal_error_marker;

void SystemClock_Config(void);

static const char *filesystem_name(BYTE type)
{
    switch (type) {
        case FS_FAT12: return "FAT12";
        case FS_FAT16: return "FAT16";
        case FS_FAT32: return "FAT32";
        default: return "Unknown";
    }
}

static void list_root_directory(void)
{
    DIR directory;
    FILINFO info;
    FRESULT result = f_opendir(&directory, "/");
    if (result != FR_OK) {
        printf("[FS] f_opendir failed: %u\n", (unsigned)result);
        return;
    }

    printf("[FS] root directory:\n");
    for (;;) {
        result = f_readdir(&directory, &info);
        if (result != FR_OK || info.fname[0] == '\0') break;
        printf("  %c %10lu  %s\n",
               (info.fattrib & AM_DIR) ? 'D' : 'F',
               (unsigned long)info.fsize, info.fname);
    }
    (void)f_closedir(&directory);
    if (result != FR_OK) printf("[FS] f_readdir failed: %u\n", (unsigned)result);
}

static void filesystem_write_read_demo(void)
{
    static const char payload[] = "STM32F103 FatFs driver is ready.\r\n";
    FIL file;
    UINT transferred = 0U;
    char readback[sizeof(payload)] = {0};

    FRESULT result = f_open(&file, "FS_TEST.TXT", FA_CREATE_ALWAYS | FA_WRITE);
    if (result == FR_OK) {
        result = f_write(&file, payload, sizeof(payload) - 1U, &transferred);
        if (result == FR_OK && transferred == sizeof(payload) - 1U)
            result = f_sync(&file);
        FRESULT close_result = f_close(&file);
        if (result == FR_OK) result = close_result;
    }
    if (result != FR_OK || transferred != sizeof(payload) - 1U) {
        printf("[FS] write test failed: %u, bytes=%u\n",
               (unsigned)result, (unsigned)transferred);
        return;
    }

    transferred = 0U;
    result = f_open(&file, "FS_TEST.TXT", FA_READ);
    if (result == FR_OK) {
        result = f_read(&file, readback, sizeof(payload) - 1U, &transferred);
        FRESULT close_result = f_close(&file);
        if (result == FR_OK) result = close_result;
    }
    if (result == FR_OK && transferred == sizeof(payload) - 1U &&
        memcmp(readback, payload, sizeof(payload) - 1U) == 0)
        printf("[FS] write/read test passed: /FS_TEST.TXT\n");
    else
        printf("[FS] readback test failed: %u, bytes=%u\n",
               (unsigned)result, (unsigned)transferred);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    SD_Debug_UART_Init();

    s_sd_hal.spi = &hspi1;
    s_sd_hal.cs_port = GPIOB;
    s_sd_hal.cs_pin = GPIO_PIN_0;
    s_sd_hal.low_prescaler = SPI_BAUDRATEPRESCALER_256;
    s_sd_hal.high_prescaler = SPI_BAUDRATEPRESCALER_16;
    if (SD_STM32_HAL_Attach(&g_sd_card, &s_sd_hal) != SD_OK ||
        SD_FatFs_Attach(&g_sd_card) != SD_OK) Error_Handler();

    printf("\n[BOOT] STM32F103 SD/FatFs driver demo\n");
    for (;;) {
        SD_DeInit_Card(&g_sd_card);
        MX_SPI1_Init();
        if (SD_STM32_HAL_Attach(&g_sd_card, &s_sd_hal) != SD_OK) Error_Handler();
        printf("[SD] insert a FAT12/FAT16/FAT32 card...\n");
        while (SD_Init_Card(&g_sd_card) <= 0) HAL_Delay(1000U);

        SD_Debug_Print_Info(&g_sd_card);
        FRESULT result = f_mount(&s_fs, "", 1U);
        if (result != FR_OK) {
            printf("[FS] mount failed: %u\n", (unsigned)result);
            SD_DeInit_Card(&g_sd_card);
            HAL_Delay(1000U);
            continue;
        }

        printf("[FS] mounted: %s, cluster=%u sectors\n",
               filesystem_name(s_fs.fs_type), (unsigned)s_fs.csize);
        list_root_directory();
        filesystem_write_read_demo();

        uint8_t misses = 0U;
        while (misses < 2U) {
            misses = SD_Card_IsPresent_Card(&g_sd_card) ? 0U : (uint8_t)(misses + 1U);
            HAL_Delay(500U);
        }
        printf("[SD] card removed\n");
        (void)f_mount(NULL, "", 0U);
        SD_DeInit_Card(&g_sd_card);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    g_fatal_error_marker = 0x45525221UL;
    SD_Debug_UART_Init();
    printf("[FATAL] Error_Handler: 0x%08lX\n", (unsigned long)g_fatal_error_marker);
    __disable_irq();
    for (;;) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    SD_Debug_UART_Init();
    printf("[ASSERT] %s:%lu\n", file, (unsigned long)line);
    Error_Handler();
}
#endif
