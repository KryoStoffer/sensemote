#include "common.h"
#include "ledstrip.h"

#define SCLK P1_4

static __xdata uint16_t pixels[MAX_LED];

void ledstrip_init(void) {
	P0DIR = 0xFF;
	P1DIR |= 0x1A; // set SCLK to output;
	init_pixels(Color(0,0,0));
}

uint16_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return( ((((uint8_t)b & 0x1F )<<10) | (((uint8_t)r & 0x1F)<<5)) | ((uint8_t)g & 0x1F));
}

void setPixel(uint16_t n, uint16_t color) {
  if (n < MAX_LED) pixels[n] = color;
}
uint16_t Wheel(uint8_t WheelPos) {
        uint8_t r,g,b;
	r=0;
	g=0;
	b=0;
        switch(WheelPos >> 5) {
                case 0:
                        r=31- WheelPos % 32;   //Red down
                        g=WheelPos % 32;      // Green up
                        b=0;                  //blue off
                        break;
                case 1:
                        g=31- WheelPos % 32;  //green down
                        b=WheelPos % 32;      //blue up
                        r=0;                  //red off
                        break;
                case 2:
                        b=31- WheelPos % 32;  //blue down
                        r=WheelPos % 32;      //red up
                        g=0;                  //green off
                        break;
        }
        return(Color(r,g,b));
}

void clock_out (void) {
	nop();nop();nop();nop();nop();nop();nop();nop();nop();nop();
	nop();nop();nop();nop();
	SCLK=1;
	nop();nop();nop();nop();nop();nop();nop();nop();nop();nop();
	nop();nop();nop();nop();
	SCLK=0;
}

void update_led(void) {
	uint8_t c,j;
	uint16_t mask;

#ifdef CRYSTAL_24_MHZ
	P1_3=0;
	P1_1=0;
#endif
	P0=0; 
	for (c=0;c<32;c++) {clock_out();} // Send init.

#ifdef LED_STRIP_REV
        for (c=(LED_STRIP_LEN-1);c!=0xFF;c--) {
#else
       	for (c=0;c<LED_STRIP_LEN;c++) {
#endif
#ifdef CRYSTAL_24_MHZ
		P1_3=1;
		P1_1=1;
#endif
          	P0=0xFF;clock_out(); // Pixel init.
		mask=0x4000;
		for (j=0;j<15;j++) {
                        if (mask & pixels[c]) P0_0=1; else P0_0=0;
#if LED_STRIP_AMOUNT >= 2
                        if (mask & pixels[c+LED_STRIP_LEN]) P0_1=1; else P0_1=0;
#endif
#if LED_STRIP_AMOUNT >= 3
                        if (mask & pixels[c+LED_STRIP_LEN*2]) P0_2=1; else P0_2=0;
#endif
#if LED_STRIP_AMOUNT >= 4
                        if (mask & pixels[c+LED_STRIP_LEN*3]) P0_3=1; else P0_3=0;
#endif
#if LED_STRIP_AMOUNT >= 5
                        if (mask & pixels[c+LED_STRIP_LEN*4]) P0_4=1; else P0_4=0;
#endif
#if LED_STRIP_AMOUNT >= 6
#ifdef CRYSTAL_24_MHZ
                        if (mask & pixels[c+LED_STRIP_LEN*5]) P1_3=1; else P1_3=0;
#else
                        if (mask & pixels[c+LED_STRIP_LEN*5]) P0_5=1; else P0_5=0;
#endif
#endif
#if LED_STRIP_AMOUNT >= 7
#ifdef CRYSTAL_24_MHZ
                        if (mask & pixels[c+LED_STRIP_LEN*6]) P1_1=1; else P1_1=0;
#else
                        if (mask & pixels[c+LED_STRIP_LEN*6]) P0_6=1; else P0_6=0;
#endif
#endif
#if LED_STRIP_AMOUNT >= 8
                        if (mask & pixels[c+LED_STRIP_LEN*7]) P0_7=1; else P0_7=0;
#endif

			clock_out();
			mask>>=1;
		}
	}

#ifdef CRYSTAL_24_MHZ
	P1_3=0;
	P1_1=0;
#endif
	P0=0; for (c=0;c<16;c++) {clock_out();} // send more clocks in the end, seems to be needed.
}

void init_pixels (uint16_t color) {
	uint8_t c;
	for (c=0; c < MAX_LED ; c++) {
		pixels[c]=color;
	}
	update_led();
}

