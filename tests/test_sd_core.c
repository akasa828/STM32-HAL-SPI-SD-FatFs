#include <stdio.h>
#include <string.h>

#include "SD_reader.h"
#include "ff.h"
#include "diskio.h"
#include "sd_fatfs.h"

#define MAX_BYTE_STEPS 4096U
#define MAX_PACKETS 16U
#define ANY_BYTE 0x100U

typedef struct {
    uint16_t expected_tx;
    uint8_t rx;
} ByteStep;

typedef struct {
    uint8_t data[SD_BLOCK_SIZE];
    uint16_t length;
    int result;
} PacketStep;

typedef struct {
    ByteStep byte_steps[MAX_BYTE_STEPS];
    size_t byte_count;
    size_t byte_pos;
    PacketStep receive_steps[MAX_PACKETS];
    size_t receive_count;
    size_t receive_pos;
    PacketStep send_steps[MAX_PACKETS];
    size_t send_count;
    size_t send_pos;
    uint32_t tick;
    uint32_t sck_hz;
    uint8_t speed;
    unsigned speed_calls;
    int fail_speed;
    unsigned fail_speed_call;
    unsigned cs_low_count;
    unsigned cs_high_count;
    unsigned enter_count;
    unsigned exit_count;
    int script_failed;
} FakeIO;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static void fake_reset(FakeIO *fake)
{
    (void)memset(fake, 0, sizeof(*fake));
    fake->sck_hz = 9000000U;
    fake->speed = SD_SPI_SPEED_HIGH;
}

static void add_byte(FakeIO *fake, uint16_t expected_tx, uint8_t rx)
{
    if (fake->byte_count >= MAX_BYTE_STEPS) {
        fake->script_failed = 1;
        return;
    }
    fake->byte_steps[fake->byte_count++] = (ByteStep){expected_tx, rx};
}

static void add_repeat(FakeIO *fake, uint16_t expected_tx, uint8_t rx, size_t count)
{
    for (size_t i = 0; i < count; ++i) add_byte(fake, expected_tx, rx);
}

static uint8_t expected_command_crc(uint8_t command, uint32_t argument, uint8_t fallback)
{
#if SD_ENABLE_CMD_CRC
    (void)fallback;
    uint8_t frame[5] = {
        (uint8_t)(0x40U | command),
        (uint8_t)(argument >> 24),
        (uint8_t)(argument >> 16),
        (uint8_t)(argument >> 8),
        (uint8_t)argument,
    };
    uint8_t crc = 0U;
    for (size_t i = 0; i < sizeof(frame); ++i) {
        crc ^= frame[i];
        for (uint8_t bit = 0; bit < 8U; ++bit)
            crc = (crc & 0x80U)
                ? (uint8_t)((crc << 1) ^ 0x12U)
                : (uint8_t)(crc << 1);
    }
    return (uint8_t)(crc | 0x01U);
#else
    (void)command;
    (void)argument;
    return fallback;
#endif
}

static void add_command(FakeIO *fake,
                        uint8_t command,
                        uint32_t argument,
                        uint8_t crc,
                        uint8_t r1)
{
    add_byte(fake, 0xFFU, 0xFFU);
    add_byte(fake, (uint8_t)(0x40U | command), 0xFFU);
    add_byte(fake, (uint8_t)(argument >> 24), 0xFFU);
    add_byte(fake, (uint8_t)(argument >> 16), 0xFFU);
    add_byte(fake, (uint8_t)(argument >> 8), 0xFFU);
    add_byte(fake, (uint8_t)argument, 0xFFU);
    add_byte(fake, expected_command_crc(command, argument, crc), 0xFFU);
    if (command == SD_CMD12) add_byte(fake, 0xFFU, 0xA5U);
    add_byte(fake, 0xFFU, r1);
}

static void add_app_command(FakeIO *fake,
                            uint8_t command,
                            uint32_t argument,
                            uint8_t cmd55_r1,
                            uint8_t app_r1)
{
    add_command(fake, SD_CMD55, 0U, 0x01U, cmd55_r1);
    add_command(fake, command, argument, 0x01U, app_r1);
}

static void add_receive(FakeIO *fake, const uint8_t *data, uint16_t length, int result)
{
    if (fake->receive_count >= MAX_PACKETS || length > SD_BLOCK_SIZE) {
        fake->script_failed = 1;
        return;
    }
    PacketStep *step = &fake->receive_steps[fake->receive_count++];
    (void)memcpy(step->data, data, length);
    step->length = length;
    step->result = result;
}

static void add_send(FakeIO *fake, const uint8_t *data, uint16_t length, int result)
{
    if (fake->send_count >= MAX_PACKETS || length > SD_BLOCK_SIZE) {
        fake->script_failed = 1;
        return;
    }
    PacketStep *step = &fake->send_steps[fake->send_count++];
    (void)memcpy(step->data, data, length);
    step->length = length;
    step->result = result;
}

static uint8_t fake_spi_byte(void *context, uint8_t tx)
{
    FakeIO *fake = context;
    if (fake->byte_pos >= fake->byte_count) {
        fake->script_failed = 1;
        return 0xFFU;
    }
    ByteStep step = fake->byte_steps[fake->byte_pos++];
    if (step.expected_tx != ANY_BYTE && step.expected_tx != tx)
        fake->script_failed = 1;
    return step.rx;
}

static int fake_receive(void *context, uint8_t *data, uint16_t length)
{
    FakeIO *fake = context;
    if (fake->receive_pos >= fake->receive_count) {
        fake->script_failed = 1;
        return SD_ERR;
    }
    const PacketStep *step = &fake->receive_steps[fake->receive_pos++];
    if (step->length != length) {
        fake->script_failed = 1;
        return SD_ERR;
    }
    if (step->result == SD_OK) (void)memcpy(data, step->data, length);
    return step->result;
}

static int fake_send(void *context, const uint8_t *data, uint16_t length)
{
    FakeIO *fake = context;
    if (fake->send_pos >= fake->send_count) {
        fake->script_failed = 1;
        return SD_ERR;
    }
    const PacketStep *step = &fake->send_steps[fake->send_pos++];
    if (step->length != length || memcmp(step->data, data, length) != 0) {
        fake->script_failed = 1;
        return SD_ERR;
    }
    return step->result;
}

static void fake_cs_low(void *context) { ++((FakeIO *)context)->cs_low_count; }
static void fake_cs_high(void *context) { ++((FakeIO *)context)->cs_high_count; }

static int fake_set_speed(void *context, uint8_t speed)
{
    FakeIO *fake = context;
    ++fake->speed_calls;
    if (fake->fail_speed ||
        (fake->fail_speed_call != 0U && fake->speed_calls == fake->fail_speed_call))
        return SD_ERR;
    fake->speed = speed;
    fake->sck_hz = (speed == SD_SPI_SPEED_LOW) ? 281250U : 9000000U;
    return SD_OK;
}

static uint32_t fake_get_sck(void *context) { return ((FakeIO *)context)->sck_hz; }
static uint32_t fake_tick(void *context) { return ((FakeIO *)context)->tick++; }
static uint32_t fake_enter(void *context) { ++((FakeIO *)context)->enter_count; return 0U; }
static void fake_exit(void *context, uint32_t state)
{ (void)state; ++((FakeIO *)context)->exit_count; }

static SD_IO make_io(FakeIO *fake)
{
    SD_IO io = {
        .context = fake,
        .spi_byte = fake_spi_byte,
        .receive = fake_receive,
        .send = fake_send,
        .cs_low = fake_cs_low,
        .cs_high = fake_cs_high,
        .set_speed = fake_set_speed,
        .get_sck_hz = fake_get_sck,
        .tick_ms = fake_tick,
        .deinit = NULL,
        .enter_critical = fake_enter,
        .exit_critical = fake_exit,
        .crc_check = 1U,
    };
    return io;
}

static int bind_ready_card(SD_Card *card, FakeIO *fake)
{
    fake_reset(fake);
    (void)memset(card, 0, sizeof(*card));
    SD_IO io = make_io(fake);
    if (SD_Card_BindIO(card, &io) != SD_OK) return 1;
    card->info.initialized = 1U;
    card->info.type = SD_TYPE_V2HC;
    card->info.block_addr = 1U;
    card->info.block_count = 64U;
    card->info.speed = SD_SPI_SPEED_HIGH;
    return 0;
}

static int script_finished(const FakeIO *fake)
{
    return !fake->script_failed && fake->byte_pos == fake->byte_count &&
           fake->receive_pos == fake->receive_count &&
           fake->send_pos == fake->send_count;
}

static void add_data_packet(FakeIO *fake, const uint8_t *data, uint16_t length, uint16_t crc)
{
    add_byte(fake, 0xFFU, SD_TOKEN_START_BLOCK);
    add_receive(fake, data, length, SD_OK);
    add_byte(fake, 0xFFU, (uint8_t)(crc >> 8));
    add_byte(fake, 0xFFU, (uint8_t)crc);
}

static int test_crc_and_status(void)
{
    static const uint8_t text[] = "123456789";
    CHECK(SD_CRC16(text, 9U) == 0x31C3U);
    CHECK(SD_CRC16(NULL, 1U) == 0U);
    char ok[3];
    CHECK(strcmp(SD_Decode_Status(0U, ok, sizeof(ok)), "OK") == 0);
    char flags[32];
    CHECK(strcmp(SD_Decode_Status(SD_R2_ERROR | SD_R2_CARD_LOCKED,
                                  flags, sizeof(flags)),
                 "ERROR|CARD_LOCKED") == 0);
    char short_buffer[2] = {'x', 'x'};
    CHECK(strcmp(SD_Decode_Status(0U, short_buffer, sizeof(short_buffer)), "") == 0);
    char all_flags[160];
    CHECK(strcmp(SD_Decode_Status(0x7FFFU, all_flags, sizeof(all_flags)),
                 "PARAM_ERR|ADDR_ERR|ERASE_SEQ_ERR|COM_CRC_ERR|ILLEGAL_CMD|"
                 "ERASE_RESET|IDLE|OUT_OF_RANGE|ERASE_PARAM|WP_VIOLATION|"
                 "ECC_FAIL|CC_ERR|ERROR|WP_ERASE_SKIP|CARD_LOCKED") == 0);
    return 0;
}

static int test_binding_and_speed_validation(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    io.exit_critical = NULL;
    CHECK(SD_Card_BindIO(&card, &io) == SD_PARAM_ERR);
    io.exit_critical = fake_exit;
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    card.info.speed = 99U;
    CHECK(SD_Set_Speed_Card(&card, 99U) == SD_PARAM_ERR);
    CHECK(fake.speed_calls == 0U);
    return 0;
}

static int test_block_range_validation(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE] = {0};
    CHECK(bind_ready_card(&card, &fake) == 0);
    CHECK(SD_Read_Block_Card(&card, 64U, data) == SD_PARAM_ERR);
    CHECK(SD_Read_Multi_Block_Card(&card, 63U, data, 2U) == SD_PARAM_ERR);
    CHECK(SD_Write_Block_Card(&card, 64U, data) == SD_PARAM_ERR);
    CHECK(SD_Erase_Blocks_Card(&card, 0U, 64U) == SD_PARAM_ERR);
    CHECK(fake.cs_low_count == 0U);
    return 0;
}

static int test_transaction_guards(void)
{
    FakeIO fake;
    SD_Card card;
    CHECK(bind_ready_card(&card, &fake) == 0);
    card.busy = 1U;
    CHECK(SD_Set_Speed_Card(&card, SD_SPI_SPEED_LOW) == SD_BUSY);
    SD_DeInit_Card(&card);
    CHECK(card.info.initialized == 1U);
    card.busy = 0U;
    CHECK(SD_Set_Speed_Card(&card, SD_SPI_SPEED_LOW) == SD_OK);
    CHECK(card.info.speed == SD_SPI_SPEED_LOW);
    SD_DeInit_Card(&card);
    CHECK(card.info.initialized == 0U);
    CHECK(card.busy == 0U);
    return 0;
}

static int test_single_read(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t expected[SD_BLOCK_SIZE];
    uint8_t actual[SD_BLOCK_SIZE] = {0};
    for (size_t i = 0; i < sizeof(expected); ++i) expected[i] = (uint8_t)i;
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_command(&fake, SD_CMD17, 3U, 0x01U, 0x00U);
    add_data_packet(&fake, expected, sizeof(expected), SD_CRC16(expected, sizeof(expected)));
    CHECK(SD_Read_Block_Card(&card, 3U, actual) == SD_OK);
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_crc_retry_and_speed_restore(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE];
    uint8_t actual[SD_BLOCK_SIZE];
    (void)memset(data, 0x5AU, sizeof(data));
    CHECK(bind_ready_card(&card, &fake) == 0);
    uint16_t bad_crc = (uint16_t)(SD_CRC16(data, sizeof(data)) ^ 0xFFFFU);
    for (unsigned i = 0; i <= SD_CRC_RETRY_MAX; ++i) {
        add_command(&fake, SD_CMD17, 4U, 0x01U, 0x00U);
        add_data_packet(&fake, data, sizeof(data), bad_crc);
    }
    CHECK(SD_Read_Block_Card(&card, 4U, actual) == SD_CRC_ERR);
    CHECK(fake.speed_calls == 2U);
    CHECK(card.info.speed == SD_SPI_SPEED_HIGH);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_multi_read_stop(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE * 2U];
    uint8_t actual[SD_BLOCK_SIZE * 2U] = {0};
    (void)memset(data, 0x11, SD_BLOCK_SIZE);
    (void)memset(data + SD_BLOCK_SIZE, 0x22, SD_BLOCK_SIZE);
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_command(&fake, SD_CMD18, 2U, 0x01U, 0x00U);
    add_data_packet(&fake, data, SD_BLOCK_SIZE, SD_CRC16(data, SD_BLOCK_SIZE));
    add_data_packet(&fake, data + SD_BLOCK_SIZE, SD_BLOCK_SIZE,
                    SD_CRC16(data + SD_BLOCK_SIZE, SD_BLOCK_SIZE));
    add_command(&fake, SD_CMD12, 0U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0xFFU);
    CHECK(SD_Read_Multi_Block_Card(&card, 2U, actual, 2U) == SD_OK);
    CHECK(memcmp(actual, data, sizeof(actual)) == 0);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_multi_read_stop_error(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE] = {0};
    uint8_t actual[SD_BLOCK_SIZE];
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_command(&fake, SD_CMD18, 1U, 0x01U, 0x00U);
    add_data_packet(&fake, data, sizeof(data), SD_CRC16(data, sizeof(data)));
    add_command(&fake, SD_CMD12, 0U, 0x01U, SD_R1_ILLEGAL_CMD);
    CHECK(SD_Read_Multi_Block_Card(&card, 1U, actual, 1U) == SD_ERR);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_single_write(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE];
    (void)memset(data, 0xA5, sizeof(data));
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_command(&fake, SD_CMD24, 5U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0xFFU);
    add_byte(&fake, SD_TOKEN_START_BLOCK, 0xFFU);
    add_send(&fake, data, sizeof(data), SD_OK);
    uint16_t crc = SD_CRC16(data, sizeof(data));
    add_byte(&fake, (uint8_t)(crc >> 8), 0xFFU);
    add_byte(&fake, (uint8_t)crc, 0xFFU);
    add_byte(&fake, 0xFFU, SD_DATA_RESP_ACCEPTED);
    add_byte(&fake, 0xFFU, 0xFFU);
    CHECK(SD_Write_Block_Card(&card, 5U, data) == SD_OK);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_single_write_transport_error(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE] = {0};
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_command(&fake, SD_CMD24, 6U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0xFFU);
    add_byte(&fake, SD_TOKEN_START_BLOCK, 0xFFU);
    add_send(&fake, data, sizeof(data), SD_TIMEOUT);
    CHECK(SD_Write_Block_Card(&card, 6U, data) == SD_TIMEOUT);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_multi_write_finish_timeout(void)
{
    FakeIO fake;
    SD_Card card;
    uint8_t data[SD_BLOCK_SIZE] = {0};
    CHECK(bind_ready_card(&card, &fake) == 0);
    add_app_command(&fake, SD_ACMD23, 1U, 0x00U, 0x00U);
    add_command(&fake, SD_CMD25, 7U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0xFFU);
    add_byte(&fake, SD_TOKEN_START_MULTI, 0xFFU);
    add_send(&fake, data, sizeof(data), SD_OK);
    uint16_t crc = SD_CRC16(data, sizeof(data));
    add_byte(&fake, (uint8_t)(crc >> 8), 0xFFU);
    add_byte(&fake, (uint8_t)crc, 0xFFU);
    add_byte(&fake, 0xFFU, SD_DATA_RESP_ACCEPTED);
    add_repeat(&fake, 0xFFU, 0x00U, 500U);
    CHECK(SD_Write_Multi_Block_Card(&card, 7U, data, 1U) == SD_TIMEOUT);
    CHECK(!fake.script_failed);
    return 0;
}

static void script_v2_init_prefix(FakeIO *fake, uint8_t ocr_first_byte)
{
    add_repeat(fake, 0xFFU, 0xFFU, 10U);
    add_command(fake, SD_CMD0, 0U, 0x95U, SD_R1_IDLE_STATE);
    add_command(fake, SD_CMD8, 0x000001AAU, 0x87U, SD_R1_IDLE_STATE);
    add_byte(fake, 0xFFU, 0x00U);
    add_byte(fake, 0xFFU, 0x00U);
    add_byte(fake, 0xFFU, 0x01U);
    add_byte(fake, 0xFFU, 0xAAU);
#if SD_ENABLE_CMD_CRC
    add_command(fake, SD_CMD59, 1U, 0x01U, 0x00U);
#endif
    add_app_command(fake, SD_ACMD41, 0x40000000U,
                    SD_R1_IDLE_STATE, 0x00U);
    add_command(fake, SD_CMD58, 0U, 0x01U, 0x00U);
    add_byte(fake, 0xFFU, ocr_first_byte);
    add_byte(fake, 0xFFU, 0xFFU);
    add_byte(fake, 0xFFU, 0x80U);
    add_byte(fake, 0xFFU, 0x00U);
}

static void script_v1_init_prefix(FakeIO *fake)
{
    add_repeat(fake, 0xFFU, 0xFFU, 10U);
    add_command(fake, SD_CMD0, 0U, 0x95U, SD_R1_IDLE_STATE);
    add_command(fake, SD_CMD8, 0x000001AAU, 0x87U,
                SD_R1_IDLE_STATE | SD_R1_ILLEGAL_CMD);
#if SD_ENABLE_CMD_CRC
    add_command(fake, SD_CMD59, 1U, 0x01U, 0x00U);
#endif
    add_app_command(fake, SD_ACMD41, 0U, SD_R1_IDLE_STATE, 0x00U);
    add_command(fake, SD_CMD16, SD_BLOCK_SIZE, 0x01U, 0x00U);
}

static int test_full_v2hc_initialization(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v2_init_prefix(&fake, 0xC0U);
    uint8_t csd[16] = {0};
    csd[0] = 0x40U;
    csd[8] = 0x0FU;
    csd[9] = 0xFFU;
    add_command(&fake, SD_CMD9, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, csd, sizeof(csd), SD_CRC16(csd, sizeof(csd)));
    uint8_t cid[16];
    for (size_t i = 0; i < sizeof(cid); ++i) cid[i] = (uint8_t)(0x80U + i);
    add_command(&fake, SD_CMD10, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, cid, sizeof(cid), SD_CRC16(cid, sizeof(cid)));

    CHECK(SD_Init_Card(&card) == SD_TYPE_V2HC);
    CHECK(card.info.initialized == 1U);
    CHECK(card.info.block_addr == 1U);
    CHECK(card.info.block_count == 4194304U);
    CHECK(card.info.capacity_mb == 2048U);
    CHECK(memcmp(card.info.cid_raw, cid, sizeof(cid)) == 0);
    CHECK(card.info.speed == SD_SPI_SPEED_HIGH);
    CHECK(fake.speed_calls == 2U);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_full_v1_initialization_and_byte_addressing(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v1_init_prefix(&fake);

    uint8_t csd[16] = {0};
    csd[5] = 9U;
    csd[6] = 0x03U;
    csd[7] = 0xFFU;
    csd[8] = 0xC0U;
    csd[9] = 0x03U;
    csd[10] = 0x80U;
    add_command(&fake, SD_CMD9, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, csd, sizeof(csd), SD_CRC16(csd, sizeof(csd)));

    uint8_t cid[16] = {0x42U};
    add_command(&fake, SD_CMD10, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, cid, sizeof(cid), SD_CRC16(cid, sizeof(cid)));

    CHECK(SD_Init_Card(&card) == SD_TYPE_V1);
    CHECK(card.info.block_addr == 0U);
    CHECK(card.info.block_count == 2097152U);
    CHECK(card.info.capacity_mb == 1024U);
    CHECK(script_finished(&fake));

    fake_reset(&fake);
    uint8_t expected[SD_BLOCK_SIZE] = {0xA5U};
    uint8_t actual[SD_BLOCK_SIZE] = {0};
    add_command(&fake, SD_CMD17, 3U * SD_BLOCK_SIZE, 0x01U, 0x00U);
    add_data_packet(&fake, expected, sizeof(expected),
                    SD_CRC16(expected, sizeof(expected)));
    CHECK(SD_Read_Block_Card(&card, 3U, actual) == SD_OK);
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_v2_ocr_not_ready(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v2_init_prefix(&fake, 0x40U);
    CHECK(SD_Init_Card(&card) == SD_ERR);
    CHECK(card.info.initialized == 0U);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_init_speed_failure(void)
{
    FakeIO fake;
    fake_reset(&fake);
    fake.fail_speed = 1;
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    CHECK(SD_Init_Card(&card) == SD_ERR);
    CHECK(card.busy == 0U);
    CHECK(fake.byte_count == 0U && fake.byte_pos == 0U);
    return 0;
}

static int test_high_speed_failure_after_initialization(void)
{
    FakeIO fake;
    fake_reset(&fake);
    fake.fail_speed_call = 2U;
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v2_init_prefix(&fake, 0xC0U);

    uint8_t csd[16] = {0x40U};
    add_command(&fake, SD_CMD9, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, csd, sizeof(csd), SD_CRC16(csd, sizeof(csd)));
    uint8_t cid[16] = {0};
    add_command(&fake, SD_CMD10, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, cid, sizeof(cid), SD_CRC16(cid, sizeof(cid)));

    CHECK(SD_Init_Card(&card) == SD_ERR);
    CHECK(card.info.initialized == 0U);
    CHECK(card.info.speed == SD_SPI_SPEED_LOW);
    CHECK(fake.speed_calls == 2U);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_bad_csd_crc(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v2_init_prefix(&fake, 0xC0U);
    uint8_t csd[16] = {0x40U};
    csd[9] = 1U;
    add_command(&fake, SD_CMD9, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, csd, sizeof(csd),
                    (uint16_t)(SD_CRC16(csd, sizeof(csd)) ^ 1U));
    CHECK(SD_Init_Card(&card) == SD_CRC_ERR);
    CHECK(card.info.initialized == 0U);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_invalid_v1_csd_block_length(void)
{
    FakeIO fake;
    fake_reset(&fake);
    SD_Card card = {0};
    SD_IO io = make_io(&fake);
    CHECK(SD_Card_BindIO(&card, &io) == SD_OK);
    script_v1_init_prefix(&fake);
    uint8_t csd[16] = {0};
    csd[5] = 15U;
    add_command(&fake, SD_CMD9, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, csd, sizeof(csd), SD_CRC16(csd, sizeof(csd)));
    CHECK(SD_Init_Card(&card) == SD_ERR);
    CHECK(card.info.initialized == 0U);
    CHECK(card.busy == 0U);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_query_transactions(void)
{
    FakeIO fake;
    SD_Card card;
    uint16_t status = 0xFFFFU;
    uint8_t cid[16] = {0};
    uint8_t scr[8] = {0};
    CHECK(bind_ready_card(&card, &fake) == 0);

    add_command(&fake, SD_CMD13, 0U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0x00U);
    CHECK(SD_Get_Status_Card(&card, &status) == SD_OK);
    CHECK(status == 0U && card.busy == 0U);

    uint8_t expected_cid[16] = {0x11U};
    add_command(&fake, SD_CMD10, 0U, 0x01U, 0x00U);
    add_data_packet(&fake, expected_cid, sizeof(expected_cid),
                    SD_CRC16(expected_cid, sizeof(expected_cid)));
    CHECK(SD_Get_CID_Card(&card, cid) == SD_OK);
    CHECK(memcmp(cid, expected_cid, sizeof(cid)) == 0 && card.busy == 0U);

    uint8_t expected_scr[8] = {0x22U};
    add_app_command(&fake, SD_ACMD51, 0U, 0x00U, 0x00U);
    add_data_packet(&fake, expected_scr, sizeof(expected_scr),
                    SD_CRC16(expected_scr, sizeof(expected_scr)));
    CHECK(SD_Read_SCR_Card(&card, scr) == SD_OK);
    CHECK(memcmp(scr, expected_scr, sizeof(scr)) == 0 && card.busy == 0U);

    add_command(&fake, SD_CMD58, 0U, 0x01U, 0x00U);
    add_byte(&fake, 0xFFU, 0x80U);
    add_byte(&fake, 0xFFU, 0xFFU);
    add_byte(&fake, 0xFFU, 0x80U);
    add_byte(&fake, 0xFFU, 0x00U);
    CHECK(SD_Card_IsPresent_Card(&card) == 1);
    CHECK(card.busy == 0U);
    CHECK(fake.enter_count == fake.exit_count);
    CHECK(script_finished(&fake));
    return 0;
}

static int test_busy_queries_and_diskio_guards(void)
{
    FakeIO fake;
    SD_Card card;
    uint16_t status;
    uint8_t bytes[16];
    CHECK(bind_ready_card(&card, &fake) == 0);
    card.busy = 1U;
    CHECK(SD_Get_Status_Card(&card, &status) == SD_BUSY);
    CHECK(SD_Get_CID_Card(&card, bytes) == SD_BUSY);
    CHECK(SD_Read_SCR_Card(&card, bytes) == SD_BUSY);
    card.busy = 0U;
    CHECK(SD_FatFs_Attach(&card) == SD_OK);
    CHECK(disk_read(0U, NULL, 0U, 1U) == RES_PARERR);
    CHECK(disk_write(0U, NULL, 0U, 1U) == RES_PARERR);
    CHECK(disk_ioctl(0U, GET_SECTOR_COUNT, NULL) == RES_PARERR);
    CHECK(disk_ioctl(0U, CTRL_SYNC, NULL) == RES_OK);
#if FF_LBA64
    CHECK(disk_read(0U, bytes, (LBA_t)UINT32_MAX + 1ULL, 1U) == RES_PARERR);
    CHECK(disk_write(0U, bytes, (LBA_t)UINT32_MAX + 1ULL, 1U) == RES_PARERR);
#endif
    return 0;
}

typedef int (*TestFunction)(void);

int main(void)
{
    static const struct {
        const char *name;
        TestFunction function;
    } tests[] = {
        {"crc_and_status", test_crc_and_status},
        {"binding_and_speed_validation", test_binding_and_speed_validation},
        {"block_range_validation", test_block_range_validation},
        {"transaction_guards", test_transaction_guards},
        {"single_read", test_single_read},
        {"crc_retry_and_speed_restore", test_crc_retry_and_speed_restore},
        {"multi_read_stop", test_multi_read_stop},
        {"multi_read_stop_error", test_multi_read_stop_error},
        {"single_write", test_single_write},
        {"single_write_transport_error", test_single_write_transport_error},
        {"multi_write_finish_timeout", test_multi_write_finish_timeout},
        {"full_v2hc_initialization", test_full_v2hc_initialization},
        {"full_v1_initialization_and_byte_addressing", test_full_v1_initialization_and_byte_addressing},
        {"v2_ocr_not_ready", test_v2_ocr_not_ready},
        {"init_speed_failure", test_init_speed_failure},
        {"high_speed_failure_after_initialization", test_high_speed_failure_after_initialization},
        {"bad_csd_crc", test_bad_csd_crc},
        {"invalid_v1_csd_block_length", test_invalid_v1_csd_block_length},
        {"query_transactions", test_query_transactions},
        {"busy_queries_and_diskio_guards", test_busy_queries_and_diskio_guards},
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].function() != 0) {
            fprintf(stderr, "FAILED: %s\n", tests[i].name);
            return 1;
        }
        printf("PASS: %s\n", tests[i].name);
    }
    return 0;
}
