#include "mbed.h"
#include "I2C.h"

I2C i2c(p9, p10);

void I2C_init(void)
{
  i2c.frequency(100000);
}

void I2C_scan_bus(void)
{
  printf("I2C bus scan: ");
  for(int addr=0; addr<0x7f; addr+=2)
  {
    char dev_data[1] = { 1 };
    int status;

    //select register 0, and read it
  	i2c.write(addr, dev_data, 1, true);
  	status = i2c.read(addr, dev_data, 1, false);

    if(status == 0) {
      printf("%02x ", addr);
    }
  }

  printf("\n\r");
}

// Write a single byte to a register
uint8_t I2C_write_byte(int dev_addr, uint8_t int_addr, uint8_t int_data) {
  char dev_data[2];
  uint8_t status;

	dev_data[0] = int_addr;
	dev_data[1] = int_data;

	status = i2c.write(dev_addr, dev_data, 2, false);
	return status;
}

// write multiple bytes starting at a given address
uint8_t I2C_write_bytes(int dev_addr, uint8_t int_addr, uint8_t *int_data, uint8_t count)
{
  uint8_t databuf[100];

  //ugly, but prepend the internal register address to the data payload
  databuf[0] = int_addr;
  for(int i=0; i<count; i++) {
    databuf[i+1] = int_data[i];
  }

  return i2c.write(dev_addr, (const char *)databuf, count+1, false);
}

// Read a single byte from a register
uint8_t I2C_read_byte(int dev_addr, uint8_t int_addr) {
	char dev_data[1] = { int_addr };

	dev_data[0] = int_addr;
	i2c.write(dev_addr, dev_data, 1, true);
	i2c.read(dev_addr, dev_data, 1, false);
	return dev_data[0];
}

void I2C_dump(int dev_addr, uint8_t int_addr, uint8_t count)
{
  char data[100] = { 0 };
  int ret;
  ret = i2c.write(dev_addr, (const char *)&int_addr, 1, true);
  if(ret!=0) {
    printf("i2c dump write failed at address 0x%02x\n\r", dev_addr);
    return;
  }
  i2c.read(dev_addr, data, count);

  printf("I2C(0x%02x:%d) : ", dev_addr, int_addr);
  for(int i=0; i<count; i++)
    printf("%02x ", data[i]);

  printf("\n\r");
}
