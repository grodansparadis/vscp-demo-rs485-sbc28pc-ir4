/*
  hw-stub.h - the simulated hardware of the host tests
*/

#ifndef HW_STUB_H
#define HW_STUB_H

#include <stdint.h>

// Characters the node will receive (character, ninth bit)
void
stub_rx_push(const uint8_t *data, uint8_t len, int8_t first_mark);
// Characters the node sent; returns the count and clears the buffer
uint8_t
stub_tx_take(uint8_t *out, uint8_t cap);
// Advance time by ms milliseconds, calling the 1 ms tick
void
stub_advance_ms(uint16_t ms);
void
stub_set_button(uint8_t pressed);
uint8_t
stub_led(void);
uint8_t
stub_eeprom(uint8_t addr);
void
stub_eeprom_erase(void);
uint16_t
stub_tx_frames(void);

#endif
