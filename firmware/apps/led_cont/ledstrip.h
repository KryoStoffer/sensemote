#ifndef LEDSTRIP_H
#define LEDSTRIP_H 1


#if DEV_ID == 1  // Stairs Down living
#define LED_STRIP_AMOUNT 8
#define LED_STRIP_LEN 8
#define LED_STRIP_REV 1
#elif DEV_ID == 2 // Stairs Top flor
#define LED_STRIP_AMOUNT 7
#define LED_STRIP_LEN 8
#elif DEV_ID == 3 // Living room decoration trees
#define LED_STRIP_AMOUNT 5
#define LED_STRIP_LEN 16
#elif DEV_ID == 4 // Mood lamp
#define LED_STRIP_AMOUNT 1
#define LED_STRIP_LEN 6
#elif DEV_ID == 42 // Test device
#define LED_STRIP_AMOUNT 8
#define LED_STRIP_LEN 8
#define LED_STRIP_REV 1
#else
# error unknow DEV_ID
#endif

#define MAX_LED (LED_STRIP_AMOUNT * LED_STRIP_LEN)

void ledstrip_init(void);
uint16_t Color(uint8_t r, uint8_t g, uint8_t b);
uint16_t Wheel(uint8_t WheelPos);
void setPixel(uint16_t n, uint16_t color);
void clock_out (void);
void update_led(void);
void init_pixels(uint16_t color);

#define nop()   __asm nop __endasm;

#endif
