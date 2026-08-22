#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H

#include <stdint.h>

typedef struct SPI_TypeDef { uint32_t marker; } SPI_TypeDef;
typedef struct GPIO_TypeDef { uint32_t marker; } GPIO_TypeDef;

extern SPI_TypeDef g_fake_spi1;
extern SPI_TypeDef g_fake_spi2;

#define SPI1 (&g_fake_spi1)
#define SPI2 (&g_fake_spi2)

#define SPI_BAUDRATEPRESCALER_2   0x00U
#define SPI_BAUDRATEPRESCALER_4   0x08U
#define SPI_BAUDRATEPRESCALER_8   0x10U
#define SPI_BAUDRATEPRESCALER_16  0x18U
#define SPI_BAUDRATEPRESCALER_32  0x20U
#define SPI_BAUDRATEPRESCALER_64  0x28U
#define SPI_BAUDRATEPRESCALER_128 0x30U
#define SPI_BAUDRATEPRESCALER_256 0x38U

#define SPI_MODE_MASTER       0x0004U
#define SPI_MODE_SLAVE        0x0000U
#define SPI_DIRECTION_2LINES  0x0000U
#define SPI_DIRECTION_1LINE   0x8000U
#define SPI_DATASIZE_8BIT     0x0000U
#define SPI_DATASIZE_16BIT    0x0800U
#define SPI_POLARITY_LOW      0x0000U
#define SPI_POLARITY_HIGH     0x0002U
#define SPI_PHASE_1EDGE       0x0000U
#define SPI_PHASE_2EDGE       0x0001U
#define SPI_NSS_SOFT          0x0200U
#define SPI_NSS_HARD_INPUT    0x0000U
#define SPI_FIRSTBIT_MSB      0x0000U
#define SPI_FIRSTBIT_LSB      0x0080U

typedef struct {
    uint32_t Mode;
    uint32_t Direction;
    uint32_t DataSize;
    uint32_t CLKPolarity;
    uint32_t CLKPhase;
    uint32_t NSS;
    uint32_t BaudRatePrescaler;
    uint32_t FirstBit;
} SPI_InitTypeDef;

typedef struct {
    SPI_TypeDef *Instance;
    SPI_InitTypeDef Init;
} SPI_HandleTypeDef;

typedef struct {
    uint16_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef enum { HAL_SPI_STATE_READY = 0, HAL_SPI_STATE_BUSY = 1 } HAL_SPI_StateTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,
                                          uint8_t *tx,
                                          uint8_t *rx,
                                          uint16_t length,
                                          uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi,
                                  uint8_t *data,
                                  uint16_t length,
                                  uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi);
HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef *spi);
HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *spi);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetPCLK2Freq(void);
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);

#endif
