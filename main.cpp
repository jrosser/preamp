#include "mbed.h"
#include "I2C_wrapper.h"
#include "wm8805.h"

AnalogIn ain(p18);

uint8_t AKM_4490_INIT_DATA[11] =
{
  0x87,
  0x82,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00
};

uint8_t AKM_4490_RESET_DATA[11] =
{
  0x86,
  0x82,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00
};

int main() {
    unsigned short adc_value;
    unsigned char dac_vol[2];
    unsigned char dac_vol_old=0;

    I2C_init();

    printf("Reset WM8805\n\r");
    wm8805_reset();

    char buf[2];
    printf("Reset DACs\n\r");
    I2C_write_bytes(0x20, 0, AKM_4490_RESET_DATA, 10);
    I2C_write_bytes(0x22, 0, AKM_4490_RESET_DATA, 10);
    I2C_write_bytes(0x24, 0, AKM_4490_RESET_DATA, 10);

    wait(1);

    printf("Init DACs\n\r");
    I2C_write_bytes(0x20, 0, AKM_4490_INIT_DATA, 10);
    I2C_write_bytes(0x22, 0, AKM_4490_INIT_DATA, 10);
    I2C_write_bytes(0x24, 0, AKM_4490_INIT_DATA, 10);

    printf("Init 8805\n\r");
    wm8805_init();
    wait(1);
    printf("Setup 8805 PLL\n\r");
    //wait(1);
    //wm8805_pll();
    //wait(1);
    //printf("Setup 8805 input\n\r");
    //wm8805_input(0);

    //release output relays
    DigitalOut muteRelay(p21);
    muteRelay = 0;
    
    while (1) {
      I2C_scan_bus();
      //I2C_dump(0x20, 0, 10);
      //I2C_dump(0x22, 0, 10);
      //I2C_dump(0x24, 0, 10);
      //I2C_dump(0X74, 0, 30);
      //wait(1);

      adc_value = ain.read_u16();
      adc_value = adc_value >> 9;  //scale to 0..127
      if(adc_value > 0) adc_value  = adc_value + 128;  //maximum attenuation is 128*0.5 dB

      if(dac_vol_old != adc_value) {
        dac_vol[0] = adc_value;
        dac_vol[1] = adc_value;
        dac_vol_old = adc_value;
        printf("V=0x%02x\n\r", dac_vol[0]);
        I2C_write_bytes(0x20, 3, dac_vol, 2);
        I2C_write_bytes(0x22, 3, dac_vol, 2);
        I2C_write_bytes(0x24, 3, dac_vol, 2);
      }

      wait(0.1);
    }
}
