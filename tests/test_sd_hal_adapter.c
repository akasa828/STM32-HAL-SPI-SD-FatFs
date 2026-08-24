#include <stdio.h>
#include <string.h>

#include "sd_stm32_hal.h"

SPI_TypeDef g_fake_spi1;
SPI_TypeDef g_fake_spi2;

static SD_IO g_bound_io;
static unsigned g_init_calls;
static HAL_SPI_StateTypeDef g_spi_state = HAL_SPI_STATE_READY;

int SD_Card_BindIO(SD_Card *card, const SD_IO *io)
{
    if (card == NULL || io == NULL) return SD_PARAM_ERR;
    g_bound_io = *io;
    return SD_OK;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,
                                          uint8_t *tx,
                                          uint8_t *rx,
                                          uint16_t length,
                                          uint32_t timeout)
{
    (void)spi; (void)tx; (void)rx; (void)length; (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi,
                                  uint8_t *data,
                                  uint16_t length,
                                  uint32_t timeout)
{
    (void)spi; (void)data; (void)length; (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi)
{
    (void)spi;
    ++g_init_calls;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef *spi) { (void)spi; return HAL_OK; }
HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *spi) { (void)spi; return g_spi_state; }
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{ (void)port; (void)pin; (void)state; }
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) { (void)port; (void)init; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 36000000U; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return 72000000U; }
uint32_t HAL_GetTick(void) { return 0U; }
uint32_t __get_PRIMASK(void) { return 0U; }
void __disable_irq(void) { }
void __enable_irq(void) { }

static uint32_t custom_bus_clock(void *context, SPI_HandleTypeDef *spi)
{
    (void)spi;
    return *(const uint32_t *)context;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
    SD_Card card = {0};
    GPIO_TypeDef port = {0};
    SPI_HandleTypeDef spi = {
        .Instance = SPI1,
        .Init = {
            .Mode = SPI_MODE_MASTER,
            .Direction = SPI_DIRECTION_2LINES,
            .DataSize = SPI_DATASIZE_8BIT,
            .CLKPolarity = SPI_POLARITY_LOW,
            .CLKPhase = SPI_PHASE_1EDGE,
            .NSS = SPI_NSS_SOFT,
            .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256,
            .FirstBit = SPI_FIRSTBIT_MSB,
            .TIMode = SPI_TIMODE_DISABLE,
            .CRCCalculation = SPI_CRCCALCULATION_DISABLE,
            .CRCPolynomial = 10U,
        },
    };
    uint32_t bus_clock = 72000000U;
    SD_STM32_HAL adapter = {
        .spi = &spi,
        .cs_port = &port,
        .cs_pin = 1U,
        .low_prescaler = SPI_BAUDRATEPRESCALER_256,
        .high_prescaler = SPI_BAUDRATEPRESCALER_8,
        .bus_clock_hz = custom_bus_clock,
        .user_context = &bus_clock,
    };

    SD_STM32_HAL invalid = adapter;
    invalid.low_prescaler = 0xFFFFFFFFU;
    CHECK(SD_STM32_HAL_Attach(&card, &invalid) == SD_PARAM_ERR);
    SPI_HandleTypeDef invalid_spi = spi;
    invalid.spi = &invalid_spi;
    invalid.low_prescaler = adapter.low_prescaler;
    invalid_spi.Init.DataSize = SPI_DATASIZE_16BIT;
    CHECK(SD_STM32_HAL_Attach(&card, &invalid) == SD_PARAM_ERR);
    invalid_spi = spi;
    invalid_spi.Init.CLKPhase = SPI_PHASE_2EDGE;
    CHECK(SD_STM32_HAL_Attach(&card, &invalid) == SD_PARAM_ERR);
    invalid_spi = spi;
    invalid_spi.Init.TIMode = SPI_TIMODE_ENABLE;
    CHECK(SD_STM32_HAL_Attach(&card, &invalid) == SD_PARAM_ERR);
    invalid_spi = spi;
    invalid_spi.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
    CHECK(SD_STM32_HAL_Attach(&card, &invalid) == SD_PARAM_ERR);
    CHECK(SD_STM32_HAL_Attach(&card, &adapter) == SD_OK);
    CHECK(g_bound_io.get_sck_hz(g_bound_io.context) == 281250U);
    CHECK(g_bound_io.set_speed(g_bound_io.context, 99U) == SD_PARAM_ERR);
    CHECK(g_bound_io.set_speed(g_bound_io.context, SD_SPI_SPEED_HIGH) == SD_OK);
    CHECK(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_8);
    CHECK(g_init_calls == 1U);

    adapter.low_prescaler = 0xFFFFFFFFU;
    CHECK(g_bound_io.set_speed(g_bound_io.context, SD_SPI_SPEED_LOW) == SD_PARAM_ERR);
    CHECK(spi.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_8);
    CHECK(g_init_calls == 1U);
    spi.Init.BaudRatePrescaler = 0xFFFFFFFFU;
    CHECK(g_bound_io.get_sck_hz(g_bound_io.context) == 0U);

    g_spi_state = HAL_SPI_STATE_BUSY;
    CHECK(g_bound_io.set_speed(g_bound_io.context, SD_SPI_SPEED_LOW) == SD_BUSY);
    puts("sd_hal_adapter: ok");
    return 0;
}
