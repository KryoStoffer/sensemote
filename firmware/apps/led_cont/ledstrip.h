#ifndef LEDSTRIP_H
#define LEDSTRIP_H 1

#define MAX_LED 16

uint16_t Color(uint8_t r, uint8_t g, uint8_t b);
uint16_t Wheel(uint8_t WheelPos);
void setPixel(uint16_t n, uint16_t color);
void clock_out (void);
void update_led(void);
void init_pixels(uint16_t color);

#define nop()   __asm nop __endasm;

#endif
