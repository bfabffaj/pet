#ifndef OLED_H
#define OLED_H

#include <stdint.h>

/* Call after MX_GPIO_Init() and starting TIM2 at 1 MHz. Returns 1 on success. */
uint8_t OLED_Init(void);
uint8_t OLED_ShowReadings(uint8_t temperature, uint8_t humidity, uint8_t valid);

#endif
