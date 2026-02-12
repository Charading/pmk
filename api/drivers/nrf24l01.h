/**
 * nrf24l01.h - Simple NRF24L01 wrapper API
 * 
 * Wraps the nrf_driver library for cleaner usage in the keyboard firmware.
 */
#ifndef NRF24L01_H
#define NRF24L01_H

#include <stdint.h>
#include <stdbool.h>

// Payload size for keyboard reports (modifier + reserved + 6 keys)
#define NRF_PAYLOAD_SIZE 8

// RF channel (both TX and RX must match)
#define NRF_CHANNEL 76

// Address for communication (5 bytes, both TX and RX must match)
extern const uint8_t NRF_ADDRESS[5];

/**
 * Initialize NRF24L01 module with default SPI pins.
 * Call once at startup.
 */
void nrf24_init(void);

/**
 * Configure as transmitter (PTX mode).
 * Call after nrf24_init() on the keyboard side.
 */
void nrf24_init_tx(void);

/**
 * Configure as receiver (PRX mode).
 * Call after nrf24_init() on the receiver side.
 */
void nrf24_init_rx(void);

/**
 * Send a payload (blocking).
 * @param data Pointer to data buffer
 * @param len Length of data (1-32 bytes)
 * @return true if sent successfully, false on error
 */
bool nrf24_send(const uint8_t *data, uint8_t len);

/**
 * Check if data is available in RX FIFO.
 * @return true if data available
 */
bool nrf24_available(void);

/**
 * Read received data from RX FIFO.
 * @param data Buffer to store received data
 * @param len Expected length to read
 */
void nrf24_read(uint8_t *data, uint8_t len);

/**
 * Get current status register value.
 * @return Status register byte
 */
uint8_t nrf24_get_status(void);

/**
 * Clear TX/RX flags in status register.
 */
void nrf24_clear_flags(void);

#endif // NRF24L01_H
