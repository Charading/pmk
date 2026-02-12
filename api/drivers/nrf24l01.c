/**
 * nrf24l01.c - NRF24L01 driver for Pico
 * 
 * Self-contained driver using SPI1 (dedicated bus, separate from ADC on SPI0)
 */
#include "nrf24l01.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// NRF24L01 register commands
#define REG_RD_CMD       0x00
#define REG_WR_CMD       0x20
#define W_TX_PAYLOAD_CMD 0xA0

// NRF24L01 registers
#define REG_CONFIG       0x00
#define REG_EN_AA        0x01
#define REG_EN_RXADDR    0x02
#define REG_SETUP_AW     0x03
#define REG_SETUP_RETR   0x04
#define REG_RF_CH        0x05
#define REG_RF_SETUP     0x06
#define REG_STATUS       0x07
#define REG_RX_ADDR_P0   0x0A
#define REG_TX_ADDR      0x10
#define REG_RX_PW_P0     0x11
#define REG_FIFO_STATUS  0x17

// NRF24L01 uses dedicated SPI1 bus (separate from ADC on SPI0)
#define NRF_SPI_PORT spi1
#define NRF_PIN_SCK  10
#define NRF_PIN_MOSI 11
#define NRF_PIN_MISO 12
#define NRF_PIN_CSN  13
#define NRF_PIN_CE   14
#define NRF_PIN_IRQ  15

// Shared address for TX/RX communication
const uint8_t NRF_ADDRESS[5] = {'K', 'E', 'Y', '0', '1'};

// Local state
static bool g_is_tx_mode = false;

// ============================================================================
// Low-level helpers
// ============================================================================

static inline void nrf_csn(bool high) {
    gpio_put(NRF_PIN_CSN, high);
}

static inline void nrf_ce(bool high) {
    gpio_put(NRF_PIN_CE, high);
}

static void nrf_rd_reg(uint8_t reg, uint8_t *data, int len) {
    uint8_t cmd = REG_RD_CMD | reg;
    nrf_csn(false);
    spi_write_blocking(NRF_SPI_PORT, &cmd, 1);
    spi_read_blocking(NRF_SPI_PORT, 0xFF, data, len);
    nrf_csn(true);
}

static void nrf_wr_reg(uint8_t reg, const uint8_t *data, int len) {
    uint8_t cmd = REG_WR_CMD | reg;
    nrf_csn(false);
    spi_write_blocking(NRF_SPI_PORT, &cmd, 1);
    spi_write_blocking(NRF_SPI_PORT, data, len);
    nrf_csn(true);
}

static void nrf_cmd(uint8_t cmd) {
    nrf_csn(false);
    spi_write_blocking(NRF_SPI_PORT, &cmd, 1);
    nrf_csn(true);
}

// ============================================================================
// Public API
// ============================================================================

void nrf24_init(void) {
    // Initialize SPI1 for NRF24L01 (dedicated bus)
    spi_init(NRF_SPI_PORT, 1000000); // 1 MHz
    spi_set_format(NRF_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(NRF_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(NRF_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(NRF_PIN_SCK, GPIO_FUNC_SPI);
    
    // CSN pin (active low chip select)
    gpio_init(NRF_PIN_CSN);
    gpio_set_dir(NRF_PIN_CSN, GPIO_OUT);
    gpio_put(NRF_PIN_CSN, 1);
    
    // CE pin (chip enable for TX/RX)
    gpio_init(NRF_PIN_CE);
    gpio_set_dir(NRF_PIN_CE, GPIO_OUT);
    gpio_put(NRF_PIN_CE, 0);
    
    // IRQ pin (optional)
    gpio_init(NRF_PIN_IRQ);
    gpio_set_dir(NRF_PIN_IRQ, GPIO_IN);
    
    sleep_ms(100); // Power-on delay
    
    printf("NRF24: SPI1 pins: SCK=%d, MOSI=%d, MISO=%d, CSN=%d, CE=%d\n",
           NRF_PIN_SCK, NRF_PIN_MOSI, NRF_PIN_MISO, NRF_PIN_CSN, NRF_PIN_CE);
    
    // Verify NRF24L01 is responding
    uint8_t config;
    nrf_rd_reg(REG_CONFIG, &config, 1);
    printf("NRF24: CONFIG = 0x%02X (expect 0x08 on fresh module)\n", config);
    
    // Write/read test
    uint8_t test_aw = 0x03;
    nrf_wr_reg(REG_SETUP_AW, &test_aw, 1);
    uint8_t read_aw;
    nrf_rd_reg(REG_SETUP_AW, &read_aw, 1);
    printf("NRF24: SETUP_AW write 0x03, read 0x%02X %s\n", 
           read_aw, (read_aw == 0x03) ? "(OK)" : "(FAIL!)");
    
    if (read_aw != 0x03) {
        printf("*** NRF24L01 NOT RESPONDING - CHECK WIRING! ***\n");
    }
}

void nrf24_init_tx(void) {
    g_is_tx_mode = true;
    nrf_ce(false);
    
    // Flush FIFOs
    nrf_cmd(0xE1); // FLUSH_TX
    nrf_cmd(0xE2); // FLUSH_RX
    
    // Clear status flags
    uint8_t val = 0x70;
    nrf_wr_reg(REG_STATUS, &val, 1);
    
    // Disable auto-ack (compatibility with simple PRX setups)
    val = 0x00;
    nrf_wr_reg(REG_EN_AA, &val, 1);

    // Disable retransmit
    val = 0x00;
    nrf_wr_reg(REG_SETUP_RETR, &val, 1);
    
    // Set RF channel
    val = NRF_CHANNEL;
    nrf_wr_reg(REG_RF_CH, &val, 1);
    
    // Set 5-byte address width
    val = 0x03;
    nrf_wr_reg(REG_SETUP_AW, &val, 1);
    
    // Set TX address
    nrf_wr_reg(REG_TX_ADDR, NRF_ADDRESS, 5);
    
    // Set RX_ADDR_P0 same as TX
    nrf_wr_reg(REG_RX_ADDR_P0, NRF_ADDRESS, 5);
    
    // Set payload width
    val = NRF_PAYLOAD_SIZE;
    nrf_wr_reg(REG_RX_PW_P0, &val, 1);
    
    // Enable RX pipe 0
    val = 0x01;
    nrf_wr_reg(REG_EN_RXADDR, &val, 1);
    
    // RF setup: 1Mbps, 0dBm
    val = 0x06;
    nrf_wr_reg(REG_RF_SETUP, &val, 1);
    
    // Power up, TX mode, CRC enabled
    val = 0x0E;
    nrf_wr_reg(REG_CONFIG, &val, 1);
    sleep_ms(5);
    
    // Verify
    uint8_t ch, cfg;
    nrf_rd_reg(REG_RF_CH, &ch, 1);
    nrf_rd_reg(REG_CONFIG, &cfg, 1);
    printf("NRF24: TX mode, ch=%d (read: ch=%d, cfg=0x%02X)\n", NRF_CHANNEL, ch, cfg);
}

void nrf24_init_rx(void) {
    g_is_tx_mode = false;
    nrf_ce(false);
    
    // Flush FIFOs
    nrf_cmd(0xE1); // FLUSH_TX
    nrf_cmd(0xE2); // FLUSH_RX
    
    // Clear status flags
    uint8_t val = 0x70;
    nrf_wr_reg(REG_STATUS, &val, 1);
    
    // Disable auto-ack
    val = 0x00;
    nrf_wr_reg(REG_EN_AA, &val, 1);
    
    // Set RF channel
    val = NRF_CHANNEL;
    nrf_wr_reg(REG_RF_CH, &val, 1);
    
    // Set 5-byte address width
    val = 0x03;
    nrf_wr_reg(REG_SETUP_AW, &val, 1);
    
    // Set RX address pipe 0
    nrf_wr_reg(REG_RX_ADDR_P0, NRF_ADDRESS, 5);
    
    // Set payload width
    val = NRF_PAYLOAD_SIZE;
    nrf_wr_reg(REG_RX_PW_P0, &val, 1);
    
    // Enable RX pipe 0
    val = 0x01;
    nrf_wr_reg(REG_EN_RXADDR, &val, 1);
    
    // RF setup: 1Mbps, 0dBm
    val = 0x06;
    nrf_wr_reg(REG_RF_SETUP, &val, 1);
    
    // Power up, RX mode, CRC enabled (0x0F)
    val = 0x0F;
    nrf_wr_reg(REG_CONFIG, &val, 1);
    sleep_ms(5);
    
    // Start listening
    nrf_ce(true);
    sleep_us(130);
    
    // Verify - and warn if not working
    uint8_t ch, cfg;
    nrf_rd_reg(REG_RF_CH, &ch, 1);
    nrf_rd_reg(REG_CONFIG, &cfg, 1);
    printf("NRF24: RX mode, ch=%d (read: ch=%d, cfg=0x%02X)\n", NRF_CHANNEL, ch, cfg);
    
    if (cfg != 0x0F) {
        printf("*** WARNING: CONFIG should be 0x0F, got 0x%02X - module not responding! ***\n", cfg);
    }
    if (ch != NRF_CHANNEL) {
        printf("*** WARNING: CHANNEL should be %d, got %d - check SPI wiring! ***\n", NRF_CHANNEL, ch);
    }
}

bool nrf24_send(const uint8_t *data, uint8_t len) {
    nrf_ce(false);
    
    // Flush TX
    nrf_cmd(0xE1);
    
    // Clear IRQ flags (RX_DR, TX_DS, MAX_RT)
    uint8_t val = 0x70;
    nrf_wr_reg(REG_STATUS, &val, 1);
    
    // Write payload
    nrf_csn(false);
    uint8_t cmd = W_TX_PAYLOAD_CMD;
    spi_write_blocking(NRF_SPI_PORT, &cmd, 1);
    spi_write_blocking(NRF_SPI_PORT, data, len);
    nrf_csn(true);
    
    // Pulse CE to transmit
    nrf_ce(true);
    sleep_us(15);
    nrf_ce(false);
    
    // Wait for TX_DS (success). With auto-ack disabled, this asserts after send.
    uint8_t status;
    for (int i = 0; i < 10; i++) {
        nrf_rd_reg(REG_STATUS, &status, 1);
        if (status & 0x20) {
            val = 0x20;
            nrf_wr_reg(REG_STATUS, &val, 1);
            return true;
        }
        sleep_ms(1);
    }
    
    return false;
}

bool nrf24_available(void) {
    uint8_t status;
    nrf_rd_reg(REG_STATUS, &status, 1);
    
    // ONLY check RX_DR flag (bit 6) - actual data received interrupt
    // Do NOT check FIFO as it can give false positives
    if (status & 0x40) {
        return true;
    }
    
    return false;
}

void nrf24_read(uint8_t *data, uint8_t len) {
    nrf_csn(false);
    uint8_t cmd = 0x61; // R_RX_PAYLOAD
    spi_write_blocking(NRF_SPI_PORT, &cmd, 1);
    spi_read_blocking(NRF_SPI_PORT, 0xFF, data, len);
    nrf_csn(true);
    
    // Clear RX_DR flag
    uint8_t val = 0x40;
    nrf_wr_reg(REG_STATUS, &val, 1);
}

uint8_t nrf24_get_status(void) {
    uint8_t status;
    nrf_rd_reg(REG_STATUS, &status, 1);
    return status;
}

void nrf24_clear_flags(void) {
    uint8_t val = 0x70;
    nrf_wr_reg(REG_STATUS, &val, 1);
}
