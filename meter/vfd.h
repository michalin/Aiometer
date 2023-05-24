#ifndef VFD_H
#define VFD_H
#include <Arduino.h>
#define RST 26
#define CLK 25
#define I2C85774A 0x38
#define I2C85774 0x38

/**
 * @brief Init VFD tubes
 * @param nTubes: Number of VFD tubes
 * @param addr: I2C address (0x38 for 85774)
 * @param rst_pin
 * @param clk_pin
 **/
void vfd_init(const uint8_t addr=I2C85774A, const uint8_t rst_pin=RST, const uint8_t clk_pin=CLK);
void vfd_set(String);
void vfd_multiplex();

#endif