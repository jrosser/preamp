#ifndef I2C_WRAPPER_H
#define I2C_WRAPPER_H

#include "stdint.h"

void I2C_init(void);
void I2C_scan_bus(void);
uint8_t I2C_write_byte(int dev_addr, uint8_t int_addr, uint8_t int_data);
uint8_t I2C_write_bytes(int dev_addr, uint8_t int_addr, uint8_t *int_data, uint8_t count);
uint8_t I2C_read_byte(int dev_addr, uint8_t int_addr);
void I2C_dump(int dev_addr, uint8_t int_addr, uint8_t count);

#endif
